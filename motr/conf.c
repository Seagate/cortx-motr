/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0D
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/finject.h"
#include "lib/string.h"           /* m0_strdup */
#include "motr/setup.h"           /* cs_args */
#include "motr/setup_internal.h"  /* cs_ad_stob_create */
#include "rpc/rpclib.h"           /* m0_rpc_client_ctx */
#include "conf/obj.h"
#include "conf/obj_ops.h"         /* M0_CONF_DIRNEXT */
#include "conf/confc.h"           /* m0_confc */
#include "conf/schema.h"          /* m0_conf_service_type */
#include "conf/dir.h"             /* m0_conf_dir_len */
#include "conf/diter.h"           /* m0_conf_diter_init */
#include "conf/helpers.h"         /* m0_confc_root_open */
#include "reqh/reqh_service.h"    /* m0_reqh_service_ctx */
#include "stob/linux.h"           /* m0_stob_linux_reopen */
#include "ioservice/storage_dev.h" /* m0_storage_dev_attach */
#include "ioservice/fid_convert.h" /* m0_fid_conf_sdev_device_id */
#include "stob/partition.h"        /* m0_stob_part_type*/
#include "be/partition_table.h"    /* m0_be_ptable_id */
#include "stob/partition.h"        /* m0_stob_part_type */
/* ----------------------------------------------------------------
 * Motr options
 * ---------------------------------------------------------------- */

/* Note: `s' is believed to be heap-allocated. */
static void option_add(struct cs_args *args, char *s)
{
	char **argv;

	M0_PRE(0 <= args->ca_argc && args->ca_argc <= args->ca_argc_max);
	if (args->ca_argc == args->ca_argc_max) {
		args->ca_argc_max = args->ca_argc_max == 0 ? 64 :
				    args->ca_argc_max * 2;
		argv = m0_alloc(sizeof(args->ca_argv[0]) * args->ca_argc_max);
		if (args->ca_argv != NULL) {
			memcpy(argv, args->ca_argv,
			       sizeof(args->ca_argv[0]) * args->ca_argc);
			m0_free(args->ca_argv);
		}
		args->ca_argv = argv;
	}
	args->ca_argv[args->ca_argc++] = s;
	M0_LOG(M0_DEBUG, "%02d %s", args->ca_argc, s);
}

static char *
strxdup(const char *addr)
{
	char *s;

	s = m0_alloc(strlen(addr) + strlen(M0_NET_XPRT_PREFIX_DEFAULT) +
		     strlen(":") + 1);
	if (s != NULL)
		sprintf(s, "%s:%s", M0_NET_XPRT_PREFIX_DEFAULT, addr);

	return s;
}

static void
service_options_add(struct cs_args *args, const struct m0_conf_service *svc)
{
	static const char *opts[] = {
		[M0_CST_MDS]     = "-G",
		[M0_CST_IOS]     = "-i",
		[M0_CST_CONFD]   = "",
		[M0_CST_RMS]     = "",
		[M0_CST_STATS]   = "-R",
		[M0_CST_HA]      = "",
		[M0_CST_SSS]     = "",
		[M0_CST_SNS_REP] = "",
		[M0_CST_SNS_REB] = "",
		[M0_CST_ADDB2]   = "",
		[M0_CST_CAS]     = "",
		[M0_CST_DIX_REP] = "",
		[M0_CST_DIX_REB] = "",
		[M0_CST_DS1]     = "",
		[M0_CST_DS2]     = "",
		[M0_CST_FIS]     = "",
		[M0_CST_FDMI]    = "",
		[M0_CST_BE]      = "",
		[M0_CST_M0T1FS]  = "",
		[M0_CST_CLIENT]  = "",
		[M0_CST_ISCS]    = "",
		[M0_CST_DTM0]    = "",
	};
	int         i;
	const char *opt;

	if (svc->cs_endpoints == NULL)
		return;

	for (i = 0; svc->cs_endpoints[i] != NULL; ++i) {
		if (!IS_IN_ARRAY(svc->cs_type, opts)) {
			M0_LOG(M0_ERROR, "invalid service type %d, ignoring",
			       svc->cs_type);
			break;
		}
		opt = opts[svc->cs_type];
		if (opt == NULL)
			continue;
		option_add(args, m0_strdup(opt));
		option_add(args, strxdup(svc->cs_endpoints[i]));
	}
}

M0_UNUSED static void
node_options_add(struct cs_args *args, const struct m0_conf_node *node)
{
/**
 * @todo Node parameters cn_memsize and cn_flags options are not used currently.
 * Options '-m' and '-q' options are used for maximum RPC message size and
 * minimum length of TM receive queue.
 * If required, change the option names accordingly.
 */
/*
	char buf[64] = {0};

	option_add(args, m0_strdup("-m"));
	(void)snprintf(buf, ARRAY_SIZE(buf) - 1, "%u", node->cn_memsize);
	option_add(args, m0_strdup(buf));

	option_add(args, m0_strdup("-q"));
	(void)snprintf(buf, ARRAY_SIZE(buf) - 1, "%lu", node->cn_flags);
	option_add(args, m0_strdup(buf));
*/
}

