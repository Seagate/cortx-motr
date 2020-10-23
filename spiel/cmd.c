/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SPIEL
#include "lib/trace.h"
#include "lib/finject.h"        /* M0_FI_ENABLED */

#include "conf/obj_ops.h"          /* M0_CONF_DIRNEXT, m0_conf_obj_get */
#include "conf/preload.h"          /* m0_confx_string_free */
#include "conf/helpers.h"          /* m0_conf_service_name_dup */
#include "fid/fid_list.h"          /* m0_fid_item */
#include "rpc/rpclib.h"            /* m0_rpc_post_with_timeout_sync */
#include "cm/repreb/trigger_fom.h" /* m0_cm_op */
#include "cm/repreb/trigger_fop.h" /* trigger_fop */
#include "sns/cm/trigger_fop.h"    /* m0_sns_cm_trigger_fop_alloc */
#include "dix/cm/trigger_fop.h"    /* m0_dix_cm_trigger_fop_alloc */
#include "sss/device_fops.h"       /* m0_sss_device_fop_create */
#include "sss/ss_fops.h"
#include "sss/process_fops.h"      /* m0_ss_fop_process_rep */
#include "spiel/spiel.h"
#include "spiel/spiel_internal.h"
#include "spiel/cmd_internal.h"

/**
 * @addtogroup spiel-api-fspec-intr
 * @{
 */

M0_TL_DESCR_DEFINE(spiel_string, "list of endpoints",
		   static, struct spiel_string_entry, sse_link, sse_magic,
		   M0_STATS_MAGIC, M0_STATS_HEAD_MAGIC);
M0_TL_DEFINE(spiel_string, static, struct spiel_string_entry);

static void spiel_fop_destroy(struct m0_fop *fop)
{
	m0_rpc_machine_lock(m0_fop_rpc_machine(fop));
	m0_fop_fini(fop);
	m0_rpc_machine_unlock(m0_fop_rpc_machine(fop));
	m0_free(fop);
}

static void spiel_rpc_link_destroy(struct m0_rpc_link *rlink)
{
	M0_LOG(M0_DEBUG, "destroying rpc link %p", rlink);
	if (rlink != NULL) {
		if (rlink->rlk_connected) {
			m0_time_t conn_timeout;
			conn_timeout = m0_time_from_now(SPIEL_CONN_TIMEOUT, 0);
			m0_rpc_link_disconnect_sync(rlink, conn_timeout);
		}
		m0_rpc_link_fini(rlink);
		m0_free(rlink);
	}
}

static bool _filter_svc(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_SERVICE_TYPE;
}

static bool _filter_controller(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_CONTROLLER_TYPE;
}

static int spiel_node_process_endpoint_add(struct m0_spiel_core *spc,
				           struct m0_conf_obj   *node,
				           struct m0_tl         *list)
{
	struct spiel_string_entry *entry;
	struct m0_conf_diter       it;
	struct m0_conf_process    *p;
	struct m0_conf_service    *svc;
	int                        rc;

	M0_ENTRY("conf_node: %p", &node);
	M0_PRE(m0_conf_obj_type(node) == &M0_CONF_NODE_TYPE);

	rc = m0_conf_diter_init(&it, spc->spc_confc, node,
				M0_CONF_NODE_PROCESSES_FID,
				M0_CONF_PROCESS_SERVICES_FID);
	if (rc != 0)
		return M0_ERR(rc);

	while ((rc = m0_conf_diter_next_sync(&it, _filter_svc)) ==
		     M0_CONF_DIRNEXT) {
		svc = M0_CONF_CAST(m0_conf_diter_result(&it), m0_conf_service);
		if (svc->cs_type == M0_CST_IOS) {
			p = M0_CONF_CAST(m0_conf_obj_grandparent(&svc->cs_obj),
					 m0_conf_process);
			M0_ALLOC_PTR(entry);
			if (entry == NULL) {
				rc = M0_ERR(-ENOMEM);
				break;
			}
			entry->sse_string = m0_strdup(p->pc_endpoint);
			spiel_string_tlink_init_at(entry, list);
			break;
		}
	}

	m0_conf_diter_fini(&it);
	return M0_RC(rc);
}

/**
 * Finds the node, which `drive' belongs to, and collects endpoints of
 * IOS-running processes of that node.
 */
static int spiel_endpoints_for_device_generic(struct m0_spiel_core *spc,
					      const struct m0_fid  *drive,
					      struct m0_tl         *out)
{
	struct m0_confc           *confc = spc->spc_confc;
	struct m0_conf_diter       it;
	struct m0_conf_obj        *root;
	struct m0_conf_obj        *drive_obj;
	struct m0_conf_obj        *ctrl_obj;
	struct m0_conf_obj        *encl_obj;
	struct m0_conf_enclosure  *encl;
	struct m0_conf_obj        *node_obj;
	int                        rc;

	M0_ENTRY();
	M0_PRE(drive != NULL);
	M0_PRE(out != NULL);

	if (m0_conf_fid_type(drive) != &M0_CONF_DRIVE_TYPE)
		return M0_ERR(-EINVAL);

	rc = m0_confc_open_sync(&root, confc->cc_root, M0_FID0);
	if (rc != 0)
		return M0_ERR(rc);

	rc = m0_conf_diter_init(&it, confc, root,
				M0_CONF_ROOT_SITES_FID,
				M0_CONF_SITE_RACKS_FID,
				M0_CONF_RACK_ENCLS_FID,
				M0_CONF_ENCLOSURE_CTRLS_FID);
	if (rc != 0) {
		m0_conf_diter_fini(&it);
		return M0_ERR(rc);
	}

	while ((rc = m0_conf_diter_next_sync(&it, _filter_controller)) ==
							M0_CONF_DIRNEXT) {
		ctrl_obj = m0_conf_diter_result(&it);
		rc = m0_confc_open_sync(&drive_obj, ctrl_obj,
					M0_CONF_CONTROLLER_DRIVES_FID, *drive);

		if (rc == 0) {
			encl_obj = m0_conf_obj_grandparent(ctrl_obj);
			encl = M0_CONF_CAST(encl_obj, m0_conf_enclosure);
			rc = m0_confc_open_by_fid_sync(confc,
					       &encl->ce_node->cn_obj.co_id,
					       &node_obj);
			if (rc == 0) {
				spiel_node_process_endpoint_add(spc, node_obj,
							        out);
				m0_confc_close(node_obj);
			}
			m0_confc_close(drive_obj);
		}
	}

	m0_conf_diter_fini(&it);
	m0_confc_close(root);

	return M0_RC(rc);
}

static int spiel_cmd_send(struct m0_rpc_machine *rmachine,
			  const char            *remote_ep,
			  struct m0_fop         *cmd_fop,
			  m0_time_t              timeout,
			  struct m0_rpc_link   **rlink)
{
	struct m0_rpc_link *rpc_link;
	m0_time_t           conn_timeout;
	int                 rc;

	M0_ENTRY("lep=%s ep=%s", m0_rpc_machine_ep(rmachine), remote_ep);

	M0_PRE(rmachine != NULL);
	M0_PRE(remote_ep != NULL);
	M0_PRE(cmd_fop != NULL);

	/* RPC link structure is too big to allocate it on stack */
	M0_ALLOC_PTR(rpc_link);
	if (rpc_link == NULL)
		return M0_ERR(-ENOMEM);

	M0_LOG(M0_DEBUG, "initializing rpc link %p", rlink);
	rc = m0_rpc_link_init(rpc_link, rmachine, NULL, remote_ep,
			      SPIEL_MAX_RPCS_IN_FLIGHT);
	if (rc == 0) {
		conn_timeout = m0_time_from_now(SPIEL_CONN_TIMEOUT, 0);
		if (M0_FI_ENABLED("timeout"))
			timeout = M0_TIME_ONE_SECOND;
		rc = m0_rpc_link_connect_sync(rpc_link, conn_timeout);

		if (rc == 0) {
			*rlink = rpc_link;
			rc = m0_rpc_post_with_timeout_sync(cmd_fop,
							&rpc_link->rlk_sess,
							NULL,
							M0_TIME_IMMEDIATELY,
							timeout);
		} else {
			m0_rpc_link_fini(rpc_link);
			m0_free(rpc_link);
		}
	}

	return M0_RC(rc);
}

/**
 * @brief Find object with given obj_fid in provided confc.
 *
 * Function uses @ref m0_conf_diter internally and traverses tree starting from
 * the root object in confc.
 *
 * Should not be called directly, macro @ref SPIEL_CONF_OBJ_FIND
 * should be used instead.
 *
 * User should close returned non-NULL conf_obj using @ref m0_confc_close()
 *
 * @param confc     confc instance
 * @param obj_fid   object FID to search for
 * @param filter    function to filter nodes during tree traversal.
 *                  Identical to filter argument in @ref m0_conf_diter_next_sync
 * @param nr_lvls   Number of directory levels to traverse
 *                  (@ref m0_conf__diter_init)
 * @param path      Path in configuration tree to traverse. Path should be
 *                  started from the root object
 * @param conf_obj  output value. Found object or NULL if object is not found
 */
static int _spiel_conf_obj_find(struct m0_confc       *confc,
				const struct m0_fid   *obj_fid,
				bool (*filter)(const struct m0_conf_obj *obj),
				uint32_t               nr_lvls,
				const struct m0_fid   *path,
				struct m0_conf_obj   **conf_obj)
{
	int                  rc;
	struct m0_conf_obj  *root_obj = NULL;
	struct m0_conf_obj  *tmp;
	struct m0_conf_diter it;

	M0_ENTRY("obj_fid="FID_F, FID_P(obj_fid));

	M0_PRE(confc != NULL);
	M0_PRE(obj_fid != NULL);
	M0_PRE(path != NULL);
	M0_PRE(nr_lvls > 0);
	M0_PRE(m0_fid_eq(&path[0], &M0_CONF_ROOT_NODES_FID) ||
	       m0_fid_eq(&path[0], &M0_CONF_ROOT_POOLS_FID));
	M0_PRE(conf_obj != NULL);

	*conf_obj = NULL;

	rc = m0_confc_open_sync(&root_obj, confc->cc_root, M0_FID0);
	if (rc != 0)
		return M0_ERR(rc);

	rc = m0_conf__diter_init(&it, confc, root_obj, nr_lvls, path);

	if (rc != 0) {
		m0_confc_close(root_obj);
		return M0_ERR(rc);
	}

	while ((rc = m0_conf_diter_next_sync(&it, filter)) == M0_CONF_DIRNEXT) {
		tmp = m0_conf_diter_result(&it);

		if (m0_fid_eq(&tmp->co_id, obj_fid)) {
			*conf_obj = tmp;
			/* Pin object to protect it from removal */
			m0_conf_obj_get_lock(*conf_obj);
			rc = 0;
			break;
		}
	}

	m0_conf_diter_fini(&it);
	m0_confc_close(root_obj);

	if (*conf_obj == NULL && rc == 0)
		rc = M0_ERR(-ENOENT);

	M0_POST(ergo(rc == 0, (*conf_obj)->co_nrefs > 0));
	return M0_RC(rc);
}

/**
 * Iterates through directory specified by the path and calls iterator callback
 * on every directory item having the object pinned during callback
 * execution.
 */
