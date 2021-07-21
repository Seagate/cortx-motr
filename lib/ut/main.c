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


#include "ut/ut.h"

/* sort test suites in alphabetic order */
extern void m0_test_lib_uuid(void);
extern void m0_ut_lib_buf_test(void);
extern void test_0C(void);
extern void test_atomic(void);
extern void test_bitmap(void);
extern void test_bitmap_onwire(void);
extern void test_bob(void);
extern void test_chan(void);
extern void test_cookie(void);
extern void test_finject(void);
extern void test_getopts(void);
extern void test_list(void);
extern void test_lockers(void);
extern void test_memory(void);
extern void m0_test_misc(void);
extern void test_mutex(void);
extern void test_processor(void);
extern void test_queue(void);
extern void test_refs(void);
extern void test_rw(void);
extern void test_thread(void);
extern void m0_ut_time_test(void);
extern void test_timer(void);
extern void test_tlist(void);
extern void test_trace(void);
extern void test_varr(void);
extern void test_vec(void);
extern void test_zerovec(void);
extern void test_locality(void);
extern void test_locality_chore(void);
extern void test_hashtable(void);
extern void test_fold(void);
extern void m0_ut_lib_thread_pool_test(void);
extern void test_combinations(void);
extern void test_hash_fnc(void);
extern void m0_test_coroutine(void);
extern void m0_test_coroutine2(void);

struct m0_ut_suite libm0_ut = {
	.ts_name = "libm0-ut",
	.ts_owners = "Nikita",
	.ts_tests = {
		{ "0C",               test_0C            },
		{ "atomic",           test_atomic        },
		{ "bitmap",           test_bitmap        },
		{ "onwire-bitmap",    test_bitmap_onwire },
		{ "bob",              test_bob           },
		{ "buf",              m0_ut_lib_buf_test },
		{ "chan",             test_chan          },
		{ "cookie",           test_cookie        },
		{ "finject",          test_finject,      "Dima" },
		{ "getopts",          test_getopts       },
		{ "hash",	      test_hashtable     },
		{ "list",             test_list          },
		{ "locality",         test_locality,     "Nikita" },
		{ "locality-chore",   test_locality_chore, "Nikita" },
		{ "lockers",          test_lockers       },
		{ "memory",           test_memory        },
		{ "misc",             m0_test_misc       },
		{ "mutex",            test_mutex         },
		{ "rwlock",           test_rw            },
		{ "processor",        test_processor     },
		{ "queue",            test_queue         },
		{ "refs",             test_refs          },
		{ "thread",           test_thread        },
		{ "time",             m0_ut_time_test    },
		{ "timer",            test_timer,        "Max" },
		{ "tlist",            test_tlist         },
		{ "trace",            test_trace,        "Dima, Andriy" },
		{ "uuid",             m0_test_lib_uuid   },
		{ "varr",             test_varr          },
		{ "vec",              test_vec,          "Huang Hua"},
		{ "zerovec",          test_zerovec       },
		{ "fold",             test_fold,         "Nikita" },
		{ "tpool",            m0_ut_lib_thread_pool_test },
		{ "combinations",     test_combinations  },
		{ "hash-fnc",         test_hash_fnc,     "Leonid" },
		{ "coroutine",        m0_test_coroutine, "Anatoliy" },
		{ "coroutine2",       m0_test_coroutine2,"Anatoliy" },
		{ NULL,               NULL               }
	}
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
