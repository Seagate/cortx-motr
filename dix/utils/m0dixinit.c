/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 *
 */



/**
 * @addtogroup dix
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIX
#include "lib/trace.h"

#include <stdio.h>
#include "lib/getopts.h"     /* M0_GETOPTS */
#include "lib/thread.h"      /* LAMBDA */
#include "lib/hash_fnc.h"    /* HASH_FNC_CITY */
#include "lib/uuid.h"        /* m0_uuid_generate */
#include "lib/string.h"      /* m0_streq */
#include "lib/ext.h"         /* m0_ext */
#include "module/instance.h" /* m0 */
#include "pool/pool.h"       /* m0_pool_version */
#include "conf/confc.h"      /* m0_confc_close */
#include "conf/ha.h"         /* m0_conf_ha_process_event_post */
#include "conf/helpers.h"    /* m0_confc_args */
#include "motr/ha.h"
#include "rpc/rpc_machine.h" /* m0_rpc_machine */
#include "rpc/rpc.h"         /* m0_rpc_bufs_nr */
#include "reqh/reqh.h"       /* m0_reqh */
#include "rm/rm_service.h"   /* m0_rms_type */
#include "net/buffer_pool.h" /* m0_net_buffer_pool */
#include "dix/meta.h"
#include "dix/layout.h"
#include "dix/client.h"

enum dix_action {
	ACTION_CREATE,
	ACTION_CHECK,
	ACTION_DESTROY
};

struct dix_ctx {
	struct m0_pools_common     dc_pools_common;
	struct m0_motr_ha          dc_motr_ha;
	const char                *dc_laddr;
	struct m0_net_domain       dc_ndom;
	struct m0_rpc_machine      dc_rpc_machine;
	struct m0_reqh             dc_reqh;
	struct m0_net_buffer_pool  dc_buffer_pool;
};

static uint32_t tm_recv_queue_min_len = 10;
static uint32_t max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;

static int dix_ha_init(struct dix_ctx *ctx, const char *ha_addr)
{
	struct m0_motr_ha_cfg motr_ha_cfg;
	int                   rc;

	M0_ENTRY();
	motr_ha_cfg = (struct m0_motr_ha_cfg){
		.mhc_addr             = ha_addr,
		.mhc_rpc_machine      = &ctx->dc_rpc_machine,
		.mhc_reqh             = &ctx->dc_reqh,
		.mhc_dispatcher_cfg = {
			.hdc_enable_note      = true,
			.hdc_enable_keepalive = false,
			.hdc_enable_fvec      = true
		},
	};
	rc = m0_motr_ha_init(&ctx->dc_motr_ha, &motr_ha_cfg);
	if (rc != 0)
		return M0_ERR(rc);
	rc = m0_motr_ha_start(&ctx->dc_motr_ha);
	if (rc != 0) {
		m0_motr_ha_fini(&ctx->dc_motr_ha);
		return M0_ERR(rc);
	}
	m0_motr_ha_connect(&ctx->dc_motr_ha);
	return M0_RC(rc);
}

static void dix_ha_stop(struct dix_ctx *ctx)
{
	M0_ENTRY();
	m0_conf_ha_process_event_post(&ctx->dc_motr_ha.mh_ha,
	                               ctx->dc_motr_ha.mh_link,
	                              &ctx->dc_reqh.rh_fid, m0_process(),
				      M0_CONF_HA_PROCESS_STOPPED,
				      M0_CONF_HA_PROCESS_OTHER);
	m0_motr_ha_disconnect(&ctx->dc_motr_ha);
	m0_motr_ha_stop(&ctx->dc_motr_ha);
	M0_LEAVE();
}

static void dix_ha_fini(struct dix_ctx *ctx)
{
	m0_motr_ha_fini(&ctx->dc_motr_ha);
}

static int dix_net_init(struct dix_ctx *ctx, const char *local_addr)
{
	M0_LOG(M0_DEBUG, "local ep is %s", local_addr);
	ctx->dc_laddr = local_addr;
	return M0_RC(m0_net_domain_init(&ctx->dc_ndom,
		     m0_net_xprt_default_get()));
}