static int _spiel_conf_dir_iterate(struct m0_confc     *confc,
				   void *ctx,
				   bool (*iter_cb)(const struct m0_conf_obj
						   *item, void *ctx),
				   uint32_t nr_lvls,
				   const struct m0_fid *path)
{
	int                  rc;
	bool                 loop;
	struct m0_conf_obj  *root_obj = NULL;
	struct m0_conf_obj  *obj;
	struct m0_conf_diter it;

	M0_ENTRY();

	M0_PRE(confc != NULL);
	M0_PRE(path != NULL);
	M0_PRE(nr_lvls > 0);
	M0_PRE(m0_fid_eq(&path[0], &M0_CONF_ROOT_NODES_FID));

	rc = m0_confc_open_sync(&root_obj, confc->cc_root, M0_FID0);
	if (rc != 0)
		return M0_ERR(rc);

	rc = m0_conf__diter_init(&it, confc, root_obj, nr_lvls, path);

	if (rc != 0) {
		m0_confc_close(root_obj);
		return M0_ERR(rc);
	}

	loop = true;
	while (loop &&
	       (m0_conf_diter_next_sync(&it, NULL)) == M0_CONF_DIRNEXT) {
		obj = m0_conf_diter_result(&it);
		/* Pin obj to protect it from removal while being in use */
		m0_conf_obj_get_lock(obj);
		loop = iter_cb(obj, ctx);
		m0_mutex_lock(&confc->cc_lock);
		m0_conf_obj_put(obj);
		m0_mutex_unlock(&confc->cc_lock);
	}

	m0_conf_diter_fini(&it);
	m0_confc_close(root_obj);

	return M0_RC(rc);
}

/****************************************************/
/*                     Services                     */
/****************************************************/

/**
 * Find endpoint of SSS service which accepts commands for given service
 */
static int spiel_ss_ep_for_svc(const struct m0_conf_service  *s,
			       char                         **ss_ep)
{
	struct m0_conf_process *p;

	M0_PRE(s != NULL);
	M0_PRE(ss_ep != NULL);

	M0_ENTRY();

	/* m0_conf_process::pc_services dir is the parent for the service */
	p = M0_CONF_CAST(m0_conf_obj_grandparent(&s->cs_obj), m0_conf_process);
	M0_ASSERT(!m0_conf_obj_is_stub(&p->pc_obj));
	/* All services within a process share the same endpoint. */
	*ss_ep = m0_strdup(p->pc_endpoint);
	return M0_RC(*ss_ep == NULL ? -ENOENT : 0);
}

static int spiel_svc_conf_obj_find(struct m0_spiel_core    *spc,
				   const struct m0_fid     *svc,
				   struct m0_conf_service **out)
{
	struct m0_conf_obj *obj;
	int                 rc;

	rc = SPIEL_CONF_OBJ_FIND(spc->spc_confc, svc, &obj, _filter_svc,
				 M0_CONF_ROOT_NODES_FID,
				 M0_CONF_NODE_PROCESSES_FID,
				 M0_CONF_PROCESS_SERVICES_FID);
	if (rc == 0)
		*out = M0_CONF_CAST(obj, m0_conf_service);
	return M0_RC(rc);
}

static int spiel_svc_fop_fill(struct m0_fop          *fop,
			      struct m0_conf_service *svc,
			      uint32_t                cmd)
{
	struct m0_sss_req *ss_fop = m0_fop_data(fop);
	char              *name;

	ss_fop->ss_cmd = cmd;
	ss_fop->ss_id  = svc->cs_obj.co_id;

	name = m0_strdup(m0_conf_service_type2str(svc->cs_type));
	if (name == NULL)
		return M0_ERR(-ENOMEM);
	m0_buf_init(&ss_fop->ss_name, name, strlen(name));

	/* TODO: Check what parameters are used by which service types
	 * and fill ss_fop->ss_param appropriately */

	return 0;
}

static int spiel_svc_fop_fill_and_send(struct m0_spiel_core *spc,
				       struct m0_fop        *fop,
				       const struct m0_fid  *svc_fid,
				       uint32_t              cmd,
				       struct m0_rpc_link  **rlink)
{
	int                     rc;
	struct m0_conf_service *svc;
	char                   *ss_ep = NULL;

	M0_ENTRY();
	M0_PRE(M0_IN(cmd, (M0_SERVICE_INIT, M0_SERVICE_START,
			   M0_SERVICE_STOP, M0_SERVICE_QUIESCE,
			   M0_SERVICE_HEALTH, M0_SERVICE_STATUS)));
	M0_PRE(m0_conf_fid_type(svc_fid) == &M0_CONF_SERVICE_TYPE);
	M0_PRE(spc != NULL);
	rc = spiel_svc_conf_obj_find(spc, svc_fid, &svc);
	if (rc != 0)
		return M0_ERR(rc);

	rc = spiel_svc_fop_fill(fop, svc, cmd) ?:
	     spiel_ss_ep_for_svc(svc, &ss_ep);
	if (rc == 0) {
		rc = spiel_cmd_send(spc->spc_rmachine, ss_ep, fop,
				    M0_TIME_NEVER, rlink);
		m0_free(ss_ep);
	}

	m0_confc_close(&svc->cs_obj);
	return M0_RC(rc);
}

static struct m0_fop *spiel_svc_fop_alloc(struct m0_rpc_machine *mach)
{
	return m0_fop_alloc(&m0_fop_ss_fopt, NULL, mach);
}

static struct m0_sss_rep *spiel_sss_reply_data(struct m0_fop *fop)
{
	struct m0_rpc_item *item    = fop->f_item.ri_reply;
	struct m0_fop      *rep_fop = m0_rpc_item_to_fop(item);

	return (struct m0_sss_rep *)m0_fop_data(rep_fop);
}

static int spiel_svc_generic_handler(struct m0_spiel_core *spc,
				     const struct m0_fid  *svc_fid,
				     enum m0_sss_req_cmd   cmd,
				     int                  *status)
{
	int                 rc;
	struct m0_fop      *fop;
	struct m0_rpc_link *rlink = NULL;

	M0_ENTRY();

	if (m0_conf_fid_type(svc_fid) != &M0_CONF_SERVICE_TYPE)
		return M0_ERR(-EINVAL);

	fop = spiel_svc_fop_alloc(spc->spc_rmachine);

	if (fop == NULL)
		return M0_ERR(-ENOMEM);

	rc = spiel_svc_fop_fill_and_send(spc, fop, svc_fid, cmd, &rlink) ?:
	     spiel_sss_reply_data(fop)->ssr_rc;

	if (rc == 0 && status != NULL)
		*status = spiel_sss_reply_data(fop)->ssr_state;

	m0_fop_put_lock(fop);
	spiel_rpc_link_destroy(rlink);

	return M0_RC(rc);
}

int m0_spiel_service_init(struct m0_spiel *spl, const struct m0_fid *svc_fid)
{
	M0_ENTRY("spl %p svc_fid "FID_F, spl, FID_P(svc_fid));

	return M0_RC(spiel_svc_generic_handler(&spl->spl_core, svc_fid,
					       M0_SERVICE_INIT, NULL));
}
M0_EXPORTED(m0_spiel_service_init);

int m0_spiel_service_start(struct m0_spiel *spl, const struct m0_fid *svc_fid)
{
	M0_ENTRY("spl %p svc_fid "FID_F, spl, FID_P(svc_fid));

	return M0_RC(spiel_svc_generic_handler(&spl->spl_core, svc_fid,
					       M0_SERVICE_START, NULL));
}
M0_EXPORTED(m0_spiel_service_start);

int m0_spiel_service_stop(struct m0_spiel *spl, const struct m0_fid *svc_fid)
{
	M0_ENTRY("spl %p svc_fid "FID_F, spl, FID_P(svc_fid));
	if (m0_fid_eq(&spl->spl_rconfc.rc_ha_entrypoint_rep.hae_active_rm_fid,
		      svc_fid))
		return M0_ERR(-EPERM);

	return M0_RC(spiel_svc_generic_handler(&spl->spl_core, svc_fid,
					       M0_SERVICE_STOP, NULL));
}
M0_EXPORTED(m0_spiel_service_stop);

int m0_spiel_service_health(struct m0_spiel *spl, const struct m0_fid *svc_fid)
{
	M0_ENTRY("spl %p svc_fid "FID_F, spl, FID_P(svc_fid));

	return M0_RC(spiel_svc_generic_handler(&spl->spl_core, svc_fid,
					       M0_SERVICE_HEALTH, NULL));
}
M0_EXPORTED(m0_spiel_service_health);

int m0_spiel_service_quiesce(struct m0_spiel     *spl,
		             const struct m0_fid *svc_fid)
{
	M0_ENTRY("spl %p svc_fid "FID_F, spl, FID_P(svc_fid));

	return M0_RC(spiel_svc_generic_handler(&spl->spl_core, svc_fid,
					       M0_SERVICE_QUIESCE, NULL));
}
M0_EXPORTED(m0_spiel_service_quiesce);

int m0_spiel_service_status(struct m0_spiel *spl, const struct m0_fid *svc_fid,
			    int *status)
{
	M0_ENTRY("spl %p svc_fid "FID_F, spl, FID_P(svc_fid));

	return M0_RC(spiel_svc_generic_handler(&spl->spl_core, svc_fid,
					       M0_SERVICE_STATUS, status));
}
M0_EXPORTED(m0_spiel_service_status);


/****************************************************/
/*                     Devices                      */
/****************************************************/

static int spiel_device_command_fop_send(struct m0_spiel_core *spc,
					 const char           *endpoint,
					 const struct m0_fid  *dev_fid,
					 int                   cmd,
					 uint32_t             *ha_state)
{
	struct m0_fop                *fop;
	struct m0_sss_device_fop_rep *rep;
	int                           rc;
	struct m0_rpc_link	     *rlink = NULL;

	fop = m0_sss_device_fop_create(spc->spc_rmachine, cmd, dev_fid);
	if (fop == NULL)
		return M0_ERR(-ENOMEM);

	rc = spiel_cmd_send(spc->spc_rmachine, endpoint, fop,
			    cmd == M0_DEVICE_FORMAT ?
				   SPIEL_DEVICE_FORMAT_TIMEOUT :
				   M0_TIME_NEVER, &rlink);
	if (rc == 0) {
		rep = m0_sss_fop_to_dev_rep(
				m0_rpc_item_to_fop(fop->f_item.ri_reply));
		rc = rep->ssdp_rc;
		if (ha_state != NULL)
			*ha_state = rep->ssdp_ha_state;
	}
	m0_fop_put_lock(fop);
	spiel_rpc_link_destroy(rlink);

	return M0_RC(rc);
}

/**
 * Send `cmd' (m0_sss_device_fop::ssd_cmd) to all IO services that use `drive'.
 */
static int spiel_device_command_send(struct m0_spiel_core       *spc,
				     const struct m0_fid        *drive,
				     enum m0_sss_device_req_cmd  cmd,
				     uint32_t                   *ha_state)
{
	struct m0_tl               endpoints;
	struct spiel_string_entry *ep;
	int                        rc;

	M0_ENTRY();

	if (m0_conf_fid_type(drive) != &M0_CONF_DRIVE_TYPE)
		return M0_ERR_INFO(-EINVAL, "drive="FID_F, FID_P(drive));

	spiel_string_tlist_init(&endpoints);

	rc = spiel_endpoints_for_device_generic(spc, drive, &endpoints);
	if (rc == 0 && spiel_string_tlist_is_empty(&endpoints))
		rc = M0_ERR_INFO(-ENOENT, "No IOS endpoints found for drive "
				 FID_F, FID_P(drive));

	if (rc == 0)
		m0_tl_teardown(spiel_string, &endpoints, ep) {
			rc = rc ?:
			spiel_device_command_fop_send(spc, ep->sse_string,
						      drive, cmd, ha_state);
			m0_free((char *)ep->sse_string);
			m0_free(ep);
		}

	spiel_string_tlist_fini(&endpoints);

	return M0_RC(rc);
}

int m0_spiel_device_attach(struct m0_spiel *spl, const struct m0_fid *dev_fid)
{
	return m0_spiel_device_attach_state(spl, dev_fid, NULL);
}
M0_EXPORTED(m0_spiel_device_attach);

