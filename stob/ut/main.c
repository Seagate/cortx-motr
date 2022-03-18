/* -*- C -*- */
/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
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

#include "module/instance.h"	/* m0_get */
#include "module/module.h"
#include "stob/module.h"	/* m0_stob_module__get */

extern void m0_stob_ut_cache(void);
extern void m0_stob_ut_cache_idle_size0(void);
extern void m0_stob_ut_stob_domain_null(void);
extern void m0_stob_ut_stob_null(void);
extern void m0_stob_ut_stob_domain_linux(void);
extern void m0_stob_ut_stob_linux(void);
extern void m0_stob_ut_adieu_linux(void);
extern void m0_stob_ut_stobio_linux(void);
extern void m0_stob_ut_stob_domain_perf(void);
extern void m0_stob_ut_stob_domain_perf_null(void);
extern void m0_stob_ut_stob_perf(void);
extern void m0_stob_ut_stob_perf_null(void);
extern void m0_stob_ut_adieu_perf(void);
extern void m0_stob_ut_stobio_perf(void);
extern void m0_stob_ut_stob_domain_ad(void);
extern void m0_stob_ut_stob_ad(void);
extern void m0_stob_ut_adieu_ad(void);
extern void m0_stob_ut_stob_domain_part(void);
extern void m0_stob_ut_stob_part(void);
extern void m0_stob_ut_stob_part_io(void);
extern void m0_stob_ut_adieu_part(void);

struct m0_ut_suite stob_ut = {
	.ts_name = "stob-ut",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "cache",		m0_stob_ut_cache		},
		{ "cache-idle-size0",	m0_stob_ut_cache_idle_size0	},
#ifndef __KERNEL__
		{ "null-stob-domain",	m0_stob_ut_stob_domain_null	},
		{ "null-stob",		m0_stob_ut_stob_null		},
		{ "linux-stob-domain",	m0_stob_ut_stob_domain_linux	},
		{ "linux-stob",		m0_stob_ut_stob_linux		},
		{ "linux-adieu",	m0_stob_ut_adieu_linux		},
		{ "linux-stobio",	m0_stob_ut_stobio_linux		},
		{ "perf-stob-domain",	m0_stob_ut_stob_domain_perf	},
		{ "perf-stob-domain-null", m0_stob_ut_stob_domain_perf_null },
		{ "perf-stob",		m0_stob_ut_stob_perf		},
		{ "perf-stob-null",	m0_stob_ut_stob_perf_null	},
		{ "perf-adieu",		m0_stob_ut_adieu_perf		},
		{ "perf-stobio",	m0_stob_ut_stobio_perf		},
		{ "ad-stob-domain",	m0_stob_ut_stob_domain_ad	},
		{ "ad-stob",		m0_stob_ut_stob_ad		},
		{ "ad-adieu",		m0_stob_ut_adieu_ad		},
		{ "part-stob-domain",	m0_stob_ut_stob_domain_part	},
		{ "part-stob",		m0_stob_ut_stob_part		},
		{ "part-stob-io",	m0_stob_ut_stob_part_io		},
//		{ "part-adieu",		m0_stob_ut_adieu_part		},
#endif  /* __KERNEL__ */
		{ NULL, NULL }
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