static int dix_rpc_init(struct dix_ctx *ctx)
{
	struct m0_rpc_machine     *rpc_machine = &ctx->dc_rpc_machine;
	struct m0_reqh            *reqh        = &ctx->dc_reqh;
	struct m0_net_domain      *ndom        = &ctx->dc_ndom;
	const char                *laddr;
	struct m0_net_buffer_pool *buffer_pool = &ctx->dc_buffer_pool;
	struct m0_net_transfer_mc *tm;
	int                        rc;
	uint32_t                   bufs_nr;
	uint32_t                   tms_nr;

	M0_ENTRY();
	tms_nr = 1;
	bufs_nr = m0_rpc_bufs_nr(tm_recv_queue_min_len, tms_nr);

	rc = m0_rpc_net_buffer_pool_setup(ndom, buffer_pool,
					  bufs_nr, tms_nr);
	if (rc != 0)
		return M0_ERR(rc);

	rc = M0_REQH_INIT(reqh,
			  .rhia_dtm = (void*)1,
			  .rhia_db = NULL,
			  .rhia_mdstore = (void*)1,
			  .rhia_pc = &ctx->dc_pools_common,
			  /* fake process fid */
			  .rhia_fid = &M0_FID_TINIT(
				  M0_CONF_PROCESS_TYPE.cot_ftype.ft_id, 0, 0));
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
	return M0_RC(rc);
reqh_fini:
	m0_reqh_fini(reqh);
pool_fini:
	m0_rpc_net_buffer_pool_cleanup(buffer_pool);
	return M0_ERR(rc);
}

static void dix_rpc_fini(struct dix_ctx *ctx)
{
	M0_ENTRY();

	m0_rpc_machine_fini(&ctx->dc_rpc_machine);
	if (m0_reqh_state_get(&ctx->dc_reqh) != M0_REQH_ST_STOPPED)
		m0_reqh_services_terminate(&ctx->dc_reqh);
	m0_reqh_fini(&ctx->dc_reqh);
	m0_rpc_net_buffer_pool_cleanup(&ctx->dc_buffer_pool);
	M0_LEAVE();
}

static void dix_net_fini(struct dix_ctx *ctx)
{
	M0_ENTRY();
	m0_net_domain_fini(&ctx->dc_ndom);
	M0_LEAVE();
}

M0_INTERNAL struct m0_rconfc *dix2rconfc(struct dix_ctx *ctx)
{
	return &ctx->dc_reqh.rh_rconfc;
}

static int dix_layouts_init(struct dix_ctx *ctx)
{
	int rc;

	M0_ENTRY();
	rc = m0_reqh_mdpool_layout_build(&ctx->dc_reqh);
	return M0_RC(rc);
}

static void dix_layouts_fini(struct dix_ctx *ctx)
{
	M0_ENTRY();
	m0_reqh_layouts_cleanup(&ctx->dc_reqh);
	M0_LEAVE();
}

static int dix_service_start(struct m0_reqh_service_type *stype,
			     struct m0_reqh *reqh)
{
	struct m0_reqh_service *service;
	struct m0_uint128       uuid;

	m0_uuid_generate(&uuid);
	m0_fid_tassume((struct m0_fid *)&uuid, &M0_CONF_SERVICE_TYPE.cot_ftype);
	return m0_reqh_service_setup(&service, stype, reqh, NULL,
			            (struct m0_fid *)&uuid);
}

static int dix_reqh_services_start(struct dix_ctx *ctx)
{
	struct m0_reqh *reqh = &ctx->dc_reqh;
	int rc;

	rc = dix_service_start(&m0_rms_type, reqh);
	if (rc != 0) {
		M0_ERR(rc);
		m0_reqh_services_terminate(reqh);
	}
	return M0_RC(rc);
}