int m0_spiel_device_attach_state(struct m0_spiel     *spl,
				 const struct m0_fid *dev_fid,
				 uint32_t            *ha_state)
{
	M0_ENTRY("spl %p svc_fid "FID_F, spl, FID_P(dev_fid));

	return M0_RC(spiel_device_command_send(&spl->spl_core, dev_fid,
					       M0_DEVICE_ATTACH, ha_state));
}
M0_EXPORTED(m0_spiel_device_attach_state);

int m0_spiel_device_detach(struct m0_spiel *spl, const struct m0_fid *dev_fid)
{
	M0_ENTRY("spl %p svc_fid "FID_F, spl, FID_P(dev_fid));

	return M0_RC(spiel_device_command_send(&spl->spl_core, dev_fid,
					       M0_DEVICE_DETACH, NULL));
}
M0_EXPORTED(m0_spiel_device_detach);

int m0_spiel_device_format(struct m0_spiel *spl, const struct m0_fid *dev_fid)
{
	M0_ENTRY("spl %p svc_fid "FID_F, spl, FID_P(dev_fid));

	return M0_RC(spiel_device_command_send(&spl->spl_core, dev_fid,
					       M0_DEVICE_FORMAT, NULL));
}
M0_EXPORTED(m0_spiel_device_format);

/****************************************************/
/*                   Processes                      */
/****************************************************/
static bool _filter_proc(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_PROCESS_TYPE;
}

static int spiel_proc_conf_obj_find(struct m0_spiel_core    *spc,
				    const struct m0_fid     *proc,
				    struct m0_conf_process **out)
{
	struct m0_conf_obj *obj;
	int                 rc;

	rc = SPIEL_CONF_OBJ_FIND(spc->spc_confc, proc, &obj, _filter_proc,
				 M0_CONF_ROOT_NODES_FID,
				 M0_CONF_NODE_PROCESSES_FID);
	if (rc == 0)
		*out = M0_CONF_CAST(obj, m0_conf_process);
	return M0_RC(rc);
}

static int spiel_process_command_send(struct m0_spiel_core *spc,
				      const struct m0_fid  *proc_fid,
				      struct m0_fop        *fop,
				      struct m0_rpc_link  **rlink)
{
	struct m0_conf_process *process;
	int                     rc;

	M0_ENTRY();

	M0_PRE(spc != NULL);
	M0_PRE(proc_fid != NULL);
	M0_PRE(fop != NULL);
	M0_PRE(m0_conf_fid_type(proc_fid) == &M0_CONF_PROCESS_TYPE);
	M0_PRE(spc != NULL);

	rc = spiel_proc_conf_obj_find(spc, proc_fid, &process);
	if (rc != 0)
		return M0_ERR(rc);

	m0_confc_close(&process->pc_obj);

	rc = spiel_cmd_send(spc->spc_rmachine, process->pc_endpoint, fop,
			    M0_TIME_NEVER, rlink);
	if (rc != 0)
		spiel_fop_destroy(fop);
	return M0_RC(rc);
}

static struct m0_ss_process_rep *spiel_process_reply_data(struct m0_fop *fop)
{
	return m0_ss_fop_process_rep(m0_rpc_item_to_fop(fop->f_item.ri_reply));
}

/**
 * When ssp_rep is not NULL, reply fop data will be copied there.
 */
static int spiel_process_command_execute(struct m0_spiel_core *spc,
					 const struct m0_fid  *proc_fid,
					 int                   cmd,
					 const struct m0_buf  *param,
					 struct m0_ss_process_rep *ssp_rep)
{
	struct m0_fop            *fop;
	struct m0_ss_process_req *req;
	int                       rc;
	struct m0_rpc_link       *rlink = NULL;

	M0_ENTRY();

	M0_PRE(spc != NULL);
	M0_PRE(proc_fid != NULL);

	if (m0_conf_fid_type(proc_fid) != &M0_CONF_PROCESS_TYPE)
		return M0_ERR(-EINVAL);
	fop = m0_ss_process_fop_create(spc->spc_rmachine, cmd, proc_fid);
	if (fop == NULL)
		return M0_ERR(-ENOMEM);
	req = m0_ss_fop_process_req(fop);
	if (param != NULL)
		req->ssp_param = *param;
	rc = spiel_process_command_send(spc, proc_fid, fop, &rlink);
	req->ssp_param = M0_BUF_INIT0; /* Clean param before destruction. */
	if (rc == 0) {
		rc = spiel_process_reply_data(fop)->sspr_rc;
		if (ssp_rep != NULL)
			*ssp_rep = *spiel_process_reply_data(fop);
	}
	m0_fop_put_lock(fop);
	spiel_rpc_link_destroy(rlink);

	return M0_RC(rc);
}

static int spiel_process_command(struct m0_spiel      *spl,
				 const struct m0_fid  *proc_fid,
				 int                   cmd)
{
	return spiel_process_command_execute(&spl->spl_core, proc_fid,
					     cmd, NULL, NULL);
}

int m0_spiel_process_stop(struct m0_spiel *spl, const struct m0_fid *proc_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_process_command(spl, proc_fid, M0_PROCESS_STOP));
}
M0_EXPORTED(m0_spiel_process_stop);

int m0_spiel_process_reconfig(struct m0_spiel     *spl,
			      const struct m0_fid *proc_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_process_command(spl, proc_fid, M0_PROCESS_RECONFIG));
}
M0_EXPORTED(m0_spiel_process_reconfig);

static int spiel_process__health(struct m0_spiel_core *spc,
				 const struct m0_fid  *proc_fid)
{
	struct m0_ss_process_rep ssp_rep = { 0 };
	int            rc;
	int            health;

	M0_ENTRY();

	health = M0_HEALTH_UNKNOWN;
	rc = spiel_process_command_execute(spc, proc_fid, M0_PROCESS_HEALTH,
					   NULL, &ssp_rep);
	if (rc == 0) {
		health = ssp_rep.sspr_health;
	}
	return rc < 0 ? M0_ERR(rc) : M0_RC(health);
}

int m0_spiel_process_health(struct m0_spiel     *spl,
			    const struct m0_fid *proc_fid)
{
	return M0_RC(spiel_process__health(&spl->spl_core, proc_fid));
}
M0_EXPORTED(m0_spiel_process_health);

int m0_spiel_process_quiesce(struct m0_spiel     *spl,
			     const struct m0_fid *proc_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_process_command(spl, proc_fid, M0_PROCESS_QUIESCE));
}
M0_EXPORTED(m0_spiel_process_quiesce);

static int spiel_running_svcs_list_fill(struct m0_bufs               *bufs,
					struct m0_spiel_running_svc **svcs)
{
	struct m0_ss_process_svc_item *src;
	struct m0_spiel_running_svc   *services;
	int                            i;

	M0_ENTRY();

	M0_ALLOC_ARR(services, bufs->ab_count);
	if (services == NULL)
		return M0_ERR(-ENOMEM);
	for(i = 0; i < bufs->ab_count; ++i) {
		src = (struct m0_ss_process_svc_item *)bufs->ab_elems[i].b_addr;
		services[i].spls_fid = src->ssps_fid;
		services[i].spls_name = m0_strdup(src->ssps_name);
		if (services[i].spls_name == NULL)
			goto err;
	}
	*svcs = services;

	return M0_RC(bufs->ab_count);
err:
	for(i = 0; i < bufs->ab_count; ++i)
		m0_free(services[i].spls_name);
	m0_free(services);
	return M0_ERR(-ENOMEM);
}

int m0_spiel_process_list_services(struct m0_spiel              *spl,
				   const struct m0_fid          *proc_fid,
				   struct m0_spiel_running_svc **services)
{
	struct m0_fop                     *fop;
	struct m0_ss_process_svc_list_rep *rep;
	int                                rc;
        struct m0_rpc_link                *rlink = NULL;

	M0_ENTRY();

	if (m0_conf_fid_type(proc_fid) != &M0_CONF_PROCESS_TYPE)
		return M0_ERR(-EINVAL);

	fop = m0_ss_process_fop_create(spiel_rmachine(spl),
				       M0_PROCESS_RUNNING_LIST, proc_fid);
	if (fop == NULL)
		return M0_ERR(-ENOMEM);
	rc = spiel_process_command_send(&spl->spl_core, proc_fid, fop, &rlink);
	if (rc == 0) {
		rep = m0_ss_fop_process_svc_list_rep(
				m0_rpc_item_to_fop(fop->f_item.ri_reply));
		rc = rep->sspr_rc;
		if (rc == 0)
			rc = spiel_running_svcs_list_fill(&rep->sspr_services,
							  services);
	}
	m0_fop_put_lock(fop);
	spiel_rpc_link_destroy(rlink);

	return M0_RC(rc);
}
M0_EXPORTED(m0_spiel_process_list_services);

int m0_spiel_process_lib_load(struct m0_spiel     *spl,
			      const struct m0_fid *proc_fid,
			      const char          *libname)
{
	const struct m0_buf param = M0_BUF_INIT_CONST(strlen(libname) + 1,
						      libname);
	M0_ENTRY(); /* The terminating NUL is part of param. */
	return M0_RC(spiel_process_command_execute(&spl->spl_core, proc_fid,
						   M0_PROCESS_LIB_LOAD,
						   &param, NULL));
}
M0_EXPORTED(m0_spiel_process_lib_load);

/****************************************************/
/*                      Pools                       */
/****************************************************/

static int spiel_stats_item_add(struct m0_tl *tl, const struct m0_fid *fid)
{
	struct m0_fid_item *si;

	M0_ALLOC_PTR(si);
	if (si == NULL)
		return M0_ERR(-ENOMEM);
	si->i_fid = *fid;
	m0_fids_tlink_init_at(si, tl);
	return M0_RC(0);
}


static bool _filter_pool(const struct m0_conf_obj *obj)
{
	M0_LOG(M0_DEBUG, FID_F, FID_P(&obj->co_id));
	return m0_conf_obj_type(obj) == &M0_CONF_POOL_TYPE;
}

/* Spiel SNS/DIX repair/re-balance context per service. */
struct spiel_repreb {
	struct m0_spiel_repreb_status  sr_status;
	/* RPC link for the corresponding service. */
	struct m0_rpc_link             sr_rlink;
	struct m0_conf_service        *sr_service;
	struct m0_fop                 *sr_fop;
	int                            sr_rc;
	bool                           sr_is_connected;
};

static int spiel_repreb_cmd_send(struct m0_rpc_machine *rmachine,
				 const char            *remote_ep,
				 struct spiel_repreb   *repreb)
{
	struct m0_rpc_link *rlink = &repreb->sr_rlink;
	m0_time_t           conn_timeout;
	int                 rc;
	struct m0_fop      *fop;
	struct m0_rpc_item *item;

	M0_ENTRY("lep=%s ep=%s", m0_rpc_machine_ep(rmachine), remote_ep);

	M0_PRE(rmachine != NULL);
	M0_PRE(remote_ep != NULL);
	M0_PRE(repreb != NULL);

	fop = repreb->sr_fop;
	rc = m0_rpc_link_init(rlink, rmachine, NULL, remote_ep,
			      SPIEL_MAX_RPCS_IN_FLIGHT);
	if (rc == 0) {
		conn_timeout = m0_time_from_now(SPIEL_CONN_TIMEOUT, 0);
		rc = m0_rpc_link_connect_sync(rlink, conn_timeout);
		if (rc != 0) {
			m0_rpc_link_fini(rlink);
			return M0_RC(rc);
		}
		item              = &fop->f_item;
		item->ri_ops      = NULL;
		item->ri_session  = &rlink->rlk_sess;
		item->ri_prio     = M0_RPC_ITEM_PRIO_MID;
		item->ri_deadline = M0_TIME_IMMEDIATELY;
		m0_fop_get(fop);
		rc = m0_rpc_post(item);
		m0_fop_put_lock(fop);
	}
	repreb->sr_is_connected = true;
	return M0_RC(rc);
}