static bool service_and_node(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_SERVICE_TYPE ||
	       m0_conf_obj_type(obj) == &M0_CONF_NODE_TYPE;
}

M0_INTERNAL int
cs_conf_to_args(struct cs_args *dest, struct m0_conf_root *root)
{
	struct m0_confc      *confc;
	struct m0_conf_diter  it;
	int                   rc;

	M0_ENTRY();

	confc = m0_confc_from_obj(&root->rt_obj);
	M0_ASSERT(confc != NULL);

	rc = m0_conf_diter_init(&it, confc, &root->rt_obj,
				M0_CONF_ROOT_NODES_FID,
				M0_CONF_NODE_PROCESSES_FID,
				M0_CONF_PROCESS_SERVICES_FID);
	if (rc != 0)
		return M0_ERR(rc);

	option_add(dest, m0_strdup("m0d")); /* XXX Does the value matter? */
	while ((rc = m0_conf_diter_next_sync(&it, service_and_node)) ==
		M0_CONF_DIRNEXT) {
		struct m0_conf_obj *obj = m0_conf_diter_result(&it);
		if (m0_conf_obj_type(obj) == &M0_CONF_SERVICE_TYPE) {
			struct m0_conf_service *svc =
				M0_CONF_CAST(obj, m0_conf_service);
			service_options_add(dest, svc);
		} else if(m0_conf_obj_type(obj) == &M0_CONF_NODE_TYPE) {
			struct m0_conf_node *node =
				M0_CONF_CAST(obj, m0_conf_node);
			node_options_add(dest, node);
		}
	}
	m0_conf_diter_fini(&it);
	return M0_RC(rc);
}

static bool is_local_service(const struct m0_conf_obj *obj)
{
	const struct m0_conf_service *svc;
	const struct m0_conf_process *proc;
	struct m0_motr *cctx = m0_cs_ctx_get(m0_conf_obj2reqh(obj));
	const struct m0_fid *proc_fid = &cctx->cc_reqh_ctx.rc_fid;
	const char *local_ep;

	if (m0_conf_obj_type(obj) != &M0_CONF_SERVICE_TYPE)
		return false;
	svc = M0_CONF_CAST(obj, m0_conf_service);
	proc = M0_CONF_CAST(m0_conf_obj_grandparent(&svc->cs_obj),
			    m0_conf_process);
	local_ep = m0_rpc_machine_ep(m0_motr_to_rmach(cctx));
	M0_LOG(M0_DEBUG, "local_ep=%s pc_endpoint=%s type=%d process="FID_F
	       " service=" FID_F, local_ep, proc->pc_endpoint, svc->cs_type,
	       FID_P(&proc->pc_obj.co_id), FID_P(&svc->cs_obj.co_id));
	/*
	 * It is expected for subordinate m0d service to have endpoint equal to
	 * respective process' endpoint.
	 */
	M0_ASSERT_INFO(cctx->cc_mkfs || /* ignore mkfs run */
		       !m0_fid_eq(&proc->pc_obj.co_id, proc_fid) ||
		       m0_streq(local_ep, svc->cs_endpoints[0]),
		       "process=" FID_F " process_fid=" FID_F
		       " local_ep=%s pc_endpoint=%s cs_endpoints[0]=%s",
		       FID_P(&proc->pc_obj.co_id), FID_P(proc_fid), local_ep,
		       proc->pc_endpoint, svc->cs_endpoints[0]);
	return m0_fid_eq(&proc->pc_obj.co_id, proc_fid) &&
		/*
		 * Comparing fids is not enough, since two different processes
		 * (e.g., m0mkfs and m0d) may have the same fid but different
		 * endpoints.
		 *
		 * Start CAS in mkfs mode to create meta indices for DIX. See
		 * MOTR-2793.
		 */
		(m0_streq(proc->pc_endpoint, local_ep) ||
		 (cctx->cc_mkfs && svc->cs_type == M0_CST_CAS));
}

static bool is_ios(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_SERVICE_TYPE &&
		M0_CONF_CAST(obj, m0_conf_service)->cs_type == M0_CST_IOS;
}

static bool is_be(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_SERVICE_TYPE &&
		M0_CONF_CAST(obj, m0_conf_service)->cs_type == M0_CST_BE;
}

static bool is_local_ios(const struct m0_conf_obj *obj)
{
	return is_ios(obj) && is_local_service(obj);
}

static bool is_device(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_SDEV_TYPE;
}

static char *cs_storage_partdom_location_gen(const char          *stob_path,
					     struct m0_be_domain *dom)
{
	char       *location;
	const char *prefix = m0_stob_part_type.st_fidt.ft_name;

