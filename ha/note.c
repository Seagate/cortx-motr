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


/**
 * @defgroup ha-note HA notification
 *
 * TODO handle memory allocation failure in m0_ha_note_handler_add()
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "conf/confc.h"
#include "fop/fom_generic.h"
#include "fop/fop.h"
#include "lib/chan.h"
#include "lib/memory.h"
#include "lib/trace.h"
#include "lib/finject.h"
#include "rpc/rpc_internal.h"   /* m0_rpc__post_locked */
#include "rpc/rpclib.h"
#include "rpc/session.h"
#include "fid/fid.h"            /* FID_F */
#include "module/instance.h"    /* m0_get */

#include "ha/note.h"
#include "ha/note_fops.h"
#include "ha/note_xc.h"
#include "ha/msg.h"             /* m0_ha_msg */
#include "ha/link.h"            /* m0_ha_link_send */
#include "ha/ha.h"              /* m0_ha_send */
#include "motr/ha.h"            /* m0_motr_ha */
#include "conf/obj_ops.h"       /* m0_conf_obj_find */

/**
 * @see: confc_fop_release()
 */
static bool note_invariant(const struct m0_ha_nvec *note, bool known)
{
#define N(i) (note->nv_note[i])
	return m0_forall(i, note->nv_nr,
			 _0C((N(i).no_state != M0_NC_UNKNOWN) == known) &&
			 _0C(!m0_fid_is_set(&N(i).no_id) ||
			     m0_conf_fid_is_valid(&N(i).no_id)) &&
			 _0C(ergo(M0_IN(N(i).no_state,
					(M0_NC_REPAIR, M0_NC_REBALANCE)),
			 m0_conf_fid_type(&N(i).no_id) == &M0_CONF_POOL_TYPE ||
			 m0_conf_fid_type(&N(i).no_id) == &M0_CONF_SDEV_TYPE ||
			 m0_conf_fid_type(&N(i).no_id) == &M0_CONF_NODE_TYPE ||
			 m0_conf_fid_type(&N(i).no_id) == &M0_CONF_DRIVE_TYPE)));
#undef N
}

M0_INTERNAL int m0_ha_state_get(struct m0_ha_nvec *note, struct m0_chan *chan)
{

	uint64_t id_of_get;

	M0_ENTRY("chan=%p note->nv_nr=%"PRIi32" note->nv_note[0].no_id="FID_F
	         " note->nv_note[0].no_state=%u", chan, note->nv_nr,
	         FID_P(note->nv_nr > 0 ? &note->nv_note[0].no_id : &M0_FID0),
	         note->nv_nr > 0 ? note->nv_note[0].no_state : 0);
	M0_PRE(note_invariant(note, false));
	M0_NVEC_PRINT(note, " > ", M0_DEBUG);

	if (m0_get()->i_note_handler == NULL)
		return M0_ERR(-EINVAL);

	id_of_get = m0_ha_note_handler_add(m0_get()->i_note_handler,
	                                   note, chan);
	m0_ha_msg_nvec_send(note, id_of_get, false, M0_HA_NVEC_GET, NULL);
	return M0_RC(0);
}

M0_INTERNAL void m0_ha_state_set(const struct m0_ha_nvec *note)
{
	M0_ENTRY("note->nv_nr=%"PRIi32" note->nv_note[0].no_id="FID_F
	         " note->nv_note[0].no_state=%u", note->nv_nr,
	         FID_P(note->nv_nr > 0 ? &note->nv_note[0].no_id : &M0_FID0),
	         note->nv_nr > 0 ? note->nv_note[0].no_state : 0);
	M0_PRE(note_invariant(note, true));
	m0_ha_msg_nvec_send(note, 0, false, M0_HA_NVEC_SET, NULL);
}

M0_INTERNAL void m0_ha_local_state_set(const struct m0_ha_nvec *nvec)
{
	if (!M0_FI_ENABLED("no_ha"))
		m0_ha_state_set(nvec);
}

