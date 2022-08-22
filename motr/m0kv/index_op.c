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
 * @addtogroup client
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLIENT
#include "lib/assert.h"             /* M0_ASSERT */
#include "lib/memory.h"             /* M0_ALLOC_ARR */
#include "lib/time.h"               /* M0_TIME_NEVER */
#include "lib/errno.h"
#include "lib/trace.h"              /* M0_ERR */
#include "index_op.h"
#include "motr/client.h"
#include "motr/idx.h"
#include "index.h"
#include "cas/cas.h"                /* m0_dix_fid_type */
#include "fid/fid.h"                /* m0_fid_tassume */
#include "lib/misc.h"               /* M0_AMB */
#include "motr/client_internal.h"   /* m0_op_common */

static int per_item_rcs_analyse(int32_t *rcs, int cnt)
{
	int i;
	int rc = 0;

	for (i = 0; i < cnt; i++)
		if (rcs[i] != 0) {
			m0_console_printf("rcs[%d]: %d\n", i, rcs[i]);
			rc = rcs[i];
		}
	return M0_RC(rc);
}

static int index_op_tail(struct m0_entity *ce,
			 struct m0_op *op, int rc,
			 int *sm_rc)
{
	if (rc == 0) {
		m0_op_launch(&op, 1);
		rc = m0_op_wait(op,
				    M0_BITS(M0_OS_FAILED,
					    M0_OS_STABLE),
				    M0_TIME_NEVER);
		m0_console_printf("operation rc: %i\n", op->op_rc);
		if (sm_rc != NULL)
			/* Save retcodes. */
			*sm_rc = op->op_rc;
	} else
		m0_console_printf("operation rc: %i\n", rc);
	m0_op_fini(op);
	m0_op_free(op);
	m0_entity_fini(ce);
	return M0_RC(rc);
}

void set_idx_flags(struct m0_op *op)
{
	struct m0_op_common *oc;
	struct m0_op_idx    *oi;

	oc = M0_AMB(oc, op, oc_op);
	oi = M0_AMB(oi, oc, oi_oc);

	if (is_skip_layout)
		oi->oi_flags |= M0_OIF_SKIP_LAYOUT;

	if (is_crow_disable)
		oi->oi_flags &= ~M0_OIF_CROW;
	else
		oi->oi_flags |= M0_OIF_CROW;
}

void set_enf_meta_flag(struct m0_idx *idx)
{
	idx->in_entity.en_flags |= M0_ENF_META;
	if (dix_pool_ver.f_container != 0) {
		idx->in_attr.idx_layout_type = DIX_LTYPE_DESCR;
		idx->in_attr.idx_pver = dix_pool_ver;
		m0_console_printf("DIX pool version: "FID_F" \n", 
				  FID_P(&idx->in_attr.idx_pver));
	}
}

int validate_pool_version(struct m0_idx *idx)
{
	int rc = 0;
	if (is_enf_meta || is_skip_layout) {
		if (m0_fid_is_valid(&dix_pool_ver) &&
		    m0_fid_is_set(&dix_pool_ver))
			set_enf_meta_flag(idx);
		else
			rc = -EINVAL;
	} else if (m0_fid_is_set(&dix_pool_ver)) {
		rc = -EINVAL;
	}
	return rc;
}

int index_create(struct m0_realm *parent, struct m0_fid_arr *fids)
{
	int i;
	int rc = 0;

	M0_PRE(fids != NULL && fids->af_count != 0);

	for(i = 0; rc == 0 && i < fids->af_count; ++i) {
		struct m0_op   *op  = NULL;
		struct m0_idx   idx = {{0}};

		m0_fid_tassume(&fids->af_elems[i], &m0_dix_fid_type);
		m0_idx_init(&idx, parent,
				   (struct m0_uint128 *)&fids->af_elems[i]);

		if (is_enf_meta || is_skip_layout)
			set_enf_meta_flag(&idx);

		rc = m0_entity_create(NULL, &idx.in_entity, &op);

		set_idx_flags(op);

		rc = index_op_tail(&idx.in_entity, op, rc, NULL);
		if (rc == 0 && (is_enf_meta || is_skip_layout))
			m0_console_printf("DIX pool version: "FID_F" \n",
					  FID_P(&idx.in_attr.idx_pver));
	}
	return M0_RC(rc);
}

