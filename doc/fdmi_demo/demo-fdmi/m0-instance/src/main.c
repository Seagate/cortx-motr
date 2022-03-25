#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>

#include "fol/fol.h"
#include "stob/ad.h"
#include "stob/ad_private.h"
#include "rpc/rpc_opcodes.h"

#include "dix/layout.h"
#include "dix/client.h"
#include "dix/meta.h"
#include "fid/fid.h"
#include "lib/types.h"
#include "lib/getopts.h"    /* M0_GETOPTS */
#include "lib/thread.h"        /* LAMBDA */
#include "lib/hash_fnc.h"    /* HASH_FNC_CITY */
#include "lib/uuid.h"        /* m0_uuid_generate */
#include "lib/string.h"        /* m0_streq */
#include "lib/ext.h"        /* m0_ext */
#include "lib/vec.h"        /* m0_ext */
#include "module/instance.h"    /* m0 */
#include "pool/pool.h"        /* m0_pool_version */
#include "conf/confc.h"        /* m0_confc_close */
#include "net/lnet/lnet.h"    /* m0_net_lnet_xprt */
#include "mero/ha.h"
#include "rpc/rpc_machine.h"    /* m0_rpc_machine */
#include "rpc/rpc.h"        /* m0_rpc_bufs_nr */
#include "reqh/reqh.h"        /* m0_reqh */
#include "rm/rm_service.h"    /* m0_rms_type */
#include "net/buffer_pool.h"    /* m0_net_buffer_pool */
#include "fdmi/fdmi.h"
#include "fdmi/service.h"
#include "fdmi/plugin_dock.h"
#include "fdmi/plugin_dock_internal.h"
#include <clovis/clovis.h>
#include <clovis/clovis_cont.h>
#include <clovis/clovis_internal.h>

#define MALLOC_ARR(arr, nr)  ((arr) = malloc((nr) * sizeof ((arr)[0])))

// Size threshold, in bytes above, which objects are considered large.
#define THRESHOLD (1024*1024)

// Pipe descriptors for communicating between Clovis and FDMI processes.
static int pipe_fds[2];
#define READ_END pipe_fds[0]
#define WRITE_END pipe_fds[1]

// Clovis <-> FDMI messages.
typedef struct classify_msg {
    bool big;
    struct m0_uint128 obj_id;
} classify_msg_t;

// Plugin FID.
static struct m0_fid CLASSIFY_PLUGIN_FID = {
    .f_container = M0_FDMI_REC_TYPE_FOL,
    .f_key = 0x1
};

// FDMI instance context record type.
struct fdmi_ctx {
    struct m0_pools_common dc_pools_common;
    struct m0_mero_ha dc_mero_ha;
    const char *dc_laddr;
    struct m0_net_domain dc_ndom;
    struct m0_rpc_machine dc_rpc_machine;
    struct m0_reqh dc_reqh;
    struct m0_net_buffer_pool dc_buffer_pool;
};

static uint32_t tm_recv_queue_min_len = 10;
static uint32_t max_rpc_msg_size = M0_RPC_DEF_MAX_RPC_MSG_SIZE;

// m0 instance for FDMI process.
static struct m0 instance;
// FDMI instance context.
static struct fdmi_ctx ctx;

void bufvec_free(struct m0_bufvec *bv)
{
    uint32_t i;
    if (bv == NULL)
        return;

    if (bv->ov_buf != NULL) {
        for (i = 0; i < bv->ov_vec.v_nr; ++i)
            if (bv->ov_buf[i] != NULL)
                free(bv->ov_buf[i]);
        free(bv->ov_buf);
    }
    free(bv->ov_vec.v_count);
    free(bv);
}

struct m0_bufvec *bufvec_alloc(int nr)
{
    struct m0_bufvec *bv;

    bv = m0_alloc(sizeof *bv);
    if (bv == NULL)
        return NULL;

    bv->ov_vec.v_nr = nr;
    MALLOC_ARR(bv->ov_vec.v_count, nr);
    if (bv->ov_vec.v_count == NULL)
        goto FAIL;