static void ha_state_single_fop_data_free(struct m0_fop *fop)
{
	struct m0_ha_state_single *hss;
	struct m0_ha_nvec         *nvec = fop->f_data.fd_data;

	M0_PRE(nvec != NULL);
	hss = container_of(nvec, struct m0_ha_state_single, hss_nvec);
	m0_free(hss);
	fop->f_data.fd_data = NULL;
}

static void ha_state_single_replied(struct m0_rpc_item *item)
{
	struct m0_fop *fop = container_of(item, struct m0_fop, f_item);

	ha_state_single_fop_data_free(fop);
	m0_fop_put(fop);
}

struct m0_rpc_item_ops ha_ri_ops = {
	.rio_replied = ha_state_single_replied,
};

M0_INTERNAL void m0_ha_state_single_post(struct m0_ha_nvec *nvec)
{
	M0_ENTRY();
	M0_PRE(nvec != NULL);
	M0_PRE(note_invariant(nvec, true));

	m0_ha_msg_nvec_send(nvec, 0, false, M0_HA_NVEC_SET, NULL);
}

/**
 * Callback used in m0_ha_state_accept(). Updates HA states for particular confc
 * instance during iteration through HA clients list.
 *
 * For internal details see comments provided for m0_ha_state_accept().
 */
static void ha_state_accept(struct m0_confc         *confc,
			    const struct m0_ha_nvec *note,
			    uint64_t                 ignore_same_state)
{
	struct m0_conf_obj   *obj;
	struct m0_conf_cache *cache;
	enum m0_ha_obj_state  prev_ha_state;
	int                   i;
	int		      rc = 0;

	M0_ENTRY("confc=%p note->nv_nr=%"PRIi32, confc, note->nv_nr);
	M0_PRE(note_invariant(note, true));

	/*
	* @todo check if this can be avoided or if it is masking the
	* real problem here.
	*/
	if (!m0_confc_is_inited(confc))
		return;

	cache = &confc->cc_cache;
	m0_conf_cache_lock(cache);
	for (i = 0; i < note->nv_nr; ++i) {
		obj = m0_conf_cache_lookup(cache, &note->nv_note[i].no_id);
		M0_LOG(M0_DEBUG, "nv_note[%d]=(no_id="FID_F" no_state=%"PRIu32
		       ") obj=%p obj->co_status=%d", i,
		       FID_P(&note->nv_note[i].no_id),
		       note->nv_note[i].no_state,
		       obj, obj == NULL ? -1 : obj->co_status);
		if (obj != NULL && obj->co_status == M0_CS_READY) {
			prev_ha_state = obj->co_ha_state;
			obj->co_ha_state = note->nv_note[i].no_state;
			if (!ignore_same_state ||
			    prev_ha_state != obj->co_ha_state)
				m0_chan_broadcast(&obj->co_ha_chan);
		} else if ((obj == NULL) &&
		           (m0_fid_tget(&note->nv_note[i].no_id) == 'r')) {
				M0_LOG(M0_DEBUG, FID_F" is process FID and not found in conc",
					FID_P(&note->nv_note[i].no_id));
				M0_LOG(M0_DEBUG,"Add the entry to conf cache");
				rc = m0_conf_obj_find(cache, &note->nv_note[i].no_id, 
						&obj);
				if (rc == 0) {
					obj->co_status = M0_CS_READY;
					prev_ha_state = obj->co_ha_state;
					obj->co_ha_state = note->nv_note[i].no_state;
					if (!ignore_same_state ||
					    prev_ha_state != obj->co_ha_state)
						m0_chan_broadcast(&obj->co_ha_chan);
					M0_LOG(M0_DEBUG, "confs obj added successfully rc= %d", rc);
				} else {
					M0_LOG(M0_DEBUG, "confs obj add failed rc= %d", rc);
				}
		}
	}
	m0_conf_cache_unlock(cache);
	M0_LEAVE();
}