static int spiel_repreb_fop_fill_and_send(struct m0_spiel_core *spc,
					  struct m0_fop        *fop,
					  enum m0_cm_op         op,
					  struct spiel_repreb  *repreb)
{
	struct trigger_fop     *treq = m0_fop_data(fop);
	struct m0_conf_service *svc = repreb->sr_service;
	struct m0_conf_process *p = M0_CONF_CAST(
					m0_conf_obj_grandparent(&svc->cs_obj),
					m0_conf_process);

	M0_ENTRY("fop %p conf_service %p", fop, svc);
	treq->op = op;
	repreb->sr_fop = fop;
	repreb->sr_is_connected = false;
	return M0_RC(spiel_repreb_cmd_send(spc->spc_rmachine, p->pc_endpoint,
					   repreb));
}

/** pool stats collection context */
struct _pool_cmd_ctx {
	int                   pl_rc;
	struct m0_spiel_core *pl_spc;          /**< spiel instance */
	struct m0_tl          pl_sdevs_fid;    /**< storage devices fid list */
	struct m0_tl          pl_services_fid; /**< services fid list */
	enum m0_repreb_type   pl_service_type; /**< type of service: SNS or DIX */
};

static int spiel_pool_device_collect(struct _pool_cmd_ctx *ctx,
				     struct m0_conf_obj *obj_diskv)
{
	struct m0_conf_objv *diskv;
	struct m0_conf_obj  *obj_disk;
	struct m0_conf_drive *disk;
	int                  rc;

	diskv = M0_CONF_CAST(obj_diskv, m0_conf_objv);
	if (diskv == NULL)
		return M0_ERR(-ENOENT);
	if (m0_conf_obj_type(diskv->cv_real) != &M0_CONF_DRIVE_TYPE)
		return -EINVAL; /* rackv, ctrlv objectv's are ignored. */

	rc = m0_confc_open_by_fid_sync(ctx->pl_spc->spc_confc,
				       &diskv->cv_real->co_id, &obj_disk);
	if (rc != 0)
		return M0_RC(rc);

	disk = M0_CONF_CAST(obj_disk, m0_conf_drive);
	if (disk->ck_sdev != NULL)
		rc = spiel_stats_item_add(&ctx->pl_sdevs_fid,
					  &disk->ck_sdev->sd_obj.co_id);

	m0_confc_close(obj_disk);
	return M0_RC(rc);
}

static bool _filter_sdev(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_SDEV_TYPE;
}

static bool spiel__pool_service_has_sdev(struct _pool_cmd_ctx *ctx,
					 const struct m0_conf_obj *service)
{
	struct m0_conf_diter it;
	struct m0_conf_obj  *obj;
	int                  rc;
	bool                 found = false;

	rc = m0_conf_diter_init(&it, ctx->pl_spc->spc_confc,
				(struct m0_conf_obj*)service,
				M0_CONF_SERVICE_SDEVS_FID);

	if (rc != 0)
		return false;

	while ((rc = m0_conf_diter_next_sync(&it, _filter_sdev))
						== M0_CONF_DIRNEXT && !found) {
		obj = m0_conf_diter_result(&it);
		if (m0_tl_find(m0_fids, si, &ctx->pl_sdevs_fid,
			       m0_fid_eq(&si->i_fid, &obj->co_id)) != NULL)
			found = true;
	}
	m0_conf_diter_fini(&it);
	return found;
}

static void spiel__pool_ctx_init(struct _pool_cmd_ctx *ctx,
				 struct m0_spiel_core *spc,
				 enum m0_repreb_type   type)
{
	M0_SET0(ctx);
	ctx->pl_spc          = spc;
	ctx->pl_service_type = type;
	m0_fids_tlist_init(&ctx->pl_sdevs_fid);
	m0_fids_tlist_init(&ctx->pl_services_fid);
	M0_POST(m0_fids_tlist_invariant(&ctx->pl_sdevs_fid));
	M0_POST(m0_fids_tlist_invariant(&ctx->pl_services_fid));
}

static void spiel__pool_ctx_fini(struct _pool_cmd_ctx *ctx)
{
	struct m0_fid_item *si;

	if (!m0_fids_tlist_is_empty(&ctx->pl_sdevs_fid))
		m0_tl_teardown(m0_fids, &ctx->pl_sdevs_fid, si) {
			m0_free(si);
		}
	m0_fids_tlist_fini(&ctx->pl_sdevs_fid);

	if (!m0_fids_tlist_is_empty(&ctx->pl_services_fid))
		m0_tl_teardown(m0_fids, &ctx->pl_services_fid, si) {
			m0_free(si);
		}
	m0_fids_tlist_fini(&ctx->pl_services_fid);
}

static void spiel__add_item(struct _pool_cmd_ctx      *pool_ctx,
			    const struct m0_conf_obj  *item,
			    struct m0_conf_service    *service,
			    enum m0_conf_service_type  type)
{
	if (service->cs_type == type &&
	    spiel__pool_service_has_sdev(pool_ctx, item))
		pool_ctx->pl_rc = spiel_stats_item_add(
					&pool_ctx->pl_services_fid,
					&item->co_id);
}
static bool spiel__pool_service_select(const struct m0_conf_obj *item,
				       void                     *ctx)
{
	struct _pool_cmd_ctx   *pool_ctx = ctx;
	struct m0_conf_service *service;

	/* continue iterating only when no issue occurred previously */
	if (pool_ctx->pl_rc != 0)
		return false;
	/* skip all but service objects */
	if (m0_conf_obj_type(item) != &M0_CONF_SERVICE_TYPE)
		return true;

	service = M0_CONF_CAST(item, m0_conf_service);
	if (pool_ctx->pl_service_type == M0_REPREB_TYPE_SNS)
		spiel__add_item(pool_ctx, item, service, M0_CST_IOS);
	else if (pool_ctx->pl_service_type == M0_REPREB_TYPE_DIX)
		spiel__add_item(pool_ctx, item, service, M0_CST_CAS);
	return true;
}

static int spiel__pool_cmd_send(struct _pool_cmd_ctx *ctx,
				const enum m0_cm_op   cmd,
				struct spiel_repreb  *repreb)
{
	struct m0_fop *fop;
	int            rc;

	M0_PRE(repreb != NULL);
	if (ctx->pl_service_type == M0_REPREB_TYPE_SNS)
		rc = m0_sns_cm_trigger_fop_alloc(ctx->pl_spc->spc_rmachine,
						 cmd, &fop);
	else
		rc = m0_dix_cm_trigger_fop_alloc(ctx->pl_spc->spc_rmachine,
						 cmd, &fop);
	if (rc != 0)
		return M0_ERR(rc);
	return M0_RC(spiel_repreb_fop_fill_and_send(ctx->pl_spc, fop, cmd,
						    repreb));
}

static int spiel__pool_cmd_status_get(struct _pool_cmd_ctx *ctx,
				      const enum m0_cm_op   cmd,
				      struct spiel_repreb  *repreb)
{
	int                            rc;
	struct m0_fop                 *fop;
	struct m0_rpc_item            *item;
	struct m0_spiel_repreb_status *status;
	struct m0_status_rep_fop      *reply;
	m0_time_t                      conn_timeout;

	M0_PRE(repreb != NULL);

	status = &repreb->sr_status;
	fop = repreb->sr_fop;
	M0_ASSERT(ergo(repreb->sr_is_connected, fop != NULL));
	item = &fop->f_item;
	rc = repreb->sr_rc ?: m0_rpc_item_wait_for_reply(item, M0_TIME_NEVER) ?:
		m0_rpc_item_error(item);

	if (M0_IN(cmd, (CM_OP_REPAIR_STATUS, CM_OP_REBALANCE_STATUS))) {
		status->srs_fid = repreb->sr_service->cs_obj.co_id;
		if (rc == 0) {
			reply = m0_fop_data(m0_rpc_item_to_fop(item->ri_reply));
			status->srs_progress = reply->ssr_progress;
			status->srs_state = reply->ssr_state;
		} else {
			status->srs_state = rc;
		}
	}
	m0_fop_put0_lock(fop);
	if (repreb->sr_is_connected) {
		M0_ASSERT(repreb->sr_rlink.rlk_connected);
		conn_timeout = m0_time_from_now(SPIEL_CONN_TIMEOUT, 0);
		m0_rpc_link_disconnect_sync(&repreb->sr_rlink, conn_timeout);
		m0_rpc_link_fini(&repreb->sr_rlink);
	}

	return M0_RC(rc);
}

static bool _filter_objv(const struct m0_conf_obj *obj)
{
	M0_LOG(M0_DEBUG, FID_F, FID_P(&obj->co_id));
	return m0_conf_obj_type(obj) == &M0_CONF_OBJV_TYPE;
}

static int spiel_pool__device_collection_fill(struct _pool_cmd_ctx *ctx,
					      const struct m0_fid  *pool_fid)
{
	struct m0_confc      *confc = ctx->pl_spc->spc_confc;
	struct m0_conf_obj   *pool_obj = NULL;
	struct m0_conf_obj   *obj;
	struct m0_conf_diter  it;
	int                   rc;

	M0_ENTRY(FID_F, FID_P(pool_fid));

	rc = SPIEL_CONF_OBJ_FIND(confc, pool_fid, &pool_obj, _filter_pool,
				 M0_CONF_ROOT_POOLS_FID) ?:
	     m0_conf_diter_init(&it, confc, pool_obj,
				M0_CONF_POOL_PVERS_FID,
				M0_CONF_PVER_SITEVS_FID,
				M0_CONF_SITEV_RACKVS_FID,
				M0_CONF_RACKV_ENCLVS_FID,
				M0_CONF_ENCLV_CTRLVS_FID,
				M0_CONF_CTRLV_DRIVEVS_FID);
	if (rc != 0)
		goto leave;

	while ((rc = m0_conf_diter_next_sync(&it, _filter_objv))
							== M0_CONF_DIRNEXT) {
		obj = m0_conf_diter_result(&it);
		/* Pin obj to protect it from removal while being in use */
		m0_conf_obj_get_lock(obj);
		spiel_pool_device_collect(ctx, obj);
		m0_mutex_lock(&confc->cc_lock);
		m0_conf_obj_put(obj);
		m0_mutex_unlock(&confc->cc_lock);
	}
	m0_conf_diter_fini(&it);

leave:
	m0_confc_close(pool_obj);
	return M0_RC(rc);
}

static int spiel_pool_generic_handler(struct m0_spiel_core           *spc,
				      const struct m0_fid            *pool_fid,
				      const enum m0_cm_op             cmd,
				      struct m0_spiel_repreb_status **statuses,
				      enum m0_repreb_type             type)
{
	int                            rc;
	int                            service_count;
	int                            i;
	struct _pool_cmd_ctx           ctx;
	struct m0_fid_item            *si;
	bool                           cmd_status;
	struct m0_spiel_repreb_status *repreb_statuses = NULL;
	struct spiel_repreb           *repreb;
	struct m0_conf_obj            *svc_obj;

	M0_ENTRY();
	M0_PRE(pool_fid != NULL);
	M0_PRE(spc != NULL);
	M0_PRE(spc->spc_rmachine != NULL);

	if (m0_conf_fid_type(pool_fid) != &M0_CONF_POOL_TYPE)
		return M0_ERR(-EINVAL);

	spiel__pool_ctx_init(&ctx, spc, type);

