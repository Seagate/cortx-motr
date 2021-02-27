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
 * @addtogroup btree
 *
 * @{
 */

#include "lib/cookie.h"
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_XXX
#include "lib/trace.h"
#include "lib/rwlock.h"
#include "btree/btree.h"

struct m0_btree {
	const struct m0_btree_type *t_type;
	unsigned                    t_height;
	struct tree                *t_addr;
	struct m0_rwlock            t_lock;
};

struct level {
	struct node *l_node;
	uint64_t     l_seq;
	unsigned     l_pos;
	struct node *l_alloc;
};

struct m0_btree_oimpl {
	struct pg_op    i_pop;
	struct alloc_op i_aop;
	struct lock_op  i_lop;
	unsigned        i_used;
	struct level    i_level[0];
};

enum base_phase {
	P_INIT = M0_SOS_INIT,
	P_DONE = M0_SOS_DONE,
	P_DOWN = M0_SOS_NR,
	P_SETUP
	P_LOCK,
	P_CHECK,
	P_ACT,
	P_CLEANUP,
	P_TRYCOOKIE,
	P_NR
};

static int get_tick(struct m0_btree_op *bop) {
	switch (bop->bo_op.o_sm.s_state) {
	case P_INIT:
		if (!m0_cookie_is_null(&bop->bo_key.k_cookie))
			return pg_op_init(&bop->bo_op,
					  &bop->bo_i->i_pop, P_TRYCOOKIE);
		else
			return P_SETUP;
	case P_SETUP:
		alloc(bop->bo_i);
		if (ENOMEM)
			return sub(P_CLEANUP, P_DONE);
	case P_DOWN:
		if (bop->bo_i->i_used < bop->bo_arbor->t_height) {
			return pg_op_init(&bop->bo_op,
					  &bop->bo_i->i_pop, P_DOWN);
		} else
			return P_LOCK;
	case P_LOCK:
		return lock_op_init(&bop->bo_op, &bop->bo_i->i_lop, P_CHECK);
	case P_CHECK:
		if (used_cookie || check_path())
			return P_ACT;
		else if (height_increased) {
			return sub(P_CLEANUP, P_INIT);
		} else {
			bop->bo_i->i_used = 0;
			return P_DOWN;
		}
	case P_ACT:
		bop->bo_cb->c_act(bop->bo_cb, ...);
		lock_op_unlock(&bop->bo_i->i_lop);
		return sub(P_CLEANUP, P_DONE);
	case P_CLEANUP:
		free(bop->bo_i);
		return ret();
	case P_TRYCOOKIE:
		if (cookie_is_valid)
			return P_LOCK;
		else
			return P_SETUP;
	default:
		M0_IMPOSSIBLE("Wrong state: %i", bop->bo_op.o_sm.s_state);
	};
}

#undef M0_TRACE_SUBSYSTEM


/*
 * Test plan:
 *
 * - test how cookies affect performance (for large trees and small trees);
 */
/** @} end of btree group */

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