M0_INTERNAL void m0_ha_state_accept(const struct m0_ha_nvec *note,
				    bool                     ignore_same_state)
{
	m0_ha_clients_iterate((m0_ha_client_cb_t)ha_state_accept, note,
			      ignore_same_state);
}

M0_INTERNAL void m0_ha_msg_accept(const struct m0_ha_msg *msg,
                                  struct m0_ha_link      *hl)
{
	const struct m0_ha_msg_nvec *nvec_req;
	struct m0_confc             *confc;
	struct m0_conf_cache        *cache;
	struct m0_ha_nvec            nvec;
	struct m0_conf_obj          *obj;
	struct m0_fid                obj_fid;
	int                          i;

	if (msg->hm_data.hed_type != M0_HA_MSG_NVEC)
		return;
	M0_PRE(M0_IN(msg->hm_data.u.hed_nvec.hmnv_ignore_same_state,
		     (false, true)));

	nvec = (struct m0_ha_nvec){
		.nv_nr   = msg->hm_data.u.hed_nvec.hmnv_nr,
	};
	M0_LOG(M0_DEBUG, "nvec nv_nr=%"PRIu32" hmvn_type=%s", nvec.nv_nr,
	       msg->hm_data.u.hed_nvec.hmnv_type == M0_HA_NVEC_SET ?  "SET" :
	       msg->hm_data.u.hed_nvec.hmnv_type == M0_HA_NVEC_GET ?  "GET" :
								    "UNKNOWN!");
	M0_ALLOC_ARR(nvec.nv_note, nvec.nv_nr);
	M0_ASSERT(nvec.nv_note != NULL);
	for (i = 0; i < nvec.nv_nr; ++i) {
		nvec.nv_note[i] = msg->hm_data.u.hed_nvec.hmnv_arr.hmna_arr[i];
		M0_LOG(M0_DEBUG, "nv_note[%d]=(no_id="FID_F" "
		       "no_state=%"PRIu32")", i, FID_P(&nvec.nv_note[i].no_id),
		       nvec.nv_note[i].no_state);
	}
	if (msg->hm_data.u.hed_nvec.hmnv_type == M0_HA_NVEC_SET) {
		m0_ha_state_accept(&nvec,
			   msg->hm_data.u.hed_nvec.hmnv_ignore_same_state);
		if (msg->hm_data.u.hed_nvec.hmnv_id_of_get != 0) {
			m0_ha_note_handler_signal(
				  m0_get()->i_note_handler, &nvec,
				  msg->hm_data.u.hed_nvec.hmnv_id_of_get);
		}
	} else if (M0_FI_ENABLED("invalid_confc")){
		nvec_req = &msg->hm_data.u.hed_nvec;
		for (i = 0; i < nvec_req->hmnv_nr; ++i) {
			obj_fid = nvec_req->hmnv_arr.hmna_arr[i].no_id;
			nvec.nv_note[i] = (struct m0_ha_note){
				.no_id    = obj_fid,
				.no_state = M0_NC_ONLINE,
			};
		}
		m0_ha_msg_nvec_send(&nvec,
				    msg->hm_data.u.hed_nvec.hmnv_id_of_get,
				    false,
				    M0_HA_NVEC_SET, hl);
	} else {
		confc = m0_reqh2confc(hl->hln_cfg.hlc_reqh);
		cache = &confc->cc_cache;
		nvec_req = &msg->hm_data.u.hed_nvec;
		m0_conf_cache_lock(cache);
		for (i = 0; i < nvec_req->hmnv_nr; ++i) {
			obj_fid = nvec_req->hmnv_arr.hmna_arr[i].no_id;
			obj = m0_conf_cache_lookup(cache, &obj_fid);
			if (obj == NULL) {
				M0_LOG(M0_DEBUG, "obj == NULL");
				nvec.nv_note[i] = (struct m0_ha_note){
					.no_id    = obj_fid,
					.no_state = M0_NC_ONLINE,
				};
			} else {
				nvec.nv_note[i] = (struct m0_ha_note){
					.no_id    = obj->co_id,
					.no_state = obj->co_ha_state,
				};
			}
		}
		m0_conf_cache_unlock(cache);
		m0_ha_msg_nvec_send(&nvec,
		                    msg->hm_data.u.hed_nvec.hmnv_id_of_get,
				    false,
		                    M0_HA_NVEC_SET, hl);
	}
	m0_free(nvec.nv_note);
}

