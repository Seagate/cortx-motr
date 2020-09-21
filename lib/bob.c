/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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


#include "xcode/xcode.h"
#include "lib/tlist.h"
#include "lib/assert.h"
#include "lib/bob.h"

/**
 * @addtogroup bob
 *
 * @{
 */

static bool bob_type_invariant(const struct m0_bob_type *bt)
{
	return
		_0C(bt->bt_name != NULL) && _0C(*bt->bt_name != '\0') &&
		_0C(bt->bt_magix != 0);
}

M0_INTERNAL void m0_bob_type_tlist_init(struct m0_bob_type *bt,
					const struct m0_tl_descr *td)
{
	M0_PRE(td->td_link_magic != 0);

	bt->bt_name         = td->td_name;
	bt->bt_magix        = td->td_link_magic;
	bt->bt_magix_offset = td->td_link_magic_offset;

	M0_POST(bob_type_invariant(bt));
}

/**
 * Returns the address of the magic field.
 *
 * Macro is used instead of inline function so that constness of the result
 * depends on the constness of "bob" argument.
 */
#define MAGIX(bt, bob) ((uint64_t *)(bob + bt->bt_magix_offset))

M0_INTERNAL void m0_bob_init(const struct m0_bob_type *bt, void *bob)
{
	M0_PRE(bob_type_invariant(bt));

	*MAGIX(bt, bob) = bt->bt_magix;
}

M0_INTERNAL void m0_bob_fini(const struct m0_bob_type *bt, void *bob)
{
	M0_ASSERT(m0_bob_check(bt, bob));
	*MAGIX(bt, bob) = 0;
}

M0_INTERNAL bool m0_bob_check(const struct m0_bob_type *bt, const void *bob)
{
	return
		_0C((unsigned long)bob + 4096 > 8192) &&
		_0C(*MAGIX(bt, bob) == bt->bt_magix) &&
		ergo(bt->bt_check != NULL, _0C(bt->bt_check(bob)));
}

/** @} end of cond group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
