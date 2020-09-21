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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "lib/memory.h"
#include "lib/string.h"       /* m0_strdup */
#include "fop/fom_generic.h"
#include "fop/fop.h"
#include "fop/fom.h"
#include "ha/note.h"
#include "ha/note_fops.h"     /* m0_ha_state_fop */
#include "rpc/rpc.h"
#include "reqh/reqh.h"
#include "conf/confc.h"       /* m0_conf_cache */

static void ha_state_fom_fini(struct m0_fom *fom)
{
	m0_fom_fini(fom);
	m0_free(fom);
}

static  size_t ha_state_fom_home_locality(const struct m0_fom *fom)
{
	return m0_fop_opcode(fom->fo_fop);
}

static int ha_state_set_fom_tick(struct m0_fom *fom)
{
	m0_fom_block_enter(fom);
	m0_ha_state_accept(m0_fop_data(fom->fo_fop), false);
	m0_fom_block_leave(fom);
	m0_rpc_reply_post(&fom->fo_fop->f_item, &fom->fo_rep_fop->f_item);
	m0_fom_phase_set(fom, M0_FOPH_FINISH);
	return M0_FSO_WAIT;
}

const struct m0_fom_ops m0_ha_state_set_fom_ops = {
	.fo_tick          = &ha_state_set_fom_tick,
	.fo_fini          = &ha_state_fom_fini,
	.fo_home_locality = &ha_state_fom_home_locality,
};

static int ha_state_set_fom_create(struct m0_fop   *fop,
				   struct m0_fom  **m,
				   struct m0_reqh  *reqh)
{
	struct m0_fom               *fom;
	struct m0_fop               *reply;
	struct m0_fop_generic_reply *rep;

	M0_PRE(fop != NULL);
	M0_PRE(m != NULL);

	M0_ALLOC_PTR(fom);
	reply = m0_fop_reply_alloc(fop, &m0_fop_generic_reply_fopt);
	if (fom == NULL || reply == NULL) {
		m0_free(fom);
		if (reply != NULL)
			m0_fop_put_lock(reply);
		return M0_ERR(-ENOMEM);
	}
	m0_fom_init(fom, &fop->f_type->ft_fom_type, &m0_ha_state_set_fom_ops,
		    fop, reply, reqh);

	rep = m0_fop_data(reply);
	rep->gr_rc = 0;
	rep->gr_msg.s_len = 0;
	*m = fom;
	return 0;
}

static void ha_state_get(struct m0_conf_cache  *cache,
			struct m0_ha_nvec      *req_fop,
			struct m0_ha_state_fop *rep_fop)
{
	struct m0_conf_obj *obj;
	struct m0_ha_nvec  *note_vec = &rep_fop->hs_note;
	int                 i;

	M0_ENTRY();
	M0_NVEC_PRINT(req_fop, " in ", M0_DEBUG);
	note_vec->nv_nr = req_fop->nv_nr;
	m0_conf_cache_lock(cache);
	for (i = 0; i < req_fop->nv_nr; ++i) {
		obj = m0_conf_cache_lookup(cache, &req_fop->nv_note[i].no_id);
		if (obj != NULL) {
			note_vec->nv_note[i].no_id = obj->co_id;
			note_vec->nv_note[i].no_state = obj->co_ha_state;
		}
	}
	m0_conf_cache_unlock(cache);
	M0_NVEC_PRINT(note_vec, " out ", M0_DEBUG);
	M0_LEAVE();
}

static int ha_state_get_fom_tick(struct m0_fom *fom)
{
	struct m0_ha_nvec      *req_fop;
	struct m0_ha_state_fop *rep_fop;
	struct m0_confc        *confc = m0_reqh2confc(m0_fom2reqh(fom));

	req_fop = m0_fop_data(fom->fo_fop);
	rep_fop = m0_fop_data(fom->fo_rep_fop);

	ha_state_get(&confc->cc_cache, req_fop, rep_fop);

        m0_rpc_reply_post(&fom->fo_fop->f_item, &fom->fo_rep_fop->f_item);
        m0_fom_phase_set(fom, M0_FOPH_FINISH);
        return M0_FSO_WAIT;
}

const struct m0_fom_ops ha_state_get_fom_ops = {
	.fo_tick          = ha_state_get_fom_tick,
	.fo_fini          = ha_state_fom_fini,
	.fo_home_locality = ha_state_fom_home_locality,
};

static int ha_state_get_fom_create(struct m0_fop   *fop,
				   struct m0_fom  **m,
				   struct m0_reqh  *reqh)
{
	struct m0_fom          *fom;
	struct m0_ha_state_fop *ha_state_fop;
	struct m0_ha_nvec      *req_fop;

	M0_PRE(fop != NULL);
	M0_PRE(m != NULL);

	M0_ALLOC_PTR(fom);
	if (fom == NULL)
		return M0_ERR(-ENOMEM);

	M0_ALLOC_PTR(ha_state_fop);
	if (ha_state_fop == NULL){
		m0_free(fom);
		return M0_ERR(-ENOMEM);
	}

	req_fop = m0_fop_data(fop);
	M0_ALLOC_ARR(ha_state_fop->hs_note.nv_note, req_fop->nv_nr);
	if (ha_state_fop->hs_note.nv_note == NULL){
		m0_free(ha_state_fop);
		m0_free(fom);
		return M0_ERR(-ENOMEM);
	}

	fom->fo_rep_fop = m0_fop_alloc(&m0_ha_state_get_rep_fopt, ha_state_fop,
				       m0_fop_rpc_machine(fop));
	if (fom->fo_rep_fop == NULL) {
		m0_free(ha_state_fop->hs_note.nv_note);
		m0_free(ha_state_fop);
		m0_free(fom);
		return M0_ERR(-ENOMEM);
	}

	m0_fom_init(fom, &fop->f_type->ft_fom_type, &ha_state_get_fom_ops,
		    fop, fom->fo_rep_fop, reqh);

	*m = fom;
	return M0_RC(0);
}
static const struct m0_fom_type_ops ha_get_fomt_ops = {
	.fto_create = &ha_state_get_fom_create
};

static const struct m0_fom_type_ops ha_set_fomt_ops = {
	.fto_create = &ha_state_set_fom_create
};

const struct m0_fom_type_ops *m0_ha_state_get_fom_type_ops = &ha_get_fomt_ops;
const struct m0_fom_type_ops *m0_ha_state_set_fom_type_ops = &ha_set_fomt_ops;

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