	rc = spiel_pool__device_collection_fill(&ctx, pool_fid) ?:
		SPIEL_CONF_DIR_ITERATE(spc->spc_confc, &ctx,
				       spiel__pool_service_select,
				       M0_CONF_ROOT_NODES_FID,
				       M0_CONF_NODE_PROCESSES_FID,
				       M0_CONF_PROCESS_SERVICES_FID) ?:
		ctx.pl_rc;
	if (rc != 0)
		goto leave;

	cmd_status = M0_IN(cmd, (CM_OP_REPAIR_STATUS, CM_OP_REBALANCE_STATUS));
	service_count = m0_fids_tlist_length(&ctx.pl_services_fid);

	M0_ALLOC_ARR(repreb, service_count);
	if (repreb == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto leave;
	}
	if (cmd_status) {
		M0_ALLOC_ARR(repreb_statuses, service_count);
		if (repreb_statuses == NULL) {
			rc = M0_ERR(-ENOMEM);
			m0_free(repreb);
			goto leave;
		}
	}

	/* Synchronously send `cmd' to each service associated with the pool. */
	i = 0;
	m0_tl_for(m0_fids, &ctx.pl_services_fid, si) {
		/* Open m0_conf_service object. */
		rc = m0_confc_open_by_fid_sync(spc->spc_confc, &si->i_fid,
					       &svc_obj);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "confc_open failed; rc=%d service="
			       FID_F" i=%d", rc, FID_P(&si->i_fid), i);
			repreb[i++].sr_rc = rc;
			continue;
		}
		/* Is the service M0_NC_ONLINE? */
		if (svc_obj->co_ha_state != M0_NC_ONLINE) {
			rc = M0_ERR(-EINVAL);
			M0_LOG(M0_ERROR, "service "FID_F" is not online; i=%d"
			       " ha_state=%d", FID_P(&si->i_fid), i,
			       svc_obj->co_ha_state);
			m0_confc_close(svc_obj);
			repreb[i++].sr_rc = rc;
			continue;
		}
		repreb[i].sr_service = M0_CONF_CAST(svc_obj, m0_conf_service);
		M0_ASSERT(i < service_count);

		/* Send pool command to the service. */
		rc = spiel__pool_cmd_send(&ctx, cmd, &repreb[i]);

		m0_confc_close(svc_obj);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "pool command sending failed;"
			       " rc=%d service="FID_F" i=%d", rc,
			       FID_P(&si->i_fid), i);
			repreb[i++].sr_rc = rc;
			continue;
		}
		++i;
	} m0_tl_endfor;

	/*
	 * Sequentially wait for services to process pool command and return
	 * result.
	 */
	i = 0;
	m0_tl_for (m0_fids, &ctx.pl_services_fid, si) {
		rc = spiel__pool_cmd_status_get(&ctx, cmd, &repreb[i]);
		if (cmd_status) {
			repreb_statuses[i] = repreb[i].sr_status;
			M0_LOG(M0_DEBUG, "service"FID_F" status=%d",
					 FID_P(&si->i_fid),
					 repreb_statuses[i].srs_state);
		}
		++i;
		if (rc != 0)
			continue;
	} m0_tl_endfor;

	if (rc == 0 && cmd_status) {
		rc = i;
		*statuses = repreb_statuses;
		M0_LOG(M0_DEBUG, "array addr=%p size=%d", repreb_statuses, i);
	} else
		m0_free(repreb_statuses);

	m0_free(repreb);
leave:
	spiel__pool_ctx_fini(&ctx);
	return M0_RC(rc);
}

int m0_spiel_sns_repair_start(struct m0_spiel     *spl,
			      const struct m0_fid *pool_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_pool_generic_handler(&spl->spl_core, pool_fid,
						CM_OP_REPAIR, NULL,
						M0_REPREB_TYPE_SNS));
}
M0_EXPORTED(m0_spiel_sns_repair_start);

int m0_spiel_dix_repair_start(struct m0_spiel     *spl,
			      const struct m0_fid *pool_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_pool_generic_handler(&spl->spl_core, pool_fid,
						CM_OP_REPAIR, NULL,
						M0_REPREB_TYPE_DIX));
}
M0_EXPORTED(m0_spiel_dix_repair_start);

/** @todo Remove once Halon supports m0_spiel_{sns,dix}_repair_start(). */
int m0_spiel_pool_repair_start(struct m0_spiel     *spl,
			       const struct m0_fid *pool_fid)
{
	return m0_spiel_sns_repair_start(spl, pool_fid);
}

int m0_spiel_sns_repair_continue(struct m0_spiel     *spl,
				 const struct m0_fid *pool_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_pool_generic_handler(&spl->spl_core, pool_fid,
						CM_OP_REPAIR_RESUME, NULL,
						M0_REPREB_TYPE_SNS));
}
M0_EXPORTED(m0_spiel_sns_repair_continue);

int m0_spiel_dix_repair_continue(struct m0_spiel     *spl,
				 const struct m0_fid *pool_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_pool_generic_handler(&spl->spl_core, pool_fid,
						CM_OP_REPAIR_RESUME, NULL,
						M0_REPREB_TYPE_DIX));
}
M0_EXPORTED(m0_spiel_dix_repair_continue);

/** @todo Remove once Halon supports m0_spiel_{sns,dix}_repair_continue(). */
int m0_spiel_pool_repair_continue(struct m0_spiel     *spl,
				  const struct m0_fid *pool_fid)
{
	return m0_spiel_sns_repair_continue(spl, pool_fid);
}

int m0_spiel_sns_repair_quiesce(struct m0_spiel     *spl,
				const struct m0_fid *pool_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_pool_generic_handler(&spl->spl_core, pool_fid,
						CM_OP_REPAIR_QUIESCE, NULL,
						M0_REPREB_TYPE_SNS));
}
M0_EXPORTED(m0_spiel_sns_repair_quiesce);

int m0_spiel_dix_repair_quiesce(struct m0_spiel     *spl,
				const struct m0_fid *pool_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_pool_generic_handler(&spl->spl_core, pool_fid,
						CM_OP_REPAIR_QUIESCE, NULL,
						M0_REPREB_TYPE_DIX));
}
M0_EXPORTED(m0_spiel_dix_repair_quiesce);

/** @todo Remove once Halon supports m0_spiel_{sns,dix}_repair_quiesce(). */
int m0_spiel_pool_repair_quiesce(struct m0_spiel     *spl,
				 const struct m0_fid *pool_fid)
{
	return m0_spiel_sns_repair_quiesce(spl, pool_fid);
}

int m0_spiel_sns_repair_abort(struct m0_spiel     *spl,
			      const struct m0_fid *pool_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_pool_generic_handler(&spl->spl_core, pool_fid,
						CM_OP_REPAIR_ABORT, NULL,
						M0_REPREB_TYPE_SNS));
}
M0_EXPORTED(m0_spiel_sns_repair_abort);

int m0_spiel_dix_repair_abort(struct m0_spiel     *spl,
			      const struct m0_fid *pool_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_pool_generic_handler(&spl->spl_core, pool_fid,
						CM_OP_REPAIR_ABORT, NULL,
						M0_REPREB_TYPE_DIX));
}
M0_EXPORTED(m0_spiel_dix_repair_abort);

/** @todo Remove once Halon supports m0_spiel_{sns,dix}_repair_abort(). */
int m0_spiel_pool_repair_abort(struct m0_spiel     *spl,
			       const struct m0_fid *pool_fid)
{
	return m0_spiel_sns_repair_abort(spl, pool_fid);
}

int m0_spiel_sns_repair_status(struct m0_spiel                *spl,
			       const struct m0_fid            *pool_fid,
			       struct m0_spiel_repreb_status **statuses)
{
	int rc;

	M0_ENTRY();
	M0_PRE(statuses != NULL);
	rc = spiel_pool_generic_handler(&spl->spl_core, pool_fid,
					CM_OP_REPAIR_STATUS, statuses,
					M0_REPREB_TYPE_SNS);
	return rc >= 0 ? M0_RC(rc) : M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_sns_repair_status);

int m0_spiel_dix_repair_status(struct m0_spiel                *spl,
			       const struct m0_fid            *pool_fid,
			       struct m0_spiel_repreb_status **statuses)
{
	int rc;

	M0_ENTRY();
	M0_PRE(statuses != NULL);
	rc = spiel_pool_generic_handler(&spl->spl_core, pool_fid,
					CM_OP_REPAIR_STATUS, statuses,
					M0_REPREB_TYPE_DIX);
	return rc >= 0 ? M0_RC(rc) : M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_dix_repair_status);

/** @todo Remove once Halon supports m0_spiel_{sns,dix}_repair_status(). */
int m0_spiel_pool_repair_status(struct m0_spiel             *spl,
				const struct m0_fid         *pool_fid,
				struct m0_spiel_sns_status **statuses)
{
	return m0_spiel_sns_repair_status(spl, pool_fid,
				(struct m0_spiel_repreb_status **)statuses);
}

int m0_spiel_sns_rebalance_start(struct m0_spiel     *spl,
				 const struct m0_fid *pool_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_pool_generic_handler(&spl->spl_core, pool_fid,
						CM_OP_REBALANCE, NULL,
						M0_REPREB_TYPE_SNS));
}
M0_EXPORTED(m0_spiel_sns_rebalance_start);

int m0_spiel_dix_rebalance_start(struct m0_spiel     *spl,
				 const struct m0_fid *pool_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_pool_generic_handler(&spl->spl_core, pool_fid,
						CM_OP_REBALANCE, NULL,
						M0_REPREB_TYPE_DIX));
}
M0_EXPORTED(m0_spiel_dix_rebalance_start);

/** @todo Remove once Halon supports m0_spiel_{sns,dix}_rebalance_start(). */
int m0_spiel_pool_rebalance_start(struct m0_spiel     *spl,
			          const struct m0_fid *pool_fid)
{
	return m0_spiel_sns_rebalance_start(spl, pool_fid);
}

int m0_spiel_sns_rebalance_continue(struct m0_spiel     *spl,
				    const struct m0_fid *pool_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_pool_generic_handler(&spl->spl_core, pool_fid,
						CM_OP_REBALANCE_RESUME, NULL,
						M0_REPREB_TYPE_SNS));
}
M0_EXPORTED(m0_spiel_sns_rebalance_continue);

int m0_spiel_node_direct_rebalance_start(struct m0_spiel     *spl,
					 const struct m0_fid *node_fid)
{
	return M0_ERR_INFO(-EPERM, "Cannot start direct rebalance for "FID_F
                                    " operation not implemented", FID_P(node_fid));
}
M0_EXPORTED(m0_spiel_node_direct_rebalance_start);

int m0_spiel_dix_rebalance_continue(struct m0_spiel     *spl,
				    const struct m0_fid *pool_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_pool_generic_handler(&spl->spl_core, pool_fid,
						CM_OP_REBALANCE_RESUME, NULL,
						M0_REPREB_TYPE_DIX));
}
M0_EXPORTED(m0_spiel_dix_rebalance_continue);

/** @todo Remove once Halon supports m0_spiel_{sns,dix}_rebalance_continue(). */
int m0_spiel_pool_rebalance_continue(struct m0_spiel     *spl,
			             const struct m0_fid *pool_fid)
{
	return m0_spiel_sns_rebalance_continue(spl, pool_fid);
}

int m0_spiel_sns_rebalance_quiesce(struct m0_spiel     *spl,
				   const struct m0_fid *pool_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_pool_generic_handler(&spl->spl_core, pool_fid,
						CM_OP_REBALANCE_QUIESCE, NULL,
						M0_REPREB_TYPE_SNS));
}
M0_EXPORTED(m0_spiel_sns_rebalance_quiesce);

