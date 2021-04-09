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
#include "btree/btree.h"
#include "lib/types.h"     /* m0_uint128_eq */
#include "lib/misc.h"      /* M0_BITS, M0_IN */
#include "lib/memory.h"    /* M0_ALLOC_PTR */
#include "lib/errno.h"     /* ENOENT */
#include "be/ut/helper.h"
#include "ut/ut.h"



struct m0_ut_suite be_ut = {
        .ts_name = "be-ut",
        .ts_yaml_config_string = "{ valgrind: { timeout: 3600 },"
                                 "  helgrind: { timeout: 3600 },"
                                 "  exclude:  ["
                                 "    btree,"
                                 "    emap,"
                                 "    tx-concurrent,"
                                 "    tx-concurrent-excl"
                                 "  ] }",
        .ts_init = NULL,
        .ts_fini = NULL,
        .ts_tests = {
#ifndef __KERNEL__
                { "btree-create_destroy",    m0_be_ut_btree_create_destroy    },
                { "btree-create_truncate",   m0_be_ut_btree_create_truncate   },
		{ "btree-create_truncate",   m0_be_ut_btree_create_truncate},
		{ "btree-create_truncate",   m0_be_ut_btree_create_truncate},
		{ "btree-create_truncate",   m0_be_ut_btree_create_truncate},
		{NULL, NULL}
        }
};

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

/*  LocalWords:  btree allocator smop smops
 */
