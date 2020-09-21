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


#include "be/domain.h"

#include "ut/ut.h"
#include "stob/stob.h"          /* m0_stob_id */

#include "be/ut/helper.h"       /* m0_be_ut_backend */

void m0_be_ut_mkfs(void)
{
	struct m0_be_ut_backend  ut_be = {};
	struct m0_be_domain_cfg  cfg = {};
	struct m0_be_domain     *dom = &ut_be.but_dom;
	struct m0_be_seg        *seg;
	void                    *addr;
	void                    *addr2;
	int                      rc;

	m0_be_ut_backend_cfg_default(&cfg);
	/* mkfs mode start */
	rc = m0_be_ut_backend_init_cfg(&ut_be, &cfg, true);
	M0_UT_ASSERT(rc == 0);
	m0_be_ut_backend_seg_add2(&ut_be, 0x10000, true, NULL, &seg);
	addr = seg->bs_addr;
	m0_be_ut_backend_fini(&ut_be);

	M0_SET0(&ut_be);
	/* normal mode start */
	rc = m0_be_ut_backend_init_cfg(&ut_be, &cfg, false);
	M0_UT_ASSERT(rc == 0);
	seg = m0_be_domain_seg(dom, addr);
	addr2 = seg->bs_addr;
	M0_ASSERT_INFO(addr == addr2, "addr = %p, addr2 = %p", addr, addr2);
	m0_be_ut_backend_seg_del(&ut_be, seg);
	m0_be_ut_backend_fini(&ut_be);

	M0_SET0(&ut_be);
	/* normal mode start */
	rc = m0_be_ut_backend_init_cfg(&ut_be, &cfg, false);
	M0_UT_ASSERT(rc == 0);
	seg = m0_be_domain_seg(dom, addr);
	M0_ASSERT_INFO(seg == NULL, "seg = %p", seg);
	m0_be_ut_backend_fini(&ut_be);
}

enum {
	BE_UT_MKFS_MULTISEG_SEG_NR   = 0x10,
	BE_UT_MKFS_MULTISEG_SEG_SIZE = 1 << 24,
};

M0_INTERNAL void m0_be_ut_mkfs_multiseg(void)
{
	struct m0_be_0type_seg_cfg  segs_cfg[BE_UT_MKFS_MULTISEG_SEG_NR];
	struct m0_be_domain_cfg     dom_cfg = {};
	struct m0_be_ut_backend     ut_be = {};
	m0_bcount_t                 size;
	unsigned                    i;
	void                       *addr;
	int                         rc;

	for (i = 0; i < ARRAY_SIZE(segs_cfg); ++i) {
		size = BE_UT_MKFS_MULTISEG_SEG_SIZE;
		addr = m0_be_ut_seg_allocate_addr(size);
		segs_cfg[i] = (struct m0_be_0type_seg_cfg){
			.bsc_stob_key        = m0_be_ut_seg_allocate_id(),
			.bsc_size            = size,
			.bsc_preallocate     = false,
			.bsc_addr            = addr,
			.bsc_stob_create_cfg = NULL,
		};
	}
	m0_be_ut_backend_cfg_default(&dom_cfg);
	dom_cfg.bc_mkfs_mode = true;
	dom_cfg.bc_seg_cfg   = segs_cfg;
	dom_cfg.bc_seg_nr    = ARRAY_SIZE(segs_cfg);

	rc = m0_be_ut_backend_init_cfg(&ut_be, &dom_cfg, true);
	M0_ASSERT(rc == 0);
	m0_be_ut_backend_fini(&ut_be);
}

void m0_be_ut_domain(void)
{
	struct m0_be_ut_backend ut_be = {};

	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_backend_fini(&ut_be);
}

void m0_be_ut_domain_is_stob(void)
{
	struct m0_be_ut_backend  ut_be = {};
	struct m0_be_domain     *dom;
	struct m0_stob_id        stob_id = {};
	bool                     is_stob;

	m0_be_ut_backend_init(&ut_be);
	dom = &ut_be.but_dom;
	is_stob = m0_be_domain_is_stob_log(dom, &stob_id);
	M0_UT_ASSERT(!is_stob);
	is_stob = m0_be_domain_is_stob_seg(dom, &stob_id);
	M0_UT_ASSERT(!is_stob);
	/*
	 * TODO add more cases after domain interfaces allow to enumerate stobs
	 * used by segments and log.
	 */
	m0_be_ut_backend_fini(&ut_be);
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