static int dix_init(struct dix_ctx *ctx,
		    const char *local_addr,
		    const char *ha_addr,
		    const char *profile)
{
	struct m0_pools_common    *pc = &ctx->dc_pools_common;
	struct m0_confc_args      *confc_args;
	struct m0_reqh            *reqh = &ctx->dc_reqh;
	struct m0_conf_root       *root;
	int                        rc;

	M0_ENTRY();
	rc = m0_layout_domain_init(&ctx->dc_reqh.rh_ldom);
	if (rc != 0)
		return M0_ERR(rc);
	rc = m0_layout_standard_types_register(&ctx->dc_reqh.rh_ldom);
	if (rc != 0)
		goto err_domain_fini;

	rc = dix_net_init(ctx, local_addr);
	if (rc != 0)
		goto err_domain_fini;

	rc = dix_rpc_init(ctx);
	if (rc != 0)
		goto err_net_fini;

	confc_args = &(struct m0_confc_args) {
		.ca_profile = profile,
		.ca_rmach   = &ctx->dc_rpc_machine,
		.ca_group   = m0_locality0_get()->lo_grp
	};

	rc = dix_ha_init(ctx, ha_addr);
	if (rc != 0)
		goto err_ha_fini;

	rc = m0_reqh_conf_setup(reqh, confc_args);
	if (rc != 0)
		goto err_ha_fini;

	rc = m0_rconfc_start_sync(dix2rconfc(ctx));
	if (rc != 0)
		goto err_rconfc_stop;

	rc = m0_confc_root_open(m0_reqh2confc(reqh), &root);
	if (rc != 0)
		goto err_rconfc_stop;

	rc = m0_conf_full_load(root);
	if (rc != 0)
		goto err_conf_fs_close;

	rc = m0_pools_common_init(pc, &ctx->dc_rpc_machine);
	if (rc != 0)
		goto err_conf_fs_close;

	rc = m0_pools_setup(pc, m0_reqh2profile(reqh), NULL, NULL);
	if (rc != 0)
		goto err_pools_common_fini;

	rc = m0_pools_service_ctx_create(pc);
	if (rc != 0)
		goto err_pools_destroy;

	m0_pools_common_service_ctx_connect_sync(pc);

	rc = m0_pool_versions_setup(pc);
	if (rc != 0)
		goto err_pools_service_ctx_destroy;

	rc = dix_reqh_services_start(ctx);
	if (rc != 0)
		goto err_pool_versions_destroy;

	rc = dix_layouts_init(ctx);
	if (rc != 0) {
		dix_layouts_fini(ctx);
		goto err_pool_versions_destroy;
	}

	m0_confc_close(&root->rt_obj);
	return M0_RC(0);

err_pool_versions_destroy:
	m0_pool_versions_destroy(&ctx->dc_pools_common);
err_pools_service_ctx_destroy:
	m0_pools_service_ctx_destroy(&ctx->dc_pools_common);
err_pools_destroy:
	m0_pools_destroy(&ctx->dc_pools_common);
err_pools_common_fini:
	m0_pools_common_fini(&ctx->dc_pools_common);
err_conf_fs_close:
	m0_confc_close(&root->rt_obj);
err_rconfc_stop:
	m0_rconfc_stop_sync(dix2rconfc(ctx));
	m0_rconfc_fini(dix2rconfc(ctx));
	m0_reqh_services_terminate(reqh);
err_ha_fini:
	dix_ha_stop(ctx);
	dix_ha_fini(ctx);
	dix_rpc_fini(ctx);
err_net_fini:
	dix_net_fini(ctx);
err_domain_fini:
	m0_layout_domain_fini(&ctx->dc_reqh.rh_ldom);
	return M0_ERR(rc);
}

static void dix_fini(struct dix_ctx *ctx)
{
	dix_layouts_fini(ctx);
	m0_layout_domain_cleanup(&ctx->dc_reqh.rh_ldom);
	m0_layout_standard_types_unregister(&ctx->dc_reqh.rh_ldom);
	m0_layout_domain_fini(&ctx->dc_reqh.rh_ldom);
	m0_pools_service_ctx_destroy(&ctx->dc_pools_common);
	m0_pool_versions_destroy(&ctx->dc_pools_common);
	m0_pools_destroy(&ctx->dc_pools_common);
	m0_pools_common_fini(&ctx->dc_pools_common);
	dix_ha_stop(ctx);
	m0_rconfc_stop_sync(dix2rconfc(ctx));
	m0_rconfc_fini(dix2rconfc(ctx));
	dix_ha_fini(ctx);
	dix_rpc_fini(ctx);
	dix_net_fini(ctx);
}

