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


#include "lib/lockers.h" /* m0_lockers */
#include "lib/types.h"   /* uint32_t */
#include "lib/string.h"  /* memset */
#include "lib/assert.h"  /* M0_PRE */
#include "lib/misc.h"    /* M0_SET0 */

/**
 * @addtogroup lockers
 *
 * @{
 */

static bool key_is_valid(const struct m0_lockers_type *lt, int key);

M0_INTERNAL void m0_lockers_init(const struct m0_lockers_type *lt,
				 struct m0_lockers            *lockers)
{
	memset(lockers->loc_slots, 0,
	       lt->lot_max * sizeof lockers->loc_slots[0]);
	lockers->available = true;
}

M0_INTERNAL int m0_lockers_allot(struct m0_lockers_type *lt)
{
	int i;

	for (i = 0; i < lt->lot_max; ++i) {
		if (!lt->lot_inuse[i]) {
			lt->lot_inuse[i] = true;
			return i;
		}
	}
	M0_IMPOSSIBLE("Lockers table overflow.");
}

M0_INTERNAL void m0_lockers_free(struct m0_lockers_type *lt, int key)
{
	M0_PRE(key_is_valid(lt, key));
	lt->lot_inuse[key] = false;
}

M0_INTERNAL void m0_lockers_set(const struct m0_lockers_type *lt,
				struct m0_lockers            *lockers,
				uint32_t                      key,
				void                         *data)
{
	M0_PRE(key_is_valid(lt, key));
	lockers->loc_slots[key] = data;
}

M0_INTERNAL void *m0_lockers_get(const struct m0_lockers_type *lt,
				 const struct m0_lockers      *lockers,
				 uint32_t                      key)
{
	M0_PRE(key_is_valid(lt, key));
	return lockers->loc_slots[key];
}

M0_INTERNAL void m0_lockers_clear(const struct m0_lockers_type *lt,
				  struct m0_lockers            *lockers,
				  uint32_t                      key)
{
	M0_PRE(key_is_valid(lt, key));
	lockers->loc_slots[key] = NULL;
}

M0_INTERNAL bool m0_lockers_is_empty(const struct m0_lockers_type *lt,
				     const struct m0_lockers      *lockers,
				     uint32_t                      key)
{
	M0_PRE(key_is_valid(lt, key));
	return lockers->loc_slots[key] == NULL;
}

M0_INTERNAL void m0_lockers_fini(struct m0_lockers_type *lt,
				 struct m0_lockers      *lockers)
{
	lockers->available = false;
}

static bool key_is_valid(const struct m0_lockers_type *lt, int key)
{
	return key < lt->lot_max && lt->lot_inuse[key];
}

M0_INTERNAL bool m0_lockers_available(struct m0_lockers *lockers)
{
	return lockers->available;
}


/** @} end of lockers group */


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