	M0_ALLOC_ARR(location,
		     strlen(stob_path) + strlen(prefix) + 128);
	if (location != NULL)
		sprintf(location, "%s:%s:%p", prefix, stob_path, dom);
	return location;
}

/**
 * Generates partition stob location which
 * must be freed with m0_free(). */

static char *cs_storage_partstob_location_gen(const char       *stob_path,
					      const char       *stob_attrib)
{
	char       *location;

	M0_ALLOC_ARR(location,
		     strlen(stob_path) + strlen(stob_attrib) + 2);
	if (location != NULL)
		sprintf(location, "%s:%s", stob_path, stob_attrib);
	return location;
}

#define PART_STOB_MAX_CHUNK_SIZE_IN_BITS  30
static int cs_conf_part_chunk_size_bit_align_get(m0_bcount_t chunk_size)
{
	int chunksize_in_bits;
	int i;
	for (i = 0; i < PART_STOB_MAX_CHUNK_SIZE_IN_BITS; ) {
		if (chunk_size > 0) {
			chunk_size >>= 1;
			i++;
		}
		else
			break;
	}
	chunksize_in_bits = i;
	return (chunksize_in_bits);
}
static void cs_conf_part_common_config_update(struct m0_reqh_context *rctx,
					      struct m0_conf_sdev    *sdev,
					      char                   *dev_path,
					      char                   *bstob)
{
	enum                        { len = 128 };
	m0_bcount_t                 def_dev_chunk_count = 1024;
	struct m0_be_part_cfg      *part_cfg;

	part_cfg = &rctx->rc_be.but_dom_cfg.bc_part_cfg;

	if(part_cfg->bpc_part_mode_set) /* common init msut be done only once */
		return;
	M0_LOG(M0_ALWAYS,"dev path:%s,size:%"PRIu64"alloc_len = %d dom = %p",
	       dev_path, sdev->sd_size, (int)(strlen(dev_path) + len),
	       &rctx->rc_be.but_dom);

	part_cfg->bpc_create_cfg = m0_alloc(strlen(dev_path) + len);
	M0_ASSERT(part_cfg->bpc_create_cfg != NULL);
	sprintf(part_cfg->bpc_create_cfg, "%p %s %"PRIu64,
	&rctx->rc_be.but_dom, dev_path,	sdev->sd_size);
	part_cfg->bpc_init_cfg = part_cfg->bpc_create_cfg;
	part_cfg->bpc_location =
		(char*)cs_storage_partdom_location_gen(dev_path,
						       &rctx->rc_be.but_dom);
	part_cfg->bpc_dom_key = sdev->sd_dev_idx;

	/* 1 chunk for partition table itself */
	part_cfg->bpc_used_chunk_count = 1;

	part_cfg->bpc_bstob = bstob;

	/** parition stob generic configuration */
	part_cfg->bpc_part_mode_set = true;
	part_cfg->bpc_chunk_size_in_bits =
	cs_conf_part_chunk_size_bit_align_get(sdev->sd_size /
					      def_dev_chunk_count);
	part_cfg->bpc_total_chunk_count =
		sdev->sd_size >> part_cfg->bpc_chunk_size_in_bits;
}

static void cs_conf_part_stob_config_update(struct m0_be_part_cfg *part_cfg,
					    int                    index,
					    int64_t                size,
					    char                  *bstob,
					    char                  *path,
					    bool                   directio)
{
	struct m0_be_part_stob_cfg *part_stob_cfg;
	part_stob_cfg = &part_cfg->bpc_stobs_cfg[index];

	part_stob_cfg->bps_enble = true;
	part_stob_cfg->bps_create_cfg =
		(char *)cs_storage_partstob_location_gen(path, directio ?
							 "directio:true":"directio:false");
	part_stob_cfg->bps_init_cfg = part_stob_cfg->bps_create_cfg;
	if (size == -1 ) /* use remaining space */
		part_stob_cfg->bps_size_in_chunks =
			part_cfg->bpc_total_chunk_count -
				part_cfg->bpc_used_chunk_count;
	else if((size > 0) && (size <= 100)) /* chunk size in 0-100 % */
		part_stob_cfg->bps_size_in_chunks =
			( part_cfg->bpc_total_chunk_count * size) /
			100;
	else
		part_stob_cfg->bps_size_in_chunks =
			(( size - 1 ) >> part_cfg->bpc_chunk_size_in_bits) + 1;
	part_cfg->bpc_used_chunk_count += part_stob_cfg->bps_size_in_chunks;
	M0_LOG(M0_ALWAYS,
	       "mk:user =%d b size=%"PRId64"c size=%"PRIu64"used=%"PRIu64"total=%"PRIu64,
	       index, size,
	       part_stob_cfg->bps_size_in_chunks,
	       part_cfg->bpc_used_chunk_count,
	       part_cfg->bpc_total_chunk_count);
	M0_ASSERT(part_cfg->bpc_used_chunk_count <=
		  part_cfg->bpc_total_chunk_count);

}