static int dix_root_pver_find(struct dix_ctx *ctx, struct m0_fid *out)
{
	struct m0_reqh      *reqh = &ctx->dc_reqh;
	struct m0_conf_root *root;
	int                  rc;

	rc = m0_confc_root_open(m0_reqh2confc(reqh), &root);
	if (rc != 0)
		return M0_ERR(rc);
	*out = root->rt_imeta_pver;
	m0_confc_close(&root->rt_obj);
	return 0;
}

static int dix_pver_fids_check(struct dix_ctx      *ctx,
			       const struct m0_fid *root,
			       const struct m0_fid *layout,
			       const struct m0_fid *ldescr)
{
	struct m0_pool_version *pv;
	const struct m0_fid    *fids[3] = { root, layout, ldescr };
	int                     i;
	int                     rc = 0;

	for (i = 0; i < ARRAY_SIZE(fids); i++) {
		pv = m0_pool_version_find(&ctx->dc_pools_common, fids[i]);
		if (pv == NULL) {
			rc = M0_ERR(-ENOENT);
			fprintf(stderr,
				"Pool version for FID="FID_F" is not found.\n",
				FID_P(fids[i]));
		}
	}
	return rc;
}

int main(int argc, char **argv)
{
	int                 rc;
	struct m0           instance;
	struct dix_ctx      ctx;
	char               *local_addr  = NULL;
	char               *ha_addr     = NULL;
	char               *prof        = NULL;
	char               *layout_pver = NULL;
	char               *ldescr_pver = NULL;
	char               *action      = NULL;
	enum dix_action     act;
	struct m0_fid       layout_pver_fid;
	struct m0_fid       ldescr_pver_fid;
	struct m0_fid       root_pver_fid;
	struct m0_dix_ldesc dld1;
	struct m0_dix_ldesc dld2;
	struct m0_dix_cli   cli;
	struct m0_sm_group  *sm_grp;
	struct m0_ext        range[] = {
		{ .e_start = 0, .e_end = IMASK_INF },
	};

	M0_SET0(&instance);
	m0_instance_setup(&instance);
	rc = m0_module_init(&instance.i_self, M0_LEVEL_INST_READY);
	if (rc != 0) {
		fprintf(stderr, "Cannot init module %i\n", rc);
		goto end;
	}
	M0_SET0(&ctx);
	rc = M0_GETOPTS("m0dixinit", argc, argv,
			M0_HELPARG('h'),
			M0_STRINGARG('l', "Local endpoint address",
					LAMBDA(void, (const char *string) {
						local_addr = (char*)string;
					})),
			M0_STRINGARG('H', "HA address",
					LAMBDA(void, (const char *str) {
						ha_addr = (char*)str;
					})),
			M0_STRINGARG('p', "Profile options for Motr",
					LAMBDA(void, (const char *str) {
						prof = (char*)str;
					})),
			M0_STRINGARG('I', "'layout' index pool version FID",
					LAMBDA(void, (const char *string) {
						layout_pver = (char*)string;
					})),
			M0_STRINGARG('d', "'layoutd-descr' index pool version"
					" FID",
					LAMBDA(void, (const char *string) {
						ldescr_pver = (char*)string;
					})),
			M0_STRINGARG('a', "\n\t\t\tcreate - create meta"
					" info\n\t\t\tcheck - check meta info\n"
					"\t\t\tdestroy - destroy meta info",
					LAMBDA(void, (const char *string) {
						action = (char*)string;
					})),
			);
	if (rc != 0)
		goto end;
	if (local_addr == NULL ||
	    ha_addr == NULL ||
	    prof == NULL ||
	    action == NULL) {
		fprintf(stderr, "Invalid parameter(s).\n");
		rc = EINVAL;
		goto end;
	}
	if (m0_streq(action, "create")) {
		act = ACTION_CREATE;
	} else if (m0_streq(action, "check")) {
		act = ACTION_CHECK;
	} else if (m0_streq(action, "destroy")) {
		act = ACTION_DESTROY;
	} else {
		fprintf(stderr,
			"Invalid action! Actions: create|check|destroy.\n");
		rc = EINVAL;
		goto end;
	}
	if (act == ACTION_CREATE &&
	   (layout_pver == NULL || ldescr_pver == NULL)) {
		fprintf(stderr, "layout in case of CREATE is mandatory.\n");
		rc = EINVAL;
		goto end;
	}

	/* Init Layout descriptors */
	rc = m0_fid_sscanf(layout_pver, &layout_pver_fid);
	if (rc != 0) {
		fprintf(stderr, "Incorrect FID format for: %s\n", layout_pver);
		goto end;
	}
	m0_fid_sscanf(ldescr_pver, &ldescr_pver_fid);
	if (rc != 0) {
		fprintf(stderr, "Incorrect FID format for: %s\n", ldescr_pver);
		goto end;
	}
	rc = dix_init(&ctx, local_addr, ha_addr, prof);
	if (rc != 0) {
		fprintf(stderr, "Initialisation error: %d\n", rc);
		goto end;
	}
	rc = dix_root_pver_find(&ctx, &root_pver_fid);
	if (rc != 0) {
		fprintf(stderr, "Can't find root pver fid: %d\n", rc);
		goto fini;
	}

	rc = dix_pver_fids_check(&ctx, &root_pver_fid, &layout_pver_fid,
				 &ldescr_pver_fid);
	if (rc != 0)
		goto fini;
	rc = m0_dix_ldesc_init(&dld1, range, ARRAY_SIZE(range),
			       HASH_FNC_CITY, &layout_pver_fid);
	if (rc != 0)
		goto fini;
	rc = m0_dix_ldesc_init(&dld2, range, ARRAY_SIZE(range),
			       HASH_FNC_CITY, &ldescr_pver_fid);
	if (rc != 0) {
		m0_dix_ldesc_fini(&dld1);
		goto fini;
	}

	if (rc == 0) {
		sm_grp = m0_locality0_get()->lo_grp;
		rc = m0_dix_cli_init(&cli, sm_grp, &ctx.dc_pools_common,
				     &ctx.dc_reqh.rh_ldom, &root_pver_fid);
		if (rc != 0) {
			fprintf(stderr,
				"DIX client initialisation failure: (%d)\n",
				rc);
			goto ldesc_fini;
		}
		if (act == ACTION_CREATE)
			m0_dix_cli_bootstrap_lock(&cli);
		else
			rc = m0_dix_cli_start_sync(&cli);
		if (rc == 0) {
			switch(act) {
			case ACTION_CREATE:
				rc = m0_dix_meta_create(&cli, sm_grp, &dld1,
						        &dld2);
				break;
			case ACTION_CHECK:
			{
				bool exists = true;

				rc = m0_dix_meta_check(&cli, sm_grp, &exists);
				if (rc == 0)
					fprintf(stdout, "Metadata exists: %s\n",
						exists ? "true" : "false");
				break;
			}
			case ACTION_DESTROY:
				rc = m0_dix_meta_destroy(&cli, sm_grp);
				break;
			default:
				M0_IMPOSSIBLE("Wrong action");
			}
			fprintf(stdout, "Execution result: %s (%d)\n", rc == 0 ?
					"Success" : "Failure", rc);
			m0_dix_cli_stop_lock(&cli);
		} else {
			/*
			 * -ENOENT most probably means that root index is not
			 *  created. If user checks for meta-data then don't
			 *  return error, just say that metadata doesn't exist.
			 */
			if (rc == -ENOENT && act == ACTION_CHECK) {
				fprintf(stdout, "Metadata exists: false\n");
				rc = 0;
			} else {
				fprintf(stderr,
					"DIX client start failure: (%d)\n", rc);
				if (rc == -ENOENT)
					fprintf(stderr, "No metadata found?\n");
			}
		}
		m0_dix_cli_fini_lock(&cli);
	}

ldesc_fini:
	m0_dix_ldesc_fini(&dld1);
	m0_dix_ldesc_fini(&dld2);
fini:
	dix_fini(&ctx);
end:
	return M0_RC(rc >= 0 ? rc : -rc);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of dix group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