int index_drop(struct m0_realm *parent, struct m0_fid_arr *fids)
{
	int i;
	int rc = 0;

	M0_PRE(fids != NULL && fids->af_count != 0);

	for(i = 0; rc == 0 && i < fids->af_count; ++i) {
		struct m0_idx   idx = {{0}};
		struct m0_op   *op = NULL;

		m0_fid_tassume(&fids->af_elems[i], &m0_dix_fid_type);
		m0_idx_init(&idx, parent,
			    (struct m0_uint128 *)&fids->af_elems[i]);

		rc = validate_pool_version(&idx);
		if (rc != 0)
			return M0_RC(rc);

		rc = m0_entity_open(&idx.in_entity, &op) ?:
		     m0_entity_delete(&idx.in_entity, &op);
		if (rc != 0)
			return M0_RC(rc);

		set_idx_flags(op);

		rc = index_op_tail(&idx.in_entity, op, rc, NULL);
	}
	return M0_RC(rc);
}

int index_list(struct m0_realm  *parent,
	       struct m0_fid    *fid,
	       int               cnt,
	       struct m0_bufvec *keys)
{
	struct m0_idx  idx = {{0}};
	struct m0_op  *op = NULL;
	int32_t       *rcs;
	int            rc;

	M0_PRE(cnt != 0);
	M0_PRE(fid != NULL);
	M0_ALLOC_ARR(rcs, cnt);
	rc = m0_bufvec_alloc(keys, cnt, sizeof(struct m0_fid));
	if (rc != 0 || rcs == NULL) {
		m0_free(rcs);
		return M0_ERR(rc);
	}
	m0_fid_tassume(fid, &m0_dix_fid_type);
	m0_idx_init(&idx, parent, (struct m0_uint128 *)fid);

	rc = validate_pool_version(&idx);
	if (rc != 0)
		return M0_RC(rc);

	rc = m0_idx_op(&idx, M0_IC_LIST, keys, NULL,
		       rcs, 0, &op);

	set_idx_flags(op);

	rc = index_op_tail(&idx.in_entity, op, rc, NULL);
	m0_free(rcs);
	return M0_RC(rc);
}

int index_lookup(struct m0_realm   *parent,
		 struct m0_fid_arr *fids,
		 struct m0_bufvec  *rets)
{
	int  i;
	int  rc = 0;

	M0_PRE(fids != NULL);
	M0_PRE(fids->af_count != 0);
	M0_PRE(rets != NULL);
	M0_PRE(rets->ov_vec.v_nr == 0);

	rc = m0_bufvec_alloc(rets, fids->af_count, sizeof(rc));
	/* Check that indices exist. */
	for(i = 0; rc == 0 && i < fids->af_count; ++i) {
		struct m0_idx  idx = {{0}};
		struct m0_op  *op = NULL;

		m0_fid_tassume(&fids->af_elems[i], &m0_dix_fid_type);
		m0_idx_init(&idx, parent,
			    (struct m0_uint128 *)&fids->af_elems[i]);

		rc = validate_pool_version(&idx);
		if (rc != 0)
			return M0_RC(rc);

		rc = m0_idx_op(&idx, M0_IC_LOOKUP, NULL, NULL,
			       NULL, 0, &op);

		set_idx_flags(op);

		rc = index_op_tail(&idx.in_entity, op, rc,
				   (int *)rets->ov_buf[i]);
	}
	return M0_RC(rc);
}

static int index_op(struct m0_realm    *parent,
		    struct m0_fid      *fid,
		    enum m0_idx_opcode  opcode,
		    struct m0_bufvec   *keys,
		    struct m0_bufvec   *vals)
{
	struct m0_idx  idx = {{0}};
	struct m0_op  *op = NULL;
	int32_t       *rcs;
	int            rc;

	M0_ASSERT(keys != NULL);
	M0_ASSERT(keys->ov_vec.v_nr != 0);
	M0_ALLOC_ARR(rcs, keys->ov_vec.v_nr);
	if (rcs == NULL)
		return M0_ERR(-ENOMEM);

	m0_fid_tassume(fid, &m0_dix_fid_type);
	m0_idx_init(&idx, parent, (struct m0_uint128 *)fid);