static int cs_conf_part_config_update(struct m0_reqh_context *rctx,
				      struct m0_conf_sdev    *sdev,
				      int                     index,
				      int64_t                 size,
				      char                   *bstob,
				      char                   *path,
				      bool                    directio)
{
	struct m0_be_part_cfg      *part_cfg;
	struct m0_be_part_stob_cfg *part_stob_cfg;

	part_cfg = &rctx->rc_be.but_dom_cfg.bc_part_cfg;

	switch(index) {
	case M0_BE_DOM_PART_IDX_LOG:
		if (rctx->rc_be_log_path  == NULL ||
		    (strcmp(rctx->rc_be_log_path, path ) == 0)) {
			cs_conf_part_common_config_update(rctx,
							  sdev,
							  path,
							  bstob);
			part_stob_cfg = &part_cfg->bpc_stobs_cfg[index];

			part_stob_cfg->bps_id = M0_BE_PTABLE_ENTRY_LOG;
			rctx->rc_be_log_path = path;
			cs_conf_part_stob_config_update(part_cfg, index,
							size, bstob, path,
							directio);
			rctx->rc_be_log_size =
				part_stob_cfg->bps_size_in_chunks <<
					part_cfg->bpc_chunk_size_in_bits;

		}
		break;
	case M0_BE_DOM_PART_IDX_SEG0:
		if (rctx->rc_be_seg0_path == NULL ||
		    (strcmp(rctx->rc_be_seg0_path, path ) == 0)) {
			cs_conf_part_common_config_update(rctx,
							  sdev,
							  path,
							  bstob);
			part_stob_cfg = &part_cfg->bpc_stobs_cfg[index];

			part_stob_cfg->bps_id = M0_BE_PTABLE_ENTRY_SEG0;
			rctx->rc_be_seg0_path = path;
			cs_conf_part_stob_config_update(part_cfg, index,
							size, bstob, path,
							directio);

		}
		break;
	case M0_BE_DOM_PART_IDX_SEG1:
		if (rctx->rc_be_seg_path == NULL ||
		    (strcmp(rctx->rc_be_seg_path, path ) == 0)) {
			cs_conf_part_common_config_update(rctx,
							  sdev,
							  path,
							  bstob);
			part_stob_cfg = &part_cfg->bpc_stobs_cfg[index];

			part_stob_cfg->bps_id = M0_BE_PTABLE_ENTRY_SEG1;
			rctx->rc_be_seg_path = path;
			if (size > 100 ) /* size in bytes, not in % */
				size = m0_align(size, M0_BE_SEG_PAGE_SIZE);
			cs_conf_part_stob_config_update(part_cfg, index,
							size, bstob, path,
							directio);
			rctx->rc_be_seg_size =
				part_stob_cfg->bps_size_in_chunks <<
					part_cfg->bpc_chunk_size_in_bits;

		}
		break;
	case M0_BE_DOM_PART_IDX_DATA:
		cs_conf_part_common_config_update(rctx,
						  sdev,
						  path,
						  bstob);
		part_stob_cfg = &part_cfg->bpc_stobs_cfg[index];

		part_stob_cfg->bps_id = M0_BE_PTABLE_ENTRY_BALLOC;
		cs_conf_part_stob_config_update(part_cfg, index,
						size, bstob, path,
						directio);

		break;
	default:
		M0_LOG(M0_ERROR,
		       "invalid part user index =%d\n", index);

	}

	return 0;
}
static int cs_conf_part_config_parse_update(struct m0_reqh_context *rctx,
					    struct m0_conf_sdev    *sdev,
                                            char                   *config)
{
	struct m0_be_part_cfg  *part_cfg;
	char                   *token;
	int                     index;
	int64_t                 size;
	char                   *bstob = NULL;
	char                   *path;
	char                   *ssize;
	bool                    directio = false;

	part_cfg = &rctx->rc_be.but_dom_cfg.bc_part_cfg;
	M0_LOG(M0_ALWAYS, "received cfgstring=%s\n", config);
	token = strtok((char *)config, ":");
	if (token != NULL) {
		if (strcmp(token, "part") == 0) {
			token = strtok(NULL, ":");
			if (strcmp(token, "log") == 0) {
				index = M0_BE_DOM_PART_IDX_LOG;
				part_cfg->bpc_stobs_cfg[index].bps_id =
					M0_BE_PTABLE_ENTRY_LOG;
			}
			else if (strcmp(token, "beseg0") == 0) {
				index = M0_BE_DOM_PART_IDX_SEG0;
				part_cfg->bpc_stobs_cfg[index].bps_id =
					M0_BE_PTABLE_ENTRY_SEG0;
			}
			else if (strcmp(token, "beseg1") == 0) {
				index = M0_BE_DOM_PART_IDX_SEG1;
				part_cfg->bpc_stobs_cfg[index].bps_id =
					M0_BE_PTABLE_ENTRY_SEG1;
			}
			else if (strcmp(token, "data") == 0) {
				index = M0_BE_DOM_PART_IDX_DATA;
				part_cfg->bpc_stobs_cfg[index].bps_id =
					M0_BE_PTABLE_ENTRY_BALLOC;
			}
			else {
				index = M0_BE_DOM_PART_IDX_FREE;
				M0_LOG(M0_ERROR,
				       "invalid part user=%s\n", token);
				return M0_ERR(-EINVAL);
			}

			token = strtok(NULL, ":");
			if (strcmp(token, "remaining%") == 0) {
				M0_LOG(M0_DEBUG,
				       "use remaining space for user=%"PRIu64,
				       part_cfg->bpc_stobs_cfg[index].bps_id );
				size = -1;
			}
			else {
				ssize = strndup(token, strlen(token)-1);
				size = atoll(ssize);
				free(ssize);
				switch (token[strlen(token)-1]) {
				case 'K':
					size *= 1024;
					break;
				case 'M':
					size *= (1024 * 1024);
					break;
				case 'G':
					size *= (1024 * 1024 * 1024);
					break;
				case '%':
					break;
				default:
					M0_LOG(M0_ERROR, "invalid size unit=%s\n",
					       token);
					return M0_ERR(-EINVAL);
				}

			}
			token = strtok(NULL, ":");
			if (token[0] != '/') {
				bstob =token;
				token = strtok(NULL, ":");
			}
			if (strcmp(token, "DIRECTIO") == 0) {
				directio = true;
				token = strtok(NULL, ":");
			}
			path = token;
			token = strtok(NULL, ":");
			M0_ASSERT(token == NULL);
			M0_LOG(M0_DEBUG, "path =%s\n", path);
			cs_conf_part_config_update(rctx, sdev, index,
						   size, bstob, path,
						   directio);

		}
		else {
			path = token;
			token = strtok(NULL, ":");
			M0_ASSERT(token == NULL);
			M0_LOG(M0_DEBUG, "path =%s\n", path);
		}
	}
	return 0;
}