    M0_ALLOC_ARR(bv->ov_buf, nr);
    if (bv->ov_buf == NULL)
        goto FAIL;

    return bv;

 FAIL:
    m0_bufvec_free(bv);
    return NULL;
}

static void inspect_fol_record(struct m0_fol_rec *fol_rec)
{
    struct m0_fol_frag *frag;
    int i;

    m0_tlist_for(&m0_rec_frag_tl, &fol_rec->fr_frags, frag) {
        const char *name;
        name = frag->rp_ops->rpo_type->rpt_name;

        if (m0_streq(name, "fop generic record frag")) {
            struct m0_fop_fol_frag *fol_frag =
                (struct m0_fop_fol_frag *)frag->rp_data;

            struct m0_fop_cob_writev *wfop;
            struct m0_fop_cob_rw *c_rwv;
            classify_msg_t classify_msg;
            uint64_t size = 0;

            switch (fol_frag->ffrp_fop_code) {
            case M0_IOSERVICE_WRITEV_OPCODE:
                wfop = fol_frag->ffrp_fop;
                c_rwv = &wfop->c_rwv;

                for (i = 0; i < c_rwv->crw_ivec.ci_nr; i++) {
                    struct m0_ioseg *seg =
                        &c_rwv->crw_ivec.ci_iosegs[i];
                    size += seg->ci_count;
                }

                classify_msg.obj_id.u_lo =
                    c_rwv->crw_gfid.f_container;
                classify_msg.obj_id.u_hi =
                    c_rwv->crw_gfid.f_key;

                classify_msg.big = (size >= THRESHOLD);

                write(WRITE_END, &classify_msg,
                      sizeof(classify_msg_t));
            }
        }
    }
    m0_tlist_endfor;
}

static int inst_ha_init(struct fdmi_ctx *ctx, const char *ha_addr)
{
    struct m0_mero_ha_cfg mero_ha_cfg;
    int rc;

    mero_ha_cfg = (struct m0_mero_ha_cfg) {
        .mhc_addr = ha_addr,.mhc_rpc_machine =
            &ctx->dc_rpc_machine,.mhc_reqh =
            &ctx->dc_reqh,.mhc_dispatcher_cfg = {
    .hdc_enable_note = true,.hdc_enable_keepalive =
                false,.hdc_enable_fvec = true},};
    rc = m0_mero_ha_init(&ctx->dc_mero_ha, &mero_ha_cfg);
    if (rc != 0)
        return rc;
    rc = m0_mero_ha_start(&ctx->dc_mero_ha);
    if (rc != 0) {
        m0_mero_ha_fini(&ctx->dc_mero_ha);
        return rc;
    }
    m0_mero_ha_connect(&ctx->dc_mero_ha);
    return rc;
}

static void inst_ha_stop(struct fdmi_ctx *ctx)
{
    m0_mero_ha_disconnect(&ctx->dc_mero_ha);
    m0_mero_ha_stop(&ctx->dc_mero_ha);
}

static void inst_ha_fini(struct fdmi_ctx *ctx)
{
    m0_mero_ha_fini(&ctx->dc_mero_ha);
}

static int inst_net_init(struct fdmi_ctx *ctx, const char *local_addr)
{
    ctx->dc_laddr = local_addr;
    return m0_net_domain_init(&ctx->dc_ndom, &m0_net_lnet_xprt);
}

