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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"
#include "lib/types.h"		/* m0_uint128_eq */
#include "lib/misc.h"		/* M0_BITS */
#include "lib/memory.h"         /* M0_ALLOC_PTR, m0_free */
#include "be/ut/helper.h"	/* m0_be_ut_backend */
#include "ut/ut.h"


void m0_be_ut_seg_dict(void)
{
	static struct m0_be_ut_backend ut_be;
	struct m0_be_tx_credit  credit = {};
	struct m0_be_tx         tx     = {};
	struct m0_be_ut_seg     ut_seg;
	struct m0_be_seg       *seg;
	const char             *nk; /*next key */
	void                   *p;
	int                     i;
	int                     rc;
#define OPT 7
#define END 11
	struct {
		const char *name;
		void       **value;
	} dict[] = {
			{ "dead",       (void*)0xdead },
			{ "beaf",       (void*)0xbeaf },
			{ "cafe",       (void*)0xcafe },
			{ "babe",       (void*)0xbabe },
			{ "d00d",       (void*)0xd00d },
			{ "8bad",       (void*)0x8bad },
			{ "f00d",       (void*)0xf00d },
		[OPT] = { "M0_BE:opt1", (void*)0xf00d0001 },
			{ "M0_BE:opt2", (void*)0xf00d0002 },
			{ "M0_BE:opt3", (void*)0xf00d0003 },
			{ "M0_BE:opt4", (void*)0xf00d0004 },
		[END] =	{ "M0_BE:end0", (void*)0xf00d0000 },
	};

	M0_SET0(&ut_be);
	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, &ut_be, 1 << 20);
	seg = ut_seg.bus_seg;
#ifdef __KERNEL__
	m0_be_tx_init(&tx, 0, seg->bs_domain, NULL, NULL, NULL, NULL, NULL);
	m0_be_seg_dict_create_credit(seg, &credit);
	m0_be_tx_prep(&tx, &credit);
	rc = m0_be_tx_open_sync(&tx);
	M0_UT_ASSERT(rc == 0);
	m0_be_seg_dict_create(seg, &tx);
	m0_be_tx_close_sync(&tx);
	m0_be_tx_fini(&tx);
#endif
	M0_SET0(&tx);
	m0_be_ut_tx_init(&tx, &ut_be);

	credit = M0_BE_TX_CREDIT(0, 0);
	for (i = 0; i < ARRAY_SIZE(dict); ++i) {
		m0_be_seg_dict_insert_credit(seg, "....", &credit);
		m0_be_seg_dict_delete_credit(seg, "....", &credit);
	}
	m0_be_tx_prep(&tx, &credit);
	m0_be_tx_open_sync(&tx);

	for (i = 0; i < ARRAY_SIZE(dict); ++i) {
		rc = m0_be_seg_dict_insert(seg, &tx, dict[i].name,
					   dict[i].value);
		M0_UT_ASSERT(rc == 0);
	}

	for (i = 0; i < ARRAY_SIZE(dict); ++i) {
		rc = m0_be_seg_dict_lookup(seg, dict[i].name, &p);
		M0_UT_ASSERT(rc == 0 && dict[i].value == p);
	}

	for (i = 0, rc = m0_be_seg_dict_begin(seg, "M0_BE:opt", &nk, &p);
	     rc == 0;
	     i++, rc = m0_be_seg_dict_next(seg, "M0_BE:opt", nk, &nk, &p)) {
		M0_UT_ASSERT(rc == 0 && p == dict[i + OPT].value &&
			     strcmp(nk, dict[i + OPT].name) == 0);
	}
	M0_UT_ASSERT(i == END-OPT);

	for (i = 0; i < ARRAY_SIZE(dict); i+=2) {
		rc = m0_be_seg_dict_delete(seg, &tx, dict[i].name);
		M0_UT_ASSERT(rc == 0);
	}

	for (i = 1; i < ARRAY_SIZE(dict); i+=2) {
		rc = m0_be_seg_dict_lookup(seg, dict[i].name, &p);
		M0_UT_ASSERT(rc == 0 && dict[i].value == p);
	}

	m0_be_tx_close_sync(&tx);
	m0_be_tx_fini(&tx);
	/* reload segment, check dictionary is persistent */
	m0_be_ut_seg_reload(&ut_seg);

	m0_be_seg_dict_init(seg);

	for (i = 1; i < ARRAY_SIZE(dict); i+=2) {
		rc = m0_be_seg_dict_lookup(seg, dict[i].name, &p);
		M0_UT_ASSERT(rc == 0 && dict[i].value == p);
	}

	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);
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