M0_INTERNAL int cs_conf_part_config_get(struct m0_reqh_context *rctx,
					struct m0_be_part_cfg  *part_cfg,
					bool                    ioservice)
{
	int                     rc;
	struct m0_motr         *cctx;
	struct m0_confc        *confc;
	struct m0_fid           tmp_fid;
	struct m0_fid          *svc_fid  = NULL;
	struct m0_conf_obj     *proc;
	struct m0_conf_obj     *svc_obj;
	struct m0_conf_diter    it;
	struct m0_conf_service *svc;
	struct m0_fid          *proc_fid;
        struct m0_conf_sdev    *sdev;
	uint32_t                dev_nr;
	uint32_t                dev_count;
	char                   *config;

	M0_ENTRY();
	dev_count = 0;
	cctx = container_of(rctx, struct m0_motr, cc_reqh_ctx);
	confc = m0_motr2confc(cctx);
	proc_fid = &rctx->rc_fid;

	rc = m0_confc_open_by_fid_sync(confc, proc_fid, &proc);
		if (rc != 0)
			return M0_ERR(rc);

	if (rctx->rc_services[M0_CST_DS1] != NULL) { /* setup for tests */
		svc_fid = &rctx->rc_service_fids[M0_CST_DS1];
	}
	else {
		rc = m0_conf_diter_init(&it, confc, proc,
					M0_CONF_PROCESS_SERVICES_FID);
		if (rc != 0)
			return M0_ERR(rc);
		while ((rc = m0_conf_diter_next_sync(&it, ioservice ? is_ios: is_be)) ==
		       M0_CONF_DIRNEXT) {
				struct m0_conf_obj *obj;

				obj = m0_conf_diter_result(&it);
				tmp_fid = obj->co_id;
				svc_fid = &tmp_fid;
				M0_LOG(M0_ALWAYS, "obj->co_id: "FID_F,
				       FID_P(svc_fid));
		}
		m0_conf_diter_fini(&it);
	}
	m0_confc_close(proc);
	if (svc_fid == NULL)
		return -1;

	rc = m0_confc_open_by_fid_sync(confc, svc_fid, &svc_obj);
	if (rc == 0) {
		svc = M0_CONF_CAST(svc_obj, m0_conf_service);
		dev_nr = m0_conf_dir_len(svc->cs_sdevs);
			rc = m0_conf_diter_init(&it, confc, svc_obj,
					M0_CONF_SERVICE_SDEVS_FID);
		if (rc != 0) {
			m0_confc_close(svc_obj);
			return M0_ERR(rc);
		}
		M0_LOG(M0_ALWAYS,"dev_nr = %d", dev_nr);
		while ((rc = m0_conf_diter_next_sync(&it, is_device)) ==
			M0_CONF_DIRNEXT) {
			sdev = M0_CONF_CAST(m0_conf_diter_result(&it),
					    m0_conf_sdev);
			dev_count +=1;
			/* TODO MBK, Temp work around to testpart config */

			config = m0_alloc(256);
			M0_ASSERT(config != NULL);
			sprintf(config, "part:log:128M:linux:DIRECTIO:%s",
				sdev->sd_filename);
			cs_conf_part_config_parse_update(rctx, sdev, config);

			config = m0_alloc(256);
			M0_ASSERT(config != NULL);
			sprintf(config, "part:beseg0:1M:linux:%s",
				sdev->sd_filename);
			cs_conf_part_config_parse_update(rctx, sdev, config);

			config = m0_alloc(256);
			M0_ASSERT(config != NULL);
			sprintf(config, "part:beseg1:%s:linux:%s", "10%",
				sdev->sd_filename);
			cs_conf_part_config_parse_update(rctx, sdev, config);

			config = m0_alloc(256);
			M0_ASSERT(config != NULL);
			sprintf(config, "part:data:%s:linux:DIRECTIO:%s",
				"remaining%", sdev->sd_filename);
			cs_conf_part_config_parse_update(rctx, sdev, config);

			M0_LOG(M0_ALWAYS,
			       "sdev " FID_F " device index: %d "
			       "sdev.sd_filename: %s, "
			       "sdev.sd_size: %" PRIu64
			       "dev count: %d ",
			       FID_P(&sdev->sd_obj.co_id), sdev->sd_dev_idx,
			       sdev->sd_filename, sdev->sd_size,dev_count);
			rc = 0;
		}
		m0_conf_diter_fini(&it);
		m0_confc_close(svc_obj);
	}
	M0_LEAVE("rc = %d",rc);
	return rc;
}