int m0_spiel_dix_rebalance_quiesce(struct m0_spiel     *spl,
				   const struct m0_fid *pool_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_pool_generic_handler(&spl->spl_core, pool_fid,
						CM_OP_REBALANCE_QUIESCE, NULL,
						M0_REPREB_TYPE_DIX));
}
M0_EXPORTED(m0_spiel_dix_rebalance_quiesce);

/** @todo Remove once Halon supports m0_spiel_{sns,dix}_rebalance_quiesce(). */
int m0_spiel_pool_rebalance_quiesce(struct m0_spiel     *spl,
			            const struct m0_fid *pool_fid)
{
	return m0_spiel_sns_rebalance_quiesce(spl, pool_fid);
}

int m0_spiel_sns_rebalance_status(struct m0_spiel                *spl,
				  const struct m0_fid            *pool_fid,
				  struct m0_spiel_repreb_status **statuses)
{
	int rc;

	M0_ENTRY();
	M0_PRE(statuses != NULL);
	rc = spiel_pool_generic_handler(&spl->spl_core, pool_fid,
					CM_OP_REBALANCE_STATUS, statuses,
					M0_REPREB_TYPE_SNS);
	return rc >= 0 ? M0_RC(rc) : M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_sns_rebalance_status);

int m0_spiel_dix_rebalance_status(struct m0_spiel                *spl,
				  const struct m0_fid            *pool_fid,
				  struct m0_spiel_repreb_status **statuses)
{
	int rc;

	M0_ENTRY();
	M0_PRE(statuses != NULL);
	rc = spiel_pool_generic_handler(&spl->spl_core, pool_fid,
					CM_OP_REBALANCE_STATUS, statuses,
					M0_REPREB_TYPE_DIX);
	return rc >= 0 ? M0_RC(rc) : M0_ERR(rc);
}
M0_EXPORTED(m0_spiel_dix_rebalance_status);

int m0_spiel_pool_rebalance_status(struct m0_spiel             *spl,
				   const struct m0_fid         *pool_fid,
				   struct m0_spiel_sns_status **statuses)
{
	return m0_spiel_sns_rebalance_status(spl, pool_fid,
			(struct m0_spiel_repreb_status **)statuses);
}

int m0_spiel_sns_rebalance_abort(struct m0_spiel     *spl,
				 const struct m0_fid *pool_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_pool_generic_handler(&spl->spl_core, pool_fid,
						CM_OP_REBALANCE_ABORT, NULL,
						M0_REPREB_TYPE_SNS));
}
M0_EXPORTED(m0_spiel_sns_rebalance_abort);

int m0_spiel_dix_rebalance_abort(struct m0_spiel     *spl,
				 const struct m0_fid *pool_fid)
{
	M0_ENTRY();
	return M0_RC(spiel_pool_generic_handler(&spl->spl_core, pool_fid,
						CM_OP_REBALANCE_ABORT, NULL,
						M0_REPREB_TYPE_DIX));
}
M0_EXPORTED(m0_spiel_dix_rebalance_abort);

/** @todo Remove once Halon supports m0_spiel_{sns,dix}_rebalance_abort(). */
int m0_spiel_pool_rebalance_abort(struct m0_spiel     *spl,
			          const struct m0_fid *pool_fid)
{
	return m0_spiel_sns_rebalance_abort(spl, pool_fid);
}

/***********************************************/
/*                 Byte count                  */
/***********************************************/

/**
 * List element of an individual key value pair on the byte count btree.
 */
struct m0_proc_data {
	struct m0_buf         pd_key;
	struct m0_buf         pd_rec;
	uint32_t              pd_kv_count;
	struct m0_tlink       pd_link;
	/** Not needed now, will be used in CORTX-28389 */
	/* uint64_t              pd_magic; */
};

/**
 * A context for fetching counters from an ioservice.
 */
struct spiel_proc_counter_item {
	/** Process fid */
	struct m0_fid         sci_fid;
	/** Rpc link for connect to a process */
	struct m0_rpc_link    sci_rlink;
	/** Process counter fetch request. */
	struct m0_fop         sci_fop;
	/**
	 * Signalled when session is established. @see
	 * m0_rpc_link_connect_async
	 */
	struct m0_clink       sci_rlink_wait;
	/**
	 * List of data received from an ioservice from each reply
	 * fop. Linked from m0_proc_data::pd_link. List will be
	 * implemented during retry mechanism task CORTX-28389.
	 */
	struct m0_tl         *sci_counters;
	/** temp field, till the counters list is implemented */
	struct m0_proc_data   sci_data;
	struct m0_spiel_core *sci_spc;
	struct m0_sm_ast      sci_ast;
	/** For waiting till reply fop is recived */
	struct m0_semaphore   sci_barrier;
	uint64_t              sci_magic;
	int                   sci_rc;
};

/****************************************************/
/*                    Filesystem                    */
/****************************************************/

/**
 * A context for asynchronous request to a process when fetching filesystem
 * statistics.
 */
struct spiel_proc_item {
	/** Process fid */
	struct m0_fid         spi_fid;
	/** Rpc link for connect to a process */
	struct m0_rpc_link    spi_rlink;
	/**
	 * Signalled when session is established. @see
	 * m0_rpc_link_connect_async
	 */
	struct m0_clink       spi_rlink_wait;
	/** Process health request. @see m0_ss_process_req */
	struct m0_fop         spi_fop;
	struct m0_sm_ast      spi_ast;
	struct m0_tlink       spi_link;
	struct _fs_stats_ctx *spi_ctx;
	/** A number of IO and MD services in the corresponding process */
	uint32_t              spi_svc_count;
	uint64_t              spi_magic;
};

M0_TL_DESCR_DEFINE(spiel_proc_items, "spiel_proc_items", static,
		    struct spiel_proc_item, spi_link, spi_magic,
		    M0_SPIEL_PROC_MAGIC, M0_SPIEL_PROC_HEAD_MAGIC);

M0_TL_DEFINE(spiel_proc_items, static, struct spiel_proc_item);

static inline void _fs_stats_ctx_lock(struct _fs_stats_ctx *fsx)
{
	m0_mutex_lock(&fsx->fx_guard);
}

static inline void _fs_stats_ctx_unlock(struct _fs_stats_ctx *fsx)
{
	m0_mutex_unlock(&fsx->fx_guard);
}

static void spiel_proc_item_postprocess(struct spiel_proc_item *proc)
{
	struct _fs_stats_ctx *fsx = proc->spi_ctx;

	_fs_stats_ctx_lock(fsx);
	fsx->fx_svc_processed += proc->spi_svc_count;
	if (fsx->fx_svc_processed == fsx->fx_svc_total)
		m0_semaphore_up(&fsx->fx_barrier);
	_fs_stats_ctx_unlock(fsx);
}

static bool spiel_proc_item_disconnect_cb(struct m0_clink *clink)
{
	struct spiel_proc_item *proc = M0_AMB(proc, clink, spi_rlink_wait);

	M0_ENTRY(FID_F, FID_P(&proc->spi_fid));
	spiel_proc_item_postprocess(proc);
	M0_LEAVE();
	return true;
}

static void spiel_process_disconnect_async(struct spiel_proc_item *proc)
{
	m0_time_t conn_timeout;

	M0_ENTRY(FID_F, FID_P(&proc->spi_fid));
	conn_timeout = m0_time_from_now(SPIEL_CONN_TIMEOUT, 0);
	m0_clink_init(&proc->spi_rlink_wait, spiel_proc_item_disconnect_cb);
	proc->spi_rlink_wait.cl_flags = M0_CF_ONESHOT;
	m0_rpc_link_disconnect_async(&proc->spi_rlink, conn_timeout,
				     &proc->spi_rlink_wait);
	M0_LEAVE();
}

static void spiel_process_health_replied_ast(struct m0_sm_group *grp,
					     struct m0_sm_ast   *ast)
{
	struct spiel_proc_item   *proc = M0_AMB(proc, ast, spi_ast);
	struct _fs_stats_ctx     *ctx = proc->spi_ctx;
	struct m0_rpc_item       *item = &proc->spi_fop.f_item;
	struct m0_ss_process_rep *rep;
	int                       rc;

	M0_ENTRY(FID_F, FID_P(&proc->spi_fid));
	rc = M0_FI_ENABLED("item_error") ? -EINVAL : m0_rpc_item_error(item);
	/**
	 * @todo Need to understand if it would make sense from consumer's
	 * standpoint to interrupt stats collection here on a network error.
	 */
	if (rc != 0)
		goto leave;
	rep = spiel_process_reply_data(&proc->spi_fop);
	if (rep->sspr_health >= M0_HEALTH_GOOD) {
		_fs_stats_ctx_lock(ctx);
		if (M0_FI_ENABLED("overflow") ||
		    m0_addu64_will_overflow(rep->sspr_free_seg,
					    ctx->fx_free_seg) ||
		    m0_addu64_will_overflow(rep->sspr_total_seg,
					    ctx->fx_total_seg) ||
		    m0_addu64_will_overflow(rep->sspr_free_disk,
					    ctx->fx_free_disk) ||
		    m0_addu64_will_overflow(rep->sspr_total_disk,
					    ctx->fx_total_disk)) {
			ctx->fx_rc = M0_ERR(-EOVERFLOW);
		} else {
			ctx->fx_free_seg    += rep->sspr_free_seg;
			ctx->fx_total_seg   += rep->sspr_total_seg;
			ctx->fx_free_disk   += rep->sspr_free_disk;
			ctx->fx_avail_disk  += rep->sspr_avail_disk;
			ctx->fx_total_disk  += rep->sspr_total_disk;
			ctx->fx_svc_replied += proc->spi_svc_count;
		}
		_fs_stats_ctx_unlock(ctx);
	}
leave:
	m0_rpc_machine_lock(ctx->fx_spc->spc_rmachine);
	m0_fop_put(&proc->spi_fop);
	m0_fop_fini(&proc->spi_fop);
	m0_rpc_machine_unlock(ctx->fx_spc->spc_rmachine);
	spiel_process_disconnect_async(proc);
	M0_LEAVE();
}

static void spiel_proc_item_disconnect_ast(struct m0_sm_group *grp,
					   struct m0_sm_ast   *ast)
{
	spiel_process_disconnect_async(container_of(ast, struct spiel_proc_item,
				       spi_ast));
}

static struct m0_sm_group* spiel_proc_sm_group(const struct spiel_proc_item *p)
{
	return p->spi_ctx->fx_spc->spc_confc->cc_group;
}

static void spiel_process_health_replied(struct m0_rpc_item *item)
{
	struct m0_fop          *fop  = m0_rpc_item_to_fop(item);
	struct spiel_proc_item *proc = M0_AMB(proc, fop, spi_fop);

	M0_ENTRY(FID_F, FID_P(&proc->spi_fid));
	proc->spi_ast.sa_cb = spiel_process_health_replied_ast;
	m0_sm_ast_post(spiel_proc_sm_group(proc), &proc->spi_ast);
	M0_LEAVE();
}

struct m0_rpc_item_ops spiel_process_health_ops = {
	.rio_replied = spiel_process_health_replied
};