static int inst_rpc_init(struct fdmi_ctx *ctx)
{
    struct m0_rpc_machine *rpc_machine = &ctx->dc_rpc_machine;
    struct m0_reqh *reqh = &ctx->dc_reqh;
    struct m0_net_domain *ndom = &ctx->dc_ndom;
    const char *laddr;
    struct m0_net_buffer_pool *buffer_pool = &ctx->dc_buffer_pool;
    struct m0_net_transfer_mc *tm;
    int rc;
    uint32_t bufs_nr;
    uint32_t tms_nr;

    tms_nr = 1;
    bufs_nr = m0_rpc_bufs_nr(tm_recv_queue_min_len, tms_nr);

    rc = m0_rpc_net_buffer_pool_setup(ndom, buffer_pool, bufs_nr, tms_nr);
    if (rc != 0)
        return rc;

    rc = M0_REQH_INIT(reqh,
                      .rhia_dtm = (void *)1,
                      .rhia_db = NULL,
                      .rhia_mdstore = (void *)1,
                      .rhia_pc = &ctx->dc_pools_common,
                      .rhia_fid =
                      &M0_FID_TINIT(M0_CONF_PROCESS_TYPE.cot_ftype.ft_id, 0,0)
                      );
    if (rc != 0)
        goto pool_fini;
    laddr = ctx->dc_laddr;
    rc = m0_rpc_machine_init(rpc_machine, ndom, laddr, reqh,
                 buffer_pool, M0_BUFFER_ANY_COLOUR,
                 max_rpc_msg_size, tm_recv_queue_min_len);
    if (rc != 0)
        goto reqh_fini;
    m0_reqh_start(reqh);
    tm = &rpc_machine->rm_tm;
    M0_ASSERT(tm->ntm_recv_pool == buffer_pool);
    return rc;
 reqh_fini:
    m0_reqh_fini(reqh);
 pool_fini:
    m0_rpc_net_buffer_pool_cleanup(buffer_pool);
    return rc;
}

static void inst_rpc_fini(struct fdmi_ctx *ctx)
{

    m0_rpc_machine_fini(&ctx->dc_rpc_machine);
    if (m0_reqh_state_get(&ctx->dc_reqh) != M0_REQH_ST_STOPPED)
        m0_reqh_services_terminate(&ctx->dc_reqh);
    m0_reqh_fini(&ctx->dc_reqh);
    m0_rpc_net_buffer_pool_cleanup(&ctx->dc_buffer_pool);
}

static void inst_net_fini(struct fdmi_ctx *ctx)
{
    m0_net_domain_fini(&ctx->dc_ndom);
}

M0_INTERNAL struct m0_rconfc *get_rconfc(struct fdmi_ctx *ctx)
{
    return &ctx->dc_reqh.rh_rconfc;
}

static int inst_layouts_init(struct fdmi_ctx *ctx)
{
    int rc;

    rc = m0_reqh_mdpool_layout_build(&ctx->dc_reqh);
    return rc;
}

static void inst_layouts_fini(struct fdmi_ctx *ctx)
{
    m0_reqh_layouts_cleanup(&ctx->dc_reqh);
}

static int inst_service_start(struct m0_reqh_service_type *stype,
                  struct m0_reqh *reqh)
{
    struct m0_reqh_service *service;
    struct m0_uint128 uuid;

    m0_uuid_generate(&uuid);
    m0_fid_tassume((struct m0_fid *)&uuid, &M0_CONF_SERVICE_TYPE.cot_ftype);
    return m0_reqh_service_setup(&service, stype, reqh, NULL,
                     (struct m0_fid *)&uuid);
}

static int inst_reqh_services_start(struct fdmi_ctx *ctx)
{
    struct m0_reqh *reqh = &ctx->dc_reqh;
    int rc;

    rc = inst_service_start(&m0_rms_type, reqh);
    if (rc != 0) {
        /* M0_ERR(rc); */
        m0_reqh_services_terminate(reqh);
    }
    printf("Starting FDMI service.\n");
    rc = inst_service_start(&m0_fdmi_service_type, reqh);
    if (rc != 0) {
        /* M0_ERR(rc); */
        m0_reqh_services_terminate(reqh);
    }

    return rc;
}

static int classify_handle_fdmi_rec_not(struct m0_uint128 *rec_id,
                    struct m0_buf fdmi_rec,
                    struct m0_fid filter_id)
{
    int rc = 0;
    struct m0_fol_rec fol_rec;

    m0_fol_rec_init(&fol_rec, NULL);
    m0_fol_rec_decode(&fol_rec, &fdmi_rec);