M0_INTERNAL uint64_t
m0_ha_msg_nvec_send(const struct m0_ha_nvec *nvec,
		    uint64_t                 id_of_get,
		    bool                     ignore_same_state,
		    int                      direction,
		    struct m0_ha_link       *hl)
{
	struct m0_ha_msg *msg;
	uint64_t          tag;

	if (hl == NULL)
		hl = m0_get()->i_ha_link;
	if (hl == NULL) {
		M0_LOG(M0_WARN, "hl == NULL");
		return 0;
	}
	M0_ALLOC_PTR(msg);
	M0_ASSERT(msg != NULL);
	*msg = (struct m0_ha_msg){
		.hm_data = {
			.hed_type   = M0_HA_MSG_NVEC,
			.u.hed_nvec = {
				.hmnv_type              = direction,
				.hmnv_id_of_get         = id_of_get,
				.hmnv_ignore_same_state = ignore_same_state,
				.hmnv_nr                = nvec->nv_nr,
			},
		},
	};
	M0_ASSERT(nvec->nv_nr > 0 &&
		  nvec->nv_nr <=
		  ARRAY_SIZE(msg->hm_data.u.hed_nvec.hmnv_arr.hmna_arr));
	memcpy(msg->hm_data.u.hed_nvec.hmnv_arr.hmna_arr, nvec->nv_note,
	       nvec->nv_nr * sizeof(nvec->nv_note[0]));
	m0_ha_link_send(hl, msg, &tag);
	m0_free(msg);

	return tag;
}

struct ha_note_handler_request {
	struct m0_chan    *hsg_wait_chan;
	struct m0_tlink    hsg_tlink;
	struct m0_ha_nvec *hsg_nvec;
	uint64_t           hsg_magic;
	uint64_t           hsg_id;
};

M0_TL_DESCR_DEFINE(ha_gets, "m0_ha_note_handler::hmh_gets", static,
		   struct ha_note_handler_request, hsg_tlink, hsg_magic,
		   20, 21);               /* XXX */
M0_TL_DEFINE(ha_gets, static, struct ha_note_handler_request);

static void ha_note_handler_msg(struct m0_ha_handler *hh,
                                struct m0_ha         *ha,
                                struct m0_ha_link    *hl,
                                struct m0_ha_msg     *msg,
                                uint64_t              tag,
                                void                 *data)
{
	struct m0_ha_note_handler *hnh;

	hnh = container_of(hh, struct m0_ha_note_handler, hnh_handler);
	M0_ASSERT(hnh == data);
	m0_ha_msg_accept(msg, hl);
}

M0_INTERNAL int m0_ha_note_handler_init(struct m0_ha_note_handler *hnh,
                                        struct m0_ha_dispatcher   *hd)
{
	M0_PRE(M0_IS0(hnh));

	m0_mutex_init(&hnh->hnh_lock);
	ha_gets_tlist_init(&hnh->hnh_gets);
	hnh->hnh_dispatcher = hd;
	hnh->hnh_handler = (struct m0_ha_handler){
		.hh_data            = hnh,
		.hh_msg_received_cb = &ha_note_handler_msg,
	};
	hnh->hnh_id_of_get = 100;
	m0_ha_dispatcher_attach(hnh->hnh_dispatcher, &hnh->hnh_handler);
	M0_ASSERT(m0_get()->i_note_handler == NULL);
	m0_get()->i_note_handler = hnh;
	return 0;
}