static bool spiel_proc_item_rlink_cb(struct m0_clink *clink)
{
	struct spiel_proc_item   *proc = M0_AMB(proc, clink, spi_rlink_wait);
	struct m0_ss_process_req *req;
	struct _fs_stats_ctx     *ctx  = proc->spi_ctx;
	struct m0_fop            *fop = &proc->spi_fop;
	struct m0_rpc_item       *item;
	int                       rc;

	M0_ENTRY(FID_F, FID_P(&proc->spi_fid));
	m0_clink_fini(clink);
	if (proc->spi_rlink.rlk_rc != 0) {
		M0_LOG(M0_ERROR, "connect failed");
		goto proc_handled;
	}
	m0_fop_init(fop, &m0_fop_process_fopt, NULL, m0_fop_release);
	rc = M0_FI_ENABLED("alloc_fail") ? -ENOMEM : m0_fop_data_alloc(fop);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "fop data alloc failed");
		goto fop_fini;
	}
	fop->f_item.ri_rmachine = ctx->fx_spc->spc_rmachine;
	req                     = m0_ss_fop_process_req(fop);
	req->ssp_cmd            = M0_PROCESS_HEALTH;
	req->ssp_id             = proc->spi_fid;
	item                    = &fop->f_item;
	item->ri_ops            = &spiel_process_health_ops;
	item->ri_session        = &proc->spi_rlink.rlk_sess;
	item->ri_prio           = M0_RPC_ITEM_PRIO_MID;
	item->ri_nr_sent_max    = 5;
	m0_fop_get(fop);
	rc = M0_FI_ENABLED("rpc_post") ? -ENOTCONN : m0_rpc_post(item);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "rpc post failed");
		goto fop_put;
	}
	M0_LEAVE();
	return true;
fop_put:
	m0_fop_put_lock(fop);
fop_fini:
	m0_fop_fini(fop);
	proc->spi_ast.sa_cb = spiel_proc_item_disconnect_ast;
	m0_sm_ast_post(spiel_proc_sm_group(proc), &proc->spi_ast);
	return true;
proc_handled:
	spiel_proc_item_postprocess(proc);
	M0_LEAVE();
	return true;
}

static void spiel_process__health_async(struct _fs_stats_ctx    *fsx,
					struct spiel_proc_item  *proc)
{
	struct m0_conf_process *process;
	m0_time_t               conn_timeout;
	int                     rc;

	M0_ENTRY("proc fid "FID_F, FID_P(&proc->spi_fid));
	rc = M0_FI_ENABLED("obj_find") ? -ENOENT :
		spiel_proc_conf_obj_find(fsx->fx_spc, &proc->spi_fid, &process);
	if (rc != 0)
		goto err;
	m0_confc_close(&process->pc_obj);
	conn_timeout = m0_time_from_now(SPIEL_CONN_TIMEOUT, 0);
	rc = m0_rpc_link_init(&proc->spi_rlink, fsx->fx_spc->spc_rmachine, NULL,
			      process->pc_endpoint, SPIEL_MAX_RPCS_IN_FLIGHT);
	if (rc != 0)
		goto err;
	m0_clink_init(&proc->spi_rlink_wait, spiel_proc_item_rlink_cb);
	proc->spi_rlink_wait.cl_flags = M0_CF_ONESHOT;
	m0_rpc_link_connect_async(&proc->spi_rlink, conn_timeout,
				  &proc->spi_rlink_wait);
	M0_LEAVE();
	return;
err:
	spiel_proc_item_postprocess(proc);
	M0_ERR(rc);
}


static void spiel__fs_stats_ctx_init(struct _fs_stats_ctx *fsx,
				     struct m0_spiel_core *spc,
				     const struct m0_conf_obj_type *item_type)
{
	M0_SET0(fsx);
	fsx->fx_spc = spc;
	fsx->fx_type = item_type;
	m0_semaphore_init(&fsx->fx_barrier, 0);
	m0_mutex_init(&fsx->fx_guard);
	spiel_proc_items_tlist_init(&fsx->fx_items);
	M0_POST(spiel_proc_items_tlist_invariant(&fsx->fx_items));
}

static void spiel__fs_stats_ctx_fini(struct _fs_stats_ctx *fsx)
{
	struct spiel_proc_item *si;

	m0_tl_teardown(spiel_proc_items, &fsx->fx_items, si) {
		if (!M0_IS0(&si->spi_rlink))
			m0_rpc_link_fini(&si->spi_rlink);
		m0_free(si);
	}
	spiel_proc_items_tlist_fini(&fsx->fx_items);
	m0_mutex_fini(&fsx->fx_guard);
	m0_semaphore_fini(&fsx->fx_barrier);
}

/**
 * Callback provided to SPIEL_CONF_DIR_ITERATE. Intended for collecting fids
 * from configuration objects which type matches to the type specified in
 * collection context, i.e. process objects in this particular case. See
 * m0_spiel_filesystem_stats_fetch() where spiel__fs_stats_ctx_init() is done.
 */
static bool spiel__item_enlist(const struct m0_conf_obj *item, void *ctx)
{
	struct _fs_stats_ctx   *fsx = ctx;
	struct spiel_proc_item *si  = NULL;

	M0_LOG(SPIEL_LOGLVL, "arrived: " FID_F " (%s)", FID_P(&item->co_id),
	       m0_fid_type_getfid(&item->co_id)->ft_name);
	/* skip all but requested object types */
	if (m0_conf_obj_type(item) != fsx->fx_type)
		return true;
	if (!M0_FI_ENABLED("alloc fail"))
		M0_ALLOC_PTR(si);
	if (si == NULL) {
		fsx->fx_rc = M0_ERR(-ENOMEM);
		return false;
	}
	si->spi_fid = item->co_id;
	si->spi_ctx = fsx;
	spiel_proc_items_tlink_init_at(si, &fsx->fx_items);
	M0_LOG(SPIEL_LOGLVL, "* booked: " FID_F " (%s)", FID_P(&item->co_id),
	       m0_fid_type_getfid(&item->co_id)->ft_name);
	return true;
}

/**
 * Determines whether process with given fid should update fs stats.
 *
 * Process, in case it hosts MDS or IOS, and is M0_NC_ONLINE to the moment of
 * call, should update collected fs stats.
 */
static int spiel__proc_is_to_update_stats(struct spiel_proc_item *proc,
					  struct m0_confc        *confc)
{
	struct m0_conf_diter    it;
	struct m0_fid          *proc_fid = &proc->spi_fid;
	struct m0_conf_obj     *proc_obj;
	struct m0_conf_service *svc;
	int                     rc;

	M0_ENTRY("proc_fid = "FID_F, FID_P(proc_fid));
	M0_PRE(m0_conf_fid_type(proc_fid) == &M0_CONF_PROCESS_TYPE);

	rc = M0_FI_ENABLED("open_by_fid") ? -EINVAL :
			  m0_confc_open_by_fid_sync(confc, proc_fid, &proc_obj);
	if (rc != 0)
		return M0_ERR(rc);
	if (proc_obj->co_ha_state != M0_NC_ONLINE) {
		rc = M0_ERR(-EINVAL);
		goto obj_close;
	}
	rc = M0_FI_ENABLED("diter_init") ? -EINVAL :
			m0_conf_diter_init(&it, confc, proc_obj,
					   M0_CONF_PROCESS_SERVICES_FID);
	if (rc != 0)
		goto obj_close;
	while (m0_conf_diter_next_sync(&it, NULL) > 0) {
		svc = M0_CONF_CAST(m0_conf_diter_result(&it), m0_conf_service);
		if (M0_IN(svc->cs_type, (M0_CST_IOS, M0_CST_MDS, M0_CST_CAS)))
			M0_CNT_INC(proc->spi_svc_count);
	}
	if (proc->spi_svc_count != 0) {
		/*
		 * There is no point to update process' HA state unless
		 * the one is expected to host the required services.
		 */
		rc = M0_FI_ENABLED("ha_update") ? -EINVAL :
			m0_conf_obj_ha_update(proc_fid);
	}
	else {
		rc = -ENOENT; /* this case is normal, don't use M0_ERR() */
	}
	m0_conf_diter_fini(&it);
obj_close:
	m0_confc_close(proc_obj);
	return M0_RC(rc);
}

static void spiel_process_counter_replied_ast(struct m0_sm_group *grp,
					      struct m0_sm_ast   *ast)
{
	struct spiel_proc_counter_item *proc = M0_AMB(proc, ast, sci_ast);
	struct m0_rpc_item             *item = &proc->sci_fop.f_item;
	struct m0_ss_process_rep       *rep;
	m0_time_t                       conn_timeout;
	int                             rc;

	M0_ENTRY(FID_F, FID_P(&proc->sci_fid));
	rc = m0_rpc_item_error(item);
	if (rc != 0)
		goto leave;
	rep = spiel_process_reply_data(&proc->sci_fop);

	rc = m0_buf_alloc(&proc->sci_data.pd_key, rep->sspr_bckey.b_nob);
	if (rc != 0)
		goto leave;
	rc = m0_buf_alloc(&proc->sci_data.pd_rec, rep->sspr_bcrec.b_nob);
	if (rc != 0) {
		m0_buf_free(&proc->sci_data.pd_key);
		goto leave;
	}

	memcpy(proc->sci_data.pd_key.b_addr, rep->sspr_bckey.b_addr,
		rep->sspr_bckey.b_nob);
	memcpy(proc->sci_data.pd_rec.b_addr, rep->sspr_bcrec.b_addr,
		rep->sspr_bcrec.b_nob);
	proc->sci_data.pd_kv_count = rep->sspr_kv_count;

leave:
	m0_rpc_machine_lock(proc->sci_spc->spc_rmachine);
	m0_fop_put(&proc->sci_fop);
	m0_fop_fini(&proc->sci_fop);
	m0_rpc_machine_unlock(proc->sci_spc->spc_rmachine);

	conn_timeout = m0_time_from_now(SPIEL_CONN_TIMEOUT, 0);
	m0_rpc_link_disconnect_sync(&proc->sci_rlink, conn_timeout);
	m0_rpc_link_fini(&proc->sci_rlink);
	proc->sci_rc = rc;
	m0_semaphore_up(&proc->sci_barrier);
	M0_LEAVE();
}

static struct m0_sm_group* spiel_counter_sm_group(struct spiel_proc_counter_item *i)
{
	return i->sci_spc->spc_confc->cc_group;
}

static void spiel_process_counter_replied(struct m0_rpc_item *item)
{
	struct m0_fop                  *fop  = m0_rpc_item_to_fop(item);
	struct spiel_proc_counter_item *proc = M0_AMB(proc, fop, sci_fop);

	M0_ENTRY(FID_F, FID_P(&proc->sci_fid));
	proc->sci_ast.sa_cb = spiel_process_counter_replied_ast;
	m0_sm_ast_post(spiel_counter_sm_group(proc), &proc->sci_ast);
	M0_LEAVE();
}

struct m0_rpc_item_ops spiel_process_counter_ops = {
	.rio_replied = spiel_process_counter_replied
};

static bool spiel_proc_counter_item_rlink_cb(struct m0_clink *clink)
{
	struct spiel_proc_counter_item *proc = M0_AMB(proc, clink, sci_rlink_wait);
	struct m0_ss_process_req       *req;
	struct m0_fop                  *fop = &proc->sci_fop;
	struct m0_rpc_item             *item;
	m0_time_t                       conn_timeout;
	int                             rc;

	M0_ENTRY(FID_F, FID_P(&proc->sci_fid));
	m0_clink_fini(clink);
	if (proc->sci_rlink.rlk_rc != 0) {
		M0_LOG(M0_ERROR, "connect failed");
		proc->sci_rc = proc->sci_rlink.rlk_rc;
		m0_semaphore_up(&proc->sci_barrier);
		goto ret;
	}
	m0_fop_init(fop, &m0_fop_process_fopt, NULL, m0_fop_release);
	rc = m0_fop_data_alloc(fop);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "fop data alloc failed");
		goto fop_fini;
	}

	fop->f_item.ri_rmachine = proc->sci_spc->spc_rmachine;
	req                     = m0_ss_fop_process_req(fop);
	req->ssp_cmd            = M0_PROCESS_COUNTER;
	req->ssp_id             = proc->sci_fid;
	item                    = &fop->f_item;
	item->ri_ops            = &spiel_process_counter_ops;
	item->ri_session        = &proc->sci_rlink.rlk_sess;
	item->ri_prio           = M0_RPC_ITEM_PRIO_MID;
	item->ri_nr_sent_max    = 5;
	m0_fop_get(fop);
	rc = m0_rpc_post(item);	
	if (rc != 0) {
		M0_LOG(M0_ERROR, "rpc post failed");
		m0_fop_put_lock(fop);
		goto fop_fini;
	}

	proc->sci_rc = rc;
	goto ret;
	