    inspect_fol_record(&fol_rec);

    m0_fol_rec_fini(&fol_rec);

    return rc;
}

int init_fdmi_plugin()
{
    int rc;

    const struct m0_fdmi_pd_ops *pdo = m0_fdmi_plugin_dock_api_get();
    const struct m0_fdmi_filter_desc fd;

    const static struct m0_fdmi_plugin_ops pcb = {
        .po_fdmi_rec = classify_handle_fdmi_rec_not
    };

    /* printf("Registering classify plugin."); */
    rc = pdo->fpo_register_filter(&CLASSIFY_PLUGIN_FID, &fd, &pcb);
    printf("Plugin registration rc: %d\n", rc);
    if (rc < 0)
        goto end_fdmi_init;

    /* printf("Classify rc: %d", rc); */
    pdo->fpo_enable_filters(true, &CLASSIFY_PLUGIN_FID, 1);

 end_fdmi_init:
    return rc;
}

static int inst_init(struct fdmi_ctx *ctx,
             const char *local_addr,
             const char *ha_addr, const char *profile)
{
    struct m0_pools_common *pc = &ctx->dc_pools_common;
    struct m0_confc_args *confc_args;
    struct m0_reqh *reqh = &ctx->dc_reqh;
    struct m0_conf_filesystem *fs;
    int rc;

    rc = m0_layout_domain_init(&ctx->dc_reqh.rh_ldom);
    if (rc != 0)
        return rc;
    rc = m0_layout_standard_types_register(&ctx->dc_reqh.rh_ldom);
    if (rc != 0)
        goto err_domain_fini;

    rc = inst_net_init(ctx, local_addr);
    if (rc != 0)
        goto err_domain_fini;

    rc = inst_rpc_init(ctx);
    if (rc != 0)
        goto err_net_fini;

    confc_args = &(struct m0_confc_args) {
    .ca_profile = profile,.ca_rmach =
            &ctx->dc_rpc_machine,.ca_group =
            m0_locality0_get()->lo_grp};

    rc = inst_ha_init(ctx, ha_addr);
    if (rc != 0)
        goto err_ha_fini;

    rc = m0_reqh_conf_setup(reqh, confc_args);
    if (rc != 0)
        goto err_ha_fini;

    rc = m0_rconfc_start_sync(get_rconfc(ctx)) ? :
        m0_ha_client_add(m0_reqh2confc(reqh));
    if (rc != 0)
        goto err_rconfc_stop;

    rc = m0_conf_fs_get(m0_reqh2profile(reqh), m0_reqh2confc(reqh), &fs);
    if (rc != 0)
        goto err_rconfc_stop;

    rc = m0_conf_full_load(fs);
    if (rc != 0)
        goto err_conf_fs_close;

    rc = m0_pools_common_init(pc, &ctx->dc_rpc_machine, fs);
    if (rc != 0)
        goto err_conf_fs_close;

    rc = m0_pools_setup(pc, fs, NULL, NULL, NULL);
    if (rc != 0)
        goto err_pools_common_fini;

    rc = m0_pools_service_ctx_create(pc, fs);
    if (rc != 0)
        goto err_pools_destroy;

    m0_pools_common_service_ctx_connect_sync(pc);

    rc = m0_pool_versions_setup(pc, fs, NULL, NULL, NULL);
    if (rc != 0)
        goto err_pools_service_ctx_destroy;

    rc = inst_reqh_services_start(ctx);
    if (rc != 0)
        goto err_pool_versions_destroy;

    rc = inst_layouts_init(ctx);
    if (rc != 0) {
        inst_layouts_fini(ctx);
        goto err_pool_versions_destroy;
    }

    m0_confc_close(&fs->cf_obj);
    return 0;

 err_pool_versions_destroy:
    m0_pool_versions_destroy(&ctx->dc_pools_common);
 err_pools_service_ctx_destroy:
    m0_pools_service_ctx_destroy(&ctx->dc_pools_common);
 err_pools_destroy:
    m0_pools_destroy(&ctx->dc_pools_common);
 err_pools_common_fini:
    m0_pools_common_fini(&ctx->dc_pools_common);
 err_conf_fs_close:
    m0_confc_close(&fs->cf_obj);
 err_rconfc_stop:
    m0_rconfc_stop_sync(get_rconfc(ctx));
    m0_rconfc_fini(get_rconfc(ctx));
    m0_reqh_services_terminate(reqh);
 err_ha_fini:
    inst_ha_stop(ctx);
    inst_ha_fini(ctx);
    inst_rpc_fini(ctx);
 err_net_fini:
    inst_net_fini(ctx);
 err_domain_fini:
    m0_layout_domain_fini(&ctx->dc_reqh.rh_ldom);
    return rc;
}