M0_INTERNAL void m0_ha_note_handler_fini(struct m0_ha_note_handler *hnh)
{
	struct ha_note_handler_request *hsg;

	M0_ASSERT(m0_get()->i_note_handler == hnh);
	m0_mutex_lock(&hnh->hnh_lock);
	m0_tl_for (ha_gets, &hnh->hnh_gets, hsg) {
		M0_LOG(M0_WARN, "handler %p del id=%"PRIu64, hsg, hsg->hsg_id);
		ha_gets_tlink_del_fini(hsg);
	} m0_tl_endfor;
	m0_mutex_unlock(&hnh->hnh_lock);

	m0_get()->i_note_handler = NULL;
	m0_ha_dispatcher_detach(hnh->hnh_dispatcher, &hnh->hnh_handler);
	ha_gets_tlist_fini(&hnh->hnh_gets);
	m0_mutex_fini(&hnh->hnh_lock);
}

M0_INTERNAL uint64_t m0_ha_note_handler_add(struct m0_ha_note_handler *hnh,
                                            struct m0_ha_nvec         *nvec_req,
                                            struct m0_chan            *chan)
{
	struct ha_note_handler_request *hsg;

	M0_ALLOC_PTR(hsg);
	m0_mutex_lock(&hnh->hnh_lock);
	*hsg = (struct ha_note_handler_request){
		.hsg_wait_chan = chan,
		.hsg_nvec      = nvec_req,
		.hsg_id        = hnh->hnh_id_of_get++,
	};
	ha_gets_tlink_init_at_tail(hsg, &hnh->hnh_gets);
	m0_mutex_unlock(&hnh->hnh_lock);
	M0_LOG(M0_DEBUG, "add id=%"PRIu64, hsg->hsg_id);
	return hsg->hsg_id;
}

M0_INTERNAL void m0_ha_note_handler_signal(struct m0_ha_note_handler *hnh,
                                           struct m0_ha_nvec         *nvec_rep,
                                           uint64_t                   id)
{
	struct ha_note_handler_request *hsg;
	int                             i;

	M0_LOG(M0_DEBUG, "signal id=%"PRIu64, id);
	/*
	 * @todo For LDR-R1 only two confd's are present and after a confd failure
	 * quorum is not possible in rconfc. During shutdown ha dispatcher is
	 * already added to the list and not cleaned up due rconfc not able to
	 * elect the confd and this function is not called. In this case it is
	 * found that some pending elements are in the ha_gets list, which
	 * causes panic during ha_gets list finalisation.
	 */
	m0_mutex_lock(&hnh->hnh_lock);
	hsg = m0_tl_find(ha_gets, hsg1, &hnh->hnh_gets, id == hsg1->hsg_id);
	M0_ASSERT_INFO(hsg != NULL, "id=%"PRIu64, id);
	ha_gets_tlink_del_fini(hsg);
	m0_mutex_unlock(&hnh->hnh_lock);
	M0_ASSERT(nvec_rep->nv_nr == hsg->hsg_nvec->nv_nr);
	for (i = 0; i < nvec_rep->nv_nr; ++i) {
		M0_ASSERT(m0_fid_eq(&nvec_rep->nv_note[i].no_id,
		                    &hsg->hsg_nvec->nv_note[i].no_id));
		hsg->hsg_nvec->nv_note[i].no_state =
			nvec_rep->nv_note[i].no_state;
	}
	m0_chan_broadcast_lock(hsg->hsg_wait_chan);
	m0_free(hsg);
}

M0_INTERNAL const char *m0_ha_state2str(enum m0_ha_obj_state state)
{
#define S_CASE(x) case x: return # x
	switch (state) {
	S_CASE(M0_NC_UNKNOWN);
	S_CASE(M0_NC_ONLINE);
	S_CASE(M0_NC_FAILED);
	S_CASE(M0_NC_TRANSIENT);
	S_CASE(M0_NC_REPAIR);
	S_CASE(M0_NC_REPAIRED);
	S_CASE(M0_NC_REBALANCE);
	S_CASE(M0_NC_DTM_RECOVERING);
	default:
		M0_IMPOSSIBLE("Invalid state: %d", state);
	}
#undef S_CASE
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
/*
* vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
*/