static int cs_conf_storage_attach_by_srv(struct cs_stobs        *cs_stob,
					 struct m0_storage_devs *devs,
					 struct m0_fid          *svc_fid,
					 struct m0_confc        *confc,
					 bool                    force)
{
	struct m0_storage_dev *dev;
	struct m0_conf_obj    *svc_obj;
	struct m0_conf_sdev   *sdev;
	struct m0_stob        *stob;
	int                    rc;

	M0_ENTRY();

	if (svc_fid == NULL)
		return 0;

	rc = m0_confc_open_by_fid_sync(confc, svc_fid, &svc_obj);
	if (rc == 0) {
		struct m0_conf_diter    it;
		struct m0_conf_service *svc = M0_CONF_CAST(svc_obj,
							   m0_conf_service);
		uint32_t                dev_nr;
		struct m0_ha_note      *note;
		uint32_t                fail_devs = 0;

		/*
		 * Total number of devices under service is used to allocate
		 * note vector and notifications are sent only for devices
		 * which are failed with -ENOENT during attach.
		 */
		dev_nr = m0_conf_dir_len(svc->cs_sdevs);
		M0_ASSERT(dev_nr != 0);
		M0_ALLOC_ARR(note, dev_nr);
		if (note == NULL) {
			m0_confc_close(svc_obj);
			return M0_ERR(-ENOMEM);
		}
		rc = m0_conf_diter_init(&it, confc, svc_obj,
					M0_CONF_SERVICE_SDEVS_FID);
		if (rc != 0) {
			m0_free(note);
			m0_confc_close(svc_obj);
			return M0_ERR(rc);
		}

		while ((rc = m0_conf_diter_next_sync(&it, is_device)) ==
			M0_CONF_DIRNEXT) {
			sdev = M0_CONF_CAST(m0_conf_diter_result(&it),
					    m0_conf_sdev);
			M0_LOG(M0_DEBUG,
			       "sdev " FID_F " device index: %d "
			       "sdev.sd_filename: %s, "
			       "sdev.sd_size: %" PRIu64,
			       FID_P(&sdev->sd_obj.co_id), sdev->sd_dev_idx,
			       sdev->sd_filename, sdev->sd_size);

			M0_ASSERT(sdev->sd_dev_idx <= M0_FID_DEVICE_ID_MAX);
			if (sdev->sd_obj.co_ha_state == M0_NC_FAILED)
				continue;
			rc = m0_storage_dev_new_by_conf(devs, sdev, force, &dev);
			if (rc == -ENOENT) {
				M0_LOG(M0_DEBUG, "co_id="FID_F" path=%s rc=%d",
				       FID_P(&sdev->sd_obj.co_id),
				       sdev->sd_filename, rc);
				note[fail_devs].no_id = sdev->sd_obj.co_id;
				note[fail_devs].no_state = M0_NC_FAILED;
				M0_CNT_INC(fail_devs);
				continue;
			}
			if (rc != 0) {
				M0_LOG(M0_ERROR, "co_id="FID_F" path=%s rc=%d",
				       FID_P(&sdev->sd_obj.co_id),
				       sdev->sd_filename, rc);
				break;
			}
			m0_storage_dev_attach(dev, devs);
			stob = m0_storage_devs_find_by_cid(devs,
					    sdev->sd_dev_idx)->isd_stob;
			if (stob != NULL ) {
				if(m0_stob_domain_is_of_type(stob->so_domain,
							     &m0_stob_part_type))
					m0_stob_linux_conf_sdev_associate(
						 m0_stob_part_bstob_get(stob),
						 &sdev->sd_obj.co_id);
				else
					m0_stob_linux_conf_sdev_associate(stob,
							   &sdev->sd_obj.co_id);
			}

		}
		m0_conf_diter_fini(&it);
		if (fail_devs > 0) {
			struct m0_ha_nvec nvec;

			nvec.nv_nr = fail_devs;
			nvec.nv_note = note;
			m0_ha_local_state_set(&nvec);
		}
		m0_free(note);
	}
	m0_confc_close(svc_obj);

	return M0_RC(rc);
}