static struct m0_clovis *m0c;
struct m0_clovis_realm my_realm;
static uint64_t default_layout;

static struct m0_pool pool;

static struct m0_clovis_container small_cont;
static struct m0_clovis_container large_cont;

int init_containers()
{
    struct m0_clovis_op *ops[2] = { NULL, NULL };
    int rc;
    struct m0_uint128 small_cont_id = M0_CLOVIS_ID_APP;
    struct m0_uint128 large_cont_id = M0_CLOVIS_ID_APP;
    small_cont_id.u_hi = small_cont_id.u_hi + 1;
    large_cont_id.u_hi = large_cont_id.u_hi + 2;

    m0_clovis_container_init(&small_cont, &my_realm, &small_cont_id, m0c,
                 &pool);
    m0_clovis_container_init(&large_cont, &my_realm, &large_cont_id, m0c,
                 &pool);

    m0_clovis_container_create(&small_cont, &ops[0]);
    m0_clovis_container_create(&large_cont, &ops[1]);

    m0_clovis_op_launch(ops, 2);
    return m0_clovis_op_wait(ops[0],
                           M0_BITS(M0_CLOVIS_OS_FAILED, M0_CLOVIS_OS_STABLE),
                           M0_TIME_NEVER);
}

void list_containers(struct m0_clovis_container *cont)
{
    struct m0_bufvec *keys;
    struct m0_bufvec *vals;
}

int fdmi_instance(const char *local_addr, const char *ha_addr, const char *prof)
{
    int rc;

    M0_SET0(&instance);
    m0_instance_setup(&instance);
    rc = m0_module_init(&instance.i_self, M0_LEVEL_INST_READY);
    if (rc != 0) {
        fprintf(stderr, "Cannot init module %i\n", rc);
        goto end;
    }
    M0_SET0(&ctx);

    rc = inst_init(&ctx, local_addr, ha_addr, prof);
    if (rc != 0) {
        fprintf(stderr, "Initialisation error: %d\n", rc);
        goto end;
    }
    // Starting FDMI plugin.
    rc = init_fdmi_plugin();

 end:
    return rc;
}

