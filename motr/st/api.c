/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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



/*
 * Wrappers for each m0_xxx in which we keep track of
 * the resources used.
 */

#include "motr/client.h"
#include "motr/st/st.h"
#include "motr/st/st_misc.h"
#include "motr/st/st_assert.h"

void st_container_init(struct m0_container        *con,
		       struct m0_realm            *parent,
		       const struct m0_uint128    *id,
		       struct m0_client           *instance)
{
	m0_container_init(con, parent, id, instance);
}

void st_obj_init(struct m0_obj *obj,
		 struct m0_realm *parent,
		 const struct m0_uint128 *id, uint64_t layout_id)
{
	m0_obj_init(obj, parent, id, layout_id);
	st_mark_entity(&obj->ob_entity);
}

void st_obj_fini(struct m0_obj *obj)
{
	m0_obj_fini(obj);
	st_unmark_entity(&obj->ob_entity);
}

void st_idx_init(struct m0_idx           *idx,
		 struct m0_realm         *parent,
		 const struct m0_uint128 *id)
{
	m0_idx_init(idx, parent, id);
	st_mark_entity(&idx->in_entity);
}

void st_idx_fini(struct m0_idx *idx)
{
	m0_idx_fini(idx);
	st_unmark_entity(&idx->in_entity);
}


int st_entity_create(struct m0_fid *pool,
		     struct m0_entity *entity,
		     struct m0_op **op)
{
	int rc;

	rc = m0_entity_create(NULL, entity, op);
	if (*op != NULL) st_mark_op(*op);

	return rc;
}

int st_entity_delete(struct m0_entity *entity,
		     struct m0_op **op)
{
	int rc;

	rc = m0_entity_delete(entity, op);
	if (*op != NULL) st_mark_op(*op);

	return rc;
}

void st_entity_fini(struct m0_entity *entity)
{
	m0_entity_fini(entity);
	st_unmark_entity(entity);
}

void st_obj_op(struct m0_obj      *obj,
	       enum m0_obj_opcode  opcode,
	       struct m0_indexvec *ext,
	       struct m0_bufvec   *data,
	       struct m0_bufvec   *attr,
	       uint64_t            mask,
	       uint32_t            flags,
	       struct m0_op      **op)
{
	m0_obj_op(obj, opcode, ext, data, attr, mask, flags, op);
	if (*op != NULL) st_mark_op(*op);
}

int st_idx_op(struct m0_idx     *idx,
	      enum m0_idx_opcode opcode,
	      struct m0_bufvec  *keys,
	      struct m0_bufvec  *vals,
	      int               *rcs,
	      int                flag,
	      struct m0_op     **op)
{
	int rc;

	rc = m0_idx_op(idx, opcode, keys, vals, rcs, flag, op);
	if (*op != NULL) st_mark_op(*op);
	m0_idx_op_setoption(*op, M0_OIO_MIN_SUCCESS,
			    M0_DIX_MIN_REPLICA_QUORUM);

	return rc;
}

void st_op_launch(struct m0_op **op, uint32_t nr)
{
	/* nothing to record, call m0_xxx directly */
	m0_op_launch(op, nr);
	return;
}

int32_t st_op_wait(struct m0_op *op, uint64_t bits, m0_time_t to)
{
	/* nothing to record, call m0_xxx directly */
	return m0_op_wait(op, bits, to);
}

void st_op_fini(struct m0_op *op)
{
	m0_op_fini(op);
}

void st_op_free(struct m0_op *op)
{
	m0_op_free(op);
	st_unmark_op(op);
}

void st_entity_open(struct m0_entity *entity)
{
	struct m0_op *ops[1] = {NULL};
	int           rc;

	m0_entity_open(entity, &ops[0]);
	if (ops[0] != NULL) st_mark_op(ops[0]);
	st_op_launch(ops, 1);
	rc = st_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					       M0_OS_STABLE),
			       m0_time_from_now(5, 0));
	if ( rc == -ETIMEDOUT) {
		m0_op_cancel(ops, 1);

		m0_op_wait(ops[0],
				  M0_BITS(M0_OS_FAILED,
				          M0_OS_STABLE),
				  M0_TIME_NEVER);
	}
	st_op_fini(ops[0]);
	st_op_free(ops[0]);
}

void st_idx_open(struct m0_entity *entity)
{
	struct m0_op *ops[1] = {NULL};

	m0_entity_open(entity, &ops[0]);
}

int st_layout_op(struct m0_obj *obj,
		 enum m0_entity_opcode opcode,
		 struct m0_client_layout *layout,
		 struct m0_op **op)
{
	int rc;

	rc = m0_client_layout_op(obj, opcode, layout, op);
	if (*op != NULL) st_mark_op(*op);

	return rc;
}
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