	rc = validate_pool_version(&idx);
	if (rc != 0)
		return M0_RC(rc);

	rc = m0_idx_op(&idx, opcode, keys, vals, rcs,
	               opcode == M0_IC_PUT ? M0_OIF_OVERWRITE : 0,
		       &op);

	set_idx_flags(op);

	rc = index_op_tail(&idx.in_entity, op, rc, NULL);
	/*
	 * Don't analyse per-item codes for NEXT, because usually user gets
	 * -ENOENT in 'rcs' since he requests more entries than exist.
	 */
	if (opcode != M0_IC_NEXT)
	     rc = per_item_rcs_analyse(rcs, keys->ov_vec.v_nr);
	m0_free(rcs);
	return M0_RC(rc);
}

int index_put(struct m0_realm   *parent,
	      struct m0_fid_arr *fids,
	      struct m0_bufvec  *keys,
	      struct m0_bufvec  *vals)
{
	int rc = 0;
	int i;

	M0_PRE(fids != NULL && fids->af_count != 0);
	M0_PRE(keys != NULL);
	M0_PRE(vals != NULL);

	for (i = 0; i < fids->af_count && rc == 0; i++)
		rc = index_op(parent, &fids->af_elems[i],
			      M0_IC_PUT, keys, vals);

	return M0_RC(rc);
}

int index_del(struct m0_realm   *parent,
	      struct m0_fid_arr *fids,
	      struct m0_bufvec  *keys)
{
	int rc = 0;
	int i;

	M0_PRE(fids != NULL && fids->af_count != 0);
	M0_PRE(keys != NULL);

	for (i = 0; i < fids->af_count && rc == 0; i++)
		rc = index_op(parent, &fids->af_elems[i],
			      M0_IC_DEL, keys, NULL);

	return M0_RC(rc);
}

int index_get(struct m0_realm  *parent,
	      struct m0_fid    *fid,
	      struct m0_bufvec *keys,
	      struct m0_bufvec *vals)
{
	int rc;
	int keys_nr;

	M0_PRE(fid != NULL);
	M0_PRE(keys != NULL);
	M0_PRE(vals != NULL && vals->ov_vec.v_nr == 0);

	/* Allocate vals entity without buffers. */
	keys_nr = keys->ov_vec.v_nr;
	rc = m0_bufvec_empty_alloc(vals, keys_nr) ?:
	     index_op(parent, fid, M0_IC_GET, keys, vals) ?:
	     m0_exists(i, keys_nr, vals->ov_buf[i] == NULL) ?
				M0_ERR(-ENODATA) : 0;

	return M0_RC(rc);
}

int index_next(struct m0_realm  *parent,
	       struct m0_fid    *fid,
	       struct m0_bufvec *keys,
	       int               cnt,
	       struct m0_bufvec *vals)
{
	int   rc;
	void *startkey;
	int   startkey_size;

	M0_PRE(fid != NULL);
	M0_PRE(cnt != 0);
	M0_PRE(keys != NULL && keys->ov_vec.v_nr == 1);
	M0_PRE(vals != NULL && vals->ov_vec.v_nr == 0);

	/* Allocate array for VALs. */
	rc = m0_bufvec_empty_alloc(vals, cnt);
	/* Allocate array for KEYs, reuse first buffer. */
	if (rc == 0) {
		startkey = m0_alloc(keys->ov_vec.v_count[0]);
		if (startkey == NULL)
			goto fail;
		startkey_size = keys->ov_vec.v_count[0];
		memcpy(startkey, keys->ov_buf[0], keys->ov_vec.v_count[0]);
		m0_bufvec_free(keys);
		rc = m0_bufvec_empty_alloc(keys, cnt);
		if (rc != 0)
			goto fail;
		keys->ov_buf[0] = startkey;
		keys->ov_vec.v_count[0] = startkey_size;
	}
	if (rc == 0)
		rc = index_op(parent, fid, M0_IC_NEXT, keys, vals);

	return M0_RC(rc);
fail:
	rc = M0_ERR(-ENOMEM);
	m0_bufvec_free(vals);
	m0_bufvec_free(keys);
	m0_free(startkey);
	return M0_ERR(rc);
}


#undef M0_TRACE_SUBSYSTEM

/** @} end of client group */

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