fop_fini:
	m0_fop_fini(fop);
	conn_timeout = m0_time_from_now(SPIEL_CONN_TIMEOUT, 0);
	m0_rpc_link_disconnect_sync(&proc->sci_rlink, conn_timeout);
	m0_rpc_link_fini(&proc->sci_rlink);
	proc->sci_rc = rc;
	m0_semaphore_up(&proc->sci_barrier);
	M0_LEAVE();
	return true;

ret:
	M0_LEAVE();
	return true;
}

static int spiel_process__counters_async(struct spiel_proc_counter_item *proc)
{
	struct m0_conf_process *process;
	struct m0_spiel_core   *spc = proc->sci_spc;
	m0_time_t               conn_timeout;
	int                     rc;

	M0_ENTRY("proc fid "FID_F, FID_P(&proc->sci_fid));
	rc = spiel_proc_conf_obj_find(spc, &proc->sci_fid, &process);
	if (rc != 0)
		return M0_ERR(rc);
	m0_confc_close(&process->pc_obj);
	conn_timeout = m0_time_from_now(SPIEL_CONN_TIMEOUT, 0);
	rc = m0_rpc_link_init(&proc->sci_rlink, spc->spc_rmachine, NULL,
			      process->pc_endpoint, SPIEL_MAX_RPCS_IN_FLIGHT);
	if (rc != 0)
		return M0_ERR(rc);

	m0_clink_init(&proc->sci_rlink_wait, spiel_proc_counter_item_rlink_cb);
	proc->sci_rlink_wait.cl_is_oneshot = true;
	m0_rpc_link_connect_async(&proc->sci_rlink, conn_timeout,
				  &proc->sci_rlink_wait);
	return M0_RC(rc);
}

static void m0_spiel_count_stats_free(struct m0_proc_counter *count_stats,
			       int                     failed_index)
{
	int i;

	for (i = 0; i < failed_index; i++) {
		if (count_stats->pc_bckey[i] != NULL)
			m0_free(count_stats->pc_bckey[i]);
		if (count_stats->pc_bcrec[i] != NULL)
			m0_free(count_stats->pc_bcrec[i]);
	}
}

int m0_spiel_count_stats_init(struct m0_proc_counter **count_stats)
{
	int rc = 0;

	M0_PRE(*count_stats == NULL);
	M0_ALLOC_PTR(*count_stats);
	if (*count_stats == NULL)
		rc = M0_ERR(-ENOMEM);
	return M0_RC(rc);
}
M0_EXPORTED(m0_spiel_count_stats_init);

void m0_spiel_count_stats_fini(struct m0_proc_counter *count_stats)
{
	if (count_stats != NULL) { 
		if (count_stats->pc_cnt > 0)
			m0_spiel_count_stats_free(count_stats,
						  count_stats->pc_cnt);
		m0_free(count_stats->pc_bckey);
		m0_free(count_stats->pc_bcrec);
		m0_free(count_stats);
	}
}
M0_EXPORTED(m0_spiel_count_stats_fini);

int m0_spiel_proc_counters_fetch(struct m0_spiel        *spl,
				 struct m0_fid          *proc_fid,
				 struct m0_proc_counter *count_stats)
{
	struct spiel_proc_counter_item *proc = NULL;
	struct m0_spiel_core           *spc = &spl->spl_core;
	struct m0_conf_obj             *proc_obj;
	struct m0_spiel_bckey          *key_cur;
	struct m0_spiel_bcrec          *rec_cur;
	int                             rc = 0;
	int                             i;

	M0_ENTRY();
	M0_PRE(spl != NULL);
	M0_PRE(spc->spc_confc != NULL);
	M0_PRE(count_stats != NULL);

	if (!m0_confc_is_inited(spc->spc_confc))
		return M0_ERR_INFO(-EAGAIN, "confc is finalised");

	rc = m0_confc_open_by_fid_sync(spc->spc_confc, proc_fid, &proc_obj);
	if (rc != 0)
		return M0_ERR(rc);
	if (proc_obj->co_ha_state != M0_NC_ONLINE) {
		rc = M0_ERR(-EPERM);
		goto obj_close;
	}

	M0_ALLOC_PTR(proc);
	if (proc == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto obj_close;
	}

	m0_semaphore_init(&proc->sci_barrier, 0);
	proc->sci_fid = *proc_fid;
	proc->sci_spc =  spc;

      /* List will be implemented when adding retry mechanism CORTX-28389. */
	/*
	M0_TL_DESCR_DEFINE(proc_counter, "proc-counter", M0_INTERNAL,
			   struct m0_proc_data, pd_link, pd_magic,
			   M0_NET_BUFFER_LINK_MAGIC, M0_NET_BUFFER_HEAD_MAGIC);
	M0_TL_DEFINE(proc_counter, M0_INTERNAL, struct m0_proc_data);
	*/
	rc = spiel_process__counters_async(proc);
	if (rc != 0)
		goto sem_fini;
	m0_semaphore_down(&proc->sci_barrier);

	count_stats->pc_proc_fid = *proc_fid;
	count_stats->pc_rc = proc->sci_rc;
	if (proc->sci_rc != 0) {
                /* rpc link is initialized in
		 * spiel_process__counters_async()
		 */
                m0_rpc_link_fini(&proc->sci_rlink);
		rc = proc->sci_rc;
		goto sem_fini;
	}

	count_stats->pc_cnt = proc->sci_data.pd_kv_count;
	M0_ALLOC_ARR(count_stats->pc_bckey, proc->sci_data.pd_kv_count);
	if (count_stats->pc_bckey == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto buf_free;
	}
	M0_ALLOC_ARR(count_stats->pc_bcrec, proc->sci_data.pd_kv_count);
	if (count_stats->pc_bcrec == NULL) {
		m0_free(count_stats->pc_bckey);
		rc = M0_ERR(-ENOMEM);
		goto buf_free;
	}

	key_cur = (struct m0_spiel_bckey *)proc->sci_data.pd_key.b_addr;
	rec_cur = (struct m0_spiel_bcrec *)proc->sci_data.pd_rec.b_addr;

	for (i = 0; i < proc->sci_data.pd_kv_count; i++) {
		M0_ALLOC_PTR(count_stats->pc_bckey[i]);
		M0_ALLOC_PTR(count_stats->pc_bcrec[i]);
		if (count_stats->pc_bckey[i] == NULL ||
		    count_stats->pc_bcrec[i] == NULL) {
			/* until the failed index we need to free */
			m0_spiel_count_stats_free(count_stats, i+1);
			rc = M0_ERR(-ENOMEM);
			goto buf_free;
		}

		memcpy(count_stats->pc_bckey[i], key_cur,
			sizeof(struct m0_spiel_bckey));
		memcpy(count_stats->pc_bcrec[i], rec_cur,
			sizeof(struct m0_spiel_bcrec));

		key_cur++;
		rec_cur++;
	}

buf_free:
	m0_buf_free(&proc->sci_data.pd_key);
	m0_buf_free(&proc->sci_data.pd_rec);
sem_fini:
	m0_semaphore_fini(&proc->sci_barrier);
obj_close:
	m0_confc_close(proc_obj);
	m0_free(proc);
	return M0_RC(rc);
}
M0_EXPORTED(m0_spiel_proc_counters_fetch);

M0_INTERNAL int m0_spiel__fs_stats_fetch(struct m0_spiel_core *spc,
					 struct m0_fs_stats   *stats)
{
	struct _fs_stats_ctx    fsx;
	struct spiel_proc_item *proc;
	int                     rc;

	M0_ENTRY();
	M0_PRE(spc != NULL);
	M0_PRE(spc->spc_confc != NULL);
	M0_PRE(stats != NULL);

	if (!m0_confc_is_inited(spc->spc_confc))
		return M0_ERR_INFO(-EAGAIN, "confc is finalised");

	spiel__fs_stats_ctx_init(&fsx, spc, &M0_CONF_PROCESS_TYPE);
	/* walk along the filesystem nodes and get to process level */
	rc = SPIEL_CONF_DIR_ITERATE(spc->spc_confc, &fsx,
				    spiel__item_enlist,
				    M0_CONF_ROOT_NODES_FID,
				    M0_CONF_NODE_PROCESSES_FID) ?:
		fsx.fx_rc;
	if (rc != 0)
		goto end;
	/* update stats by the list of found processes */
	m0_tl_for(spiel_proc_items, &fsx.fx_items, proc) {
		rc = spiel__proc_is_to_update_stats(proc, spc->spc_confc);
		if (rc != 0) {
			rc = 0;
			spiel_proc_items_tlist_del(proc);
			m0_free(proc);
			continue;
		}
		fsx.fx_svc_total += proc->spi_svc_count;
	} m0_tl_endfor;
	m0_tl_for(spiel_proc_items, &fsx.fx_items, proc) {
		spiel_process__health_async(&fsx, proc);
	} m0_tl_endfor;
	m0_semaphore_down(&fsx.fx_barrier);
	_fs_stats_ctx_lock(&fsx);
	M0_ASSERT(fsx.fx_svc_processed == fsx.fx_svc_total &&
		  fsx.fx_svc_replied <= fsx.fx_svc_total);
	/* report stats to consumer */
	*stats = (struct m0_fs_stats) {
		.fs_free_seg = fsx.fx_free_seg,
		.fs_total_seg = fsx.fx_total_seg,
		.fs_free_disk = fsx.fx_free_disk,
		.fs_avail_disk = fsx.fx_avail_disk,
		.fs_total_disk = fsx.fx_total_disk,
		.fs_svc_total = fsx.fx_svc_total,
		.fs_svc_replied = fsx.fx_svc_replied,
	};
	_fs_stats_ctx_unlock(&fsx);
end:
	spiel__fs_stats_ctx_fini(&fsx);
	return M0_RC(rc ? rc : fsx.fx_rc);
}
M0_EXPORTED(m0_spiel_filesystem_stats_fetch);

int m0_spiel_filesystem_stats_fetch(struct m0_spiel    *spl,
				    struct m0_fs_stats *stats)
{
	return m0_spiel__fs_stats_fetch(&spl->spl_core, stats);
}

int m0_spiel_confstr(struct m0_spiel *spl, char **out)
{
	char *confstr;
	int   rc;

	rc = m0_conf_cache_to_string(&spiel_confc(spl)->cc_cache, &confstr,
				     false);
	if (rc != 0)
		return M0_ERR(rc);
	*out = m0_strdup(confstr);
	if (*out == NULL)
		rc = M0_ERR(-ENOMEM);
	m0_confx_string_free(confstr);
	return M0_RC(rc);
}
M0_EXPORTED(m0_spiel_confstr);

int m0_spiel_conf_pver_status(struct m0_spiel          *spl,
			      struct m0_fid            *fid,
			      struct m0_conf_pver_info *out_info)
{
	struct m0_confc *confc;

	M0_ENTRY();
	M0_PRE(spl != NULL);

	confc = spl->spl_core.spc_confc;
	if (!m0_confc_is_inited(confc))
		return M0_ERR_INFO(-EAGAIN, "confc is finalised");

	return M0_RC(m0_conf_pver_status(fid, confc, out_info));
}
M0_EXPORTED(m0_spiel_conf_pver_status);
/** @} */
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