int main(int argc, char **argv)
{
    int rc;
    char *local_addr = NULL;    // For FDMI  process.
    char *local_addr2 = NULL;    // For Clovis process.
    char *ha_addr = NULL;
    char *prof = NULL;

    /* Retrieving command-line parameters. */
    if (argc < 5)
        return -1;

    local_addr = argv[1];
    ha_addr = argv[2];
    prof = argv[3];
    local_addr2 = argv[4];

    if (local_addr == NULL ||
        local_addr2 == NULL || ha_addr == NULL || prof == NULL) {
        fprintf(stderr, "Invalid parameter(s).\n");
        return EINVAL;
    }

    /* XXX: We need to initialize Mero twice:
       - Once directly, to start the FDMI instance.
       - Once indirectly, as we use Clovis to populate containers.

       However, m0_init can only be called once per process. We work our way
       around this by forking early on and using pipes as an IPC mechanism.
     */

    pipe(pipe_fds);

    if (fork()) {
        rc = fdmi_instance(local_addr, ha_addr, prof);
        if (rc < 0) {
            fprintf(stderr,
                "Failed to initialize FDMI instance: %d\n", rc);
            return rc;
        }
        pause();
    } else {        // Child
        FILE *file = fdopen(READ_END, "r");

        /* Clovis init */
        classify_msg_t msg;

        struct m0_idx_dix_config idx_service_conf = {
            .kc_create_meta = false
        };

        struct m0_fid pver = M0_FID_TINIT('v', 2, 4);
        int rc;
        struct m0_ext range[] = { {.e_start = 0,.e_end = IMASK_INF} };
        rc = m0_dix_ldesc_init(&idx_service_conf.kc_layout_ldesc, range,
                       1, HASH_FNC_FNV1, &pver);
        if (rc < 0)
            return rc;
        rc = m0_dix_ldesc_init(&idx_service_conf.kc_ldescr_ldesc, range,
                       1, HASH_FNC_FNV1, &pver);
        if (rc < 0)
            return rc;

        struct m0_clovis_config cfg = {
            .cc_is_oostore = true,
            .cc_is_read_verify = false,
            .cc_ha_addr = ha_addr,    // IP@tcp:12345:45:1
            .cc_local_addr = local_addr2,    // IP@tcp:12345:44:101
            .cc_profile = prof,    // <0x7000000000000001:0>
            .cc_process_fid = "<0x7200000000000000:0>",
            .cc_tm_recv_queue_min_len =
                M0_NET_TM_RECV_QUEUE_DEF_LEN,
            .cc_max_rpc_msg_size = M0_RPC_DEF_MAX_RPC_MSG_SIZE,
            .cc_idx_service_id = M0_CLOVIS_IDX_DIX,    // DIX
            .cc_idx_service_conf = (void *)&idx_service_conf
        };

        rc = m0_clovis_init(&m0c, &cfg, true);
        if (rc < 0)
            return rc;

        default_layout = m0_clovis_default_layout_id(m0c);

        /* No rc */
        m0_clovis_realm_init(&my_realm,
                     NULL, &M0_CLOVIS_UBER_REALM,
                     M0_CLOVIS_ST_CONTAINER, m0c);

        rc = init_containers();

        while (1) {
            int nels = fread(&msg, sizeof(classify_msg_t), 1, file);

            if (nels < 0) {
                // Read error.
                return -1;
            } else if (nels == 1) {
                // XXX: We seem to receive multiple records with different u_hi for a
                // single write. Hacking our way around this...

                struct m0_uint128 obj_id = msg.obj_id;
                obj_id.u_hi = 0;

                struct m0_clovis_op *ops[1] = { NULL };
                struct m0_clovis_container *target =
                    msg.big ? &large_cont : &small_cont;

                printf("Handling %s write.\n",
                       msg.big ? "LARGE" : "SMALL");

                char str[64];
                sprintf(str,
                    "%" PRIx64 ":%" PRIx64,
                    obj_id.u_hi, obj_id.u_lo);
                int klen = strlen(str);

                char *key_str = m0_alloc(klen + 1);    // Null-terminating
                memcpy(key_str, str, klen + 1);    // Null-terminating

                struct m0_bufvec *keys = bufvec_alloc(1);
                struct m0_bufvec *vals = bufvec_alloc(1);
                char *dummy = m0_alloc(1);
                vals->ov_vec.v_count[0] = 1;
                vals->ov_buf[0] = dummy;

                keys->ov_vec.v_count[0] = klen + 1;
                keys->ov_buf[0] = key_str;    // Can use that directly.
                int rcs[32];

                m0_clovis_container_op(target, M0_CLOVIS_IC_PUT,
                               keys, vals, rcs, 0, ops);

                m0_clovis_op_launch(ops, 1);

                rc = m0_clovis_op_wait(ops[0],
                               M0_BITS
                               (M0_CLOVIS_OS_FAILED,
                            M0_CLOVIS_OS_STABLE),
                               M0_TIME_NEVER);

                bufvec_free(keys);
                bufvec_free(vals);
            }
        }

        pause();
    }
}