/* XXX copy-paste from motr/setup.c:pver_is_actual() */
static bool cs_conf_storage_pver_is_actual(const struct m0_conf_obj *obj)
{
	/**
	 * @todo XXX filter only actual pool versions till formulaic
	 *           pool version creation in place.
	 */
	return m0_conf_obj_type(obj) == &M0_CONF_PVER_TYPE &&
		M0_CONF_CAST(obj, m0_conf_pver)->pv_kind == M0_CONF_PVER_ACTUAL;
}

static int cs_conf_storage_is_n1_k0_s0(struct m0_confc *confc)
{
	struct m0_conf_root  *root = NULL;
	struct m0_conf_diter  it;
	struct m0_conf_pver  *pver_obj;
	int                   rc;
	bool                  result = false;

	M0_ENTRY();
	rc = m0_confc_root_open(confc, &root);
	M0_LOG(M0_DEBUG, "m0_confc_root_open: rc=%d", rc);
	if (rc == 0) {
		confc = m0_confc_from_obj(&root->rt_obj);
		rc = m0_conf_diter_init(&it, confc, &root->rt_obj,
					M0_CONF_ROOT_POOLS_FID,
					M0_CONF_POOL_PVERS_FID);
		M0_LOG(M0_DEBUG, "m0_conf_diter_init rc %d", rc);
	}
	while (rc == 0 &&
	       m0_conf_diter_next_sync(&it, cs_conf_storage_pver_is_actual) ==
	       M0_CONF_DIRNEXT) {
		pver_obj = M0_CONF_CAST(m0_conf_diter_result(&it),
					m0_conf_pver);
		if (pver_obj->pv_u.subtree.pvs_attr.pa_N == 1 &&
		    pver_obj->pv_u.subtree.pvs_attr.pa_K == 0) {
			result = true;
			break;
		}
	}
	if (rc == 0)
		m0_conf_diter_fini(&it);
	if (root != NULL)
		m0_confc_close(&root->rt_obj);
	return M0_RC(!!result);
}

M0_INTERNAL int cs_conf_storage_init(struct cs_stobs        *stob,
				     struct m0_storage_devs *devs,
				     bool                    force)
{
	int                     rc;
	struct m0_motr         *cctx;
	struct m0_reqh_context *rctx;
	struct m0_confc        *confc;
	struct m0_fid           tmp_fid;
	struct m0_fid          *svc_fid  = NULL;
	struct m0_conf_obj     *proc;

	M0_ENTRY();

	rctx = container_of(stob, struct m0_reqh_context, rc_stob);
	cctx = container_of(rctx, struct m0_motr, cc_reqh_ctx);
	confc = m0_motr2confc(cctx);

	if (cs_conf_storage_is_n1_k0_s0(confc))
		m0_storage_devs_locks_disable(devs);
	if (rctx->rc_services[M0_CST_DS1] != NULL) { /* setup for tests */
		svc_fid = &rctx->rc_service_fids[M0_CST_DS1];
	} else {
		struct m0_conf_diter  it;
		struct m0_fid        *proc_fid = &rctx->rc_fid;

		M0_ASSERT(m0_fid_is_set(proc_fid));

		M0_LOG(M0_DEBUG, FID_F, FID_P(proc_fid));
		rc = m0_confc_open_by_fid_sync(confc, proc_fid, &proc);
		if (rc != 0)
			return M0_ERR(rc);

		rc = m0_conf_diter_init(&it, confc, proc,
					M0_CONF_PROCESS_SERVICES_FID);
		if (rc != 0)
			return M0_ERR(rc);

		while ((rc = m0_conf_diter_next_sync(&it, is_ios)) ==
		       M0_CONF_DIRNEXT) {
			struct m0_conf_obj *obj = m0_conf_diter_result(&it);
			/*
			 * Copy of fid is needed because the conf cache can
			 * be invalidated after m0_confc_close() invocation
			 * rendering any pointer as invalid too.
			 */
			tmp_fid = obj->co_id;
			svc_fid = &tmp_fid;
			M0_LOG(M0_DEBUG, "obj->co_id: "FID_F, FID_P(svc_fid));
		}
		m0_conf_diter_fini(&it);
		m0_confc_close(proc);
	}
	if (svc_fid != NULL)
		M0_LOG(M0_DEBUG, "svc_fid: "FID_F, FID_P(svc_fid));

	/*
	 * XXX A kludge.
	 * See m0_storage_dev_attach() comment in cs_storage_devs_init().
	 */
	m0_storage_devs_lock(devs);
	rc = cs_conf_storage_attach_by_srv(stob, devs, svc_fid, confc, force);
	m0_storage_devs_unlock(devs);
	return M0_RC(rc);
}

