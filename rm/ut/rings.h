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


#pragma once

#ifndef __MOTR_RM_RINGS_H__
#define __MOTR_RM_RINGS_H__

#include "rm/rm.h"
#include "rm/ut/rmut.h"

/* from http://en.wikipedia.org/wiki/Rings_of_Power */
enum {
	/* Three Rings for the Elven-kings under the sky... */
	/* Narya, the Ring of Fire (set with a ruby) */
	NARYA = 1 << 0,
	/* Nenya, the Ring of Water or Ring of Adamant (made of mithril and set
	   with a "white stone") */
	NENYA = 1 << 1,
	/* and Vilya, the Ring of Air, the "mightiest of the Three" (made of
	   gold and set with a sapphire)*/
	VILYA = 1 << 2,

	/* Seven for the Dwarf-lords in their halls of stone... */
	/* Ring of Durin */
	DURIN = 1 << 3,
	/* Ring of Thror */
	THROR = 1 << 4,
	/* Unnamed gnome rings... */
	GR_2  = 1 << 5,
	GR_3  = 1 << 6,
	GR_4  = 1 << 7,
	GR_5  = 1 << 8,
	GR_6  = 1 << 9,

	/* Nine for Mortal Men doomed to die... */
	/* Witch-king of Angmar */
	ANGMAR = 1 << 10,
	/* Khamul */
	KHAMUL = 1 << 11,
	/* unnamed man rings... */
	MR_2   = 1 << 12,
	MR_3   = 1 << 13,
	MR_4   = 1 << 14,
	MR_5   = 1 << 15,
	MR_6   = 1 << 16,
	MR_7   = 1 << 17,
	MR_8   = 1 << 18,

	/* One for the Dark Lord on his dark throne */
	THE_ONE = 1 << 19,

	/*
	 * Ring NOT in the story - ring that doesn't conflict with itself.
	 * It can be granted to multiple incoming requests.
	 */
	SHARED_RING = 1 << 20,

	/* Ring NOT in the story - to test failure cases */
	INVALID_RING = 1 << 21,

	/*
	 * Special value for the ring denoting any ring.
	 * Any cached or held credit is suitable for satisfying request
	 * for ANY_RING. Also, ANY_RING doesn't conflicts with any other
	 * particular ring (cro_conflicts(ANY_RING, _) == 0).
	 *
	 * ANY_RING is not borrowed or revoked "as is",
	 * some "real" ring is borrowed/revoked instead.
	 * This trick is made through rings_policy() callback.
	 *
	 * ANY_RING is introduced to test requests for
	 * non-conflicting credits.
	 */
	ANY_RING = 1 << 22,

	ALLRINGS = NARYA | NENYA | VILYA | DURIN | THROR | GR_2 |
	GR_3 | GR_4 | GR_5 | GR_6 | ANGMAR | KHAMUL | MR_2 | MR_3 | MR_4 |
	MR_5 | MR_6 | MR_7 | MR_8 | THE_ONE | SHARED_RING,

	RINGS_RESOURCE_TYPE_ID = 0,

	/*
	 * Rings incoming policy.
	 * Incoming policy affects only requests for ANY_RING.
	 */
	RINGS_RIP              = RIP_NR + 1
};

struct m0_rings {
	struct m0_rm_resource rs_resource;
	uint64_t              rs_id;
};

extern struct m0_rm_resource_type           rings_resource_type;
extern const struct m0_rm_resource_ops      rings_ops;
extern const struct m0_rm_resource_type_ops rings_rtype_ops;
extern const struct m0_rm_credit_ops        rings_credit_ops;
extern const struct m0_rm_incoming_ops      rings_incoming_ops;

extern void rings_utdata_ops_set(struct rm_ut_data *data);

/* __MOTR_RM_RINGS_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
