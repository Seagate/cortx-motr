/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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


#include "lib/assert.h"

#include "xcode/xcode.h"

/**
   @addtogroup xcode

   @{
 */

M0_INTERNAL void m0_xcode_cursor_init(struct m0_xcode_cursor *it,
				      const struct m0_xcode_obj *obj)
{
	M0_SET0(it);
	m0_xcode_cursor_top(it)->s_obj = *obj;
}

M0_INTERNAL struct m0_xcode_cursor_frame *
m0_xcode_cursor_top(struct m0_xcode_cursor *it)
{
	M0_PRE(IS_IN_ARRAY(it->xcu_depth, it->xcu_stack));
	return &it->xcu_stack[it->xcu_depth];
}

M0_INTERNAL const struct m0_xcode_field *
m0_xcode_cursor_field(const struct m0_xcode_cursor *it)
{
	if (it->xcu_depth > 0) {
		const struct m0_xcode_cursor_frame *pre =
			m0_xcode_cursor_top((void *)it) - 1; /* Drop const. */
		return &pre->s_obj.xo_type->xct_child[pre->s_fieldno];
	} else
		return NULL;
}


M0_INTERNAL int m0_xcode_next(struct m0_xcode_cursor *it)
{
	struct m0_xcode_cursor_frame *top;
	struct m0_xcode_cursor_frame *next;
	const struct m0_xcode_type   *xt;
	int                           nr;

	M0_PRE(it->xcu_depth >= 0);

	top = m0_xcode_cursor_top(it);
	xt  = top->s_obj.xo_type;
	nr  = xt->xct_nr;

	M0_ASSERT_INFO_EX(m0_xcode_type_invariant(xt),
			  "xct_name: %s", xt->xct_name);
	switch (top->s_flag) {
	case M0_XCODE_CURSOR_NONE:
		top->s_flag = M0_XCODE_CURSOR_PRE;
		break;
	case M0_XCODE_CURSOR_IN:
		switch (xt->xct_aggr) {
		case M0_XA_RECORD:
		case M0_XA_TYPEDEF:
			++top->s_fieldno;
			break;
		case M0_XA_SEQUENCE:
			if (top->s_fieldno == 0) {
				top->s_elno = 0;
				top->s_fieldno = 1;
			} else {
		case M0_XA_ARRAY: /* sic. All hail C. */
				++top->s_elno;
			}
			if (top->s_elno >= m0_xcode_tag(&top->s_obj)) {
				top->s_elno = 0;
				top->s_fieldno = 2;
			}
			break;
		case M0_XA_UNION:
			if (top->s_fieldno != 0) {
				top->s_fieldno = nr;
				break;
			}
			while (++top->s_fieldno < nr &&
			       m0_xcode_tag(&top->s_obj) !=
			       xt->xct_child[top->s_fieldno].xf_tag) {
				;
			}
			break;
		case M0_XA_OPAQUE:
		default:
			M0_IMPOSSIBLE("wrong aggregation type");
		}
		/* fall through */
	case M0_XCODE_CURSOR_PRE:
		if (top->s_fieldno < nr) {
			top->s_flag = M0_XCODE_CURSOR_IN;
			if (xt->xct_aggr != M0_XA_ATOM) {
				int result;

				++it->xcu_depth;
				next = m0_xcode_cursor_top(it);
				result = m0_xcode_subobj(&next->s_obj,
							 &top->s_obj,
							 top->s_fieldno,
							 top->s_elno);
				if (result != 0)
					return result;
				next->s_fieldno = 0;
				next->s_elno    = 0;
				next->s_flag    = M0_XCODE_CURSOR_PRE;
				next->s_datum   = 0;
			}
		} else
			top->s_flag = M0_XCODE_CURSOR_POST;
		break;
	case M0_XCODE_CURSOR_POST:
		if (--it->xcu_depth < 0)
			return 0;
		top = m0_xcode_cursor_top(it);
		M0_ASSERT(top->s_flag < M0_XCODE_CURSOR_POST);
		top->s_flag = M0_XCODE_CURSOR_IN;
		break;
	default:
		M0_IMPOSSIBLE("wrong order");
	}
	return +1;
}

M0_INTERNAL void m0_xcode_skip(struct m0_xcode_cursor *it)
{
	m0_xcode_cursor_top(it)->s_flag = M0_XCODE_CURSOR_POST;
}

/** @} end of xcode group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