M0_INTERNAL int cs_conf_services_init(struct m0_motr *cctx)
{
	int                        rc;
	struct m0_conf_diter       it;
	struct m0_conf_root       *root;
	struct m0_reqh_context    *rctx;
	struct m0_confc           *confc;

	M0_ENTRY();

	rctx = &cctx->cc_reqh_ctx;
	rctx->rc_nr_services = 0;
	confc = m0_motr2confc(cctx);
	rc = m0_confc_root_open(confc, &root);
	if (rc != 0)
		return M0_ERR_INFO(rc, "conf root open fail");
	rc = M0_FI_ENABLED("diter_fail") ? -ENOMEM :
		m0_conf_diter_init(&it, confc, &root->rt_obj,
				   M0_CONF_ROOT_NODES_FID,
				   M0_CONF_NODE_PROCESSES_FID,
				   M0_CONF_PROCESS_SERVICES_FID);
	if (rc != 0)
		goto fs_close;
	while ((rc = m0_conf_diter_next_sync(&it, is_local_service)) ==
		M0_CONF_DIRNEXT) {
		struct m0_conf_obj     *obj = m0_conf_diter_result(&it);
		struct m0_conf_service *svc =
			M0_CONF_CAST(obj, m0_conf_service);
		char                   *svc_type_name =
			m0_strdup(m0_conf_service_type2str(svc->cs_type));

		M0_LOG(M0_DEBUG, "service:%s fid:" FID_F, svc_type_name,
		       FID_P(&svc->cs_obj.co_id));
		M0_ASSERT(rctx->rc_nr_services < M0_CST_NR);
		if (svc_type_name == NULL) {
			int i;
			rc = M0_ERR(-ENOMEM);
			for (i = 0; i < rctx->rc_nr_services; ++i)
				m0_free(rctx->rc_services[i]);
			break;
		}
		M0_ASSERT_INFO(rctx->rc_services[svc->cs_type] == NULL,
			       "registering " FID_F " service type=%d when "
			       FID_F " for the type is already registered",
			       FID_P(&svc->cs_obj.co_id), svc->cs_type,
			       FID_P(&rctx->rc_service_fids[svc->cs_type]));
		rctx->rc_services[svc->cs_type] = svc_type_name;
		rctx->rc_service_fids[svc->cs_type] = svc->cs_obj.co_id;
		M0_CNT_INC(rctx->rc_nr_services);
	}
	m0_conf_diter_fini(&it);
fs_close:
	m0_confc_close(&root->rt_obj);
	return M0_RC(rc);
}

M0_INTERNAL int cs_conf_device_reopen(struct m0_poolmach *pm,
				      struct cs_stobs *stob,
				      uint32_t dev_id)
{
	struct m0_motr         *cctx;
	struct m0_reqh_context *rctx;
	struct m0_confc        *confc;
	int                     rc;
	struct m0_fid           fid;
	struct m0_conf_sdev    *sdev;
	struct m0_stob_id       stob_id;
	struct m0_conf_service *svc;

	M0_ENTRY();

	rctx = container_of(stob, struct m0_reqh_context, rc_stob);
	cctx = container_of(rctx, struct m0_motr, cc_reqh_ctx);
	confc = m0_motr2confc(cctx);
	fid = pm->pm_state->pst_devices_array[dev_id].pd_id;

	rc = m0_conf_sdev_get(confc, &fid, &sdev);
	if (rc != 0)
		return M0_ERR(rc);

	svc = M0_CONF_CAST(m0_conf_obj_grandparent(&sdev->sd_obj),
			   m0_conf_service);
	if (is_local_ios(&svc->cs_obj)) {
		M0_LOG(M0_DEBUG, "sdev size: %" PRId64 " path: %s FID:"FID_F,
		       sdev->sd_size, sdev->sd_filename,
		       FID_P(&sdev->sd_obj.co_id));
		m0_stob_id_make(0, dev_id, &stob->s_sdom->sd_id, &stob_id);
		rc = m0_stob_linux_reopen(&stob_id, sdev->sd_filename);
	}
	m0_confc_close(&sdev->sd_obj);
	return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
