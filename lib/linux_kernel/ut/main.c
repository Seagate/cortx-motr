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


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include "ut/ut.h"

/* These unit tests are done in the kernel */
M0_INTERNAL void test_bitmap(void);
M0_INTERNAL void test_bitmap_onwire(void);
M0_INTERNAL void test_chan(void);
M0_INTERNAL void test_cookie(void);
M0_INTERNAL void test_finject(void);
M0_INTERNAL void test_list(void);
M0_INTERNAL void test_locality(void);
M0_INTERNAL void test_locality_chore(void);
M0_INTERNAL void test_lockers(void);
M0_INTERNAL void test_tlist(void);
M0_INTERNAL void test_mutex(void);
M0_INTERNAL void test_queue(void);
M0_INTERNAL void test_refs(void);
M0_INTERNAL void test_rw(void);
M0_INTERNAL void test_thread(void);
M0_INTERNAL void m0_ut_time_test(void);
M0_INTERNAL void test_trace(void);
M0_INTERNAL void test_varr(void);
M0_INTERNAL void test_vec(void);
M0_INTERNAL void test_zerovec(void);
M0_INTERNAL void test_memory(void);
M0_INTERNAL void test_bob(void);
M0_INTERNAL void m0_ut_lib_buf_test(void);
M0_INTERNAL void m0_test_lib_uuid(void);
M0_INTERNAL void test_hashtable(void);
M0_INTERNAL void test_fold(void);

struct m0_ut_suite m0_klibm0_ut = {
	.ts_name = "klibm0-ut",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "bitmap",        test_bitmap        },
		{ "bitmap-onwire", test_bitmap_onwire },
		{ "memory",        test_memory        },
		{ "bob",           test_bob           },
		{ "buf",           m0_ut_lib_buf_test },
		{ "chan",          test_chan          },
		{ "cookie",        test_cookie        },
#ifdef ENABLE_FAULT_INJECTION
		{ "finject",       test_finject       },
#endif
		{ "hash",          test_hashtable     },
		{ "list",          test_list          },
		{ "locality",      test_locality,       "Nikita" },
		{ "loc-chores",    test_locality_chore, "Nikita" },
		{ "lockers",       test_lockers       },
		{ "tlist",         test_tlist         },
		{ "mutex",         test_mutex         },
		{ "queue",         test_queue         },
		{ "refs",          test_refs          },
		{ "rwlock",        test_rw            },
		{ "thread",        test_thread        },
		{ "time",          m0_ut_time_test    },
		{ "trace",         test_trace         },
		{ "uuid",          m0_test_lib_uuid   },
		{ "varr",          test_varr	      },
		{ "vec",           test_vec           },
		{ "zerovec",       test_zerovec       },
		{ "fold",          test_fold,           "Nikita" },
		{ NULL,            NULL               }
	}
};
M0_EXPORTED(m0_klibm0_ut);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
