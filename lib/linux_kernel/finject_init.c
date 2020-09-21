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


#ifdef ENABLE_FAULT_INJECTION

#include <linux/version.h> /* LINUX_VERSION_CODE */
#include <linux/kernel.h>  /* UINT_MAX */
#include <linux/random.h>  /* random32 */
#include <linux/sched.h>   /* current */

#include "lib/mutex.h"     /* m0_mutex */
#include "lib/time.h"      /* m0_time_now */
#include "lib/finject.h"
#include "lib/finject_internal.h"

enum {
	FI_RAND_PROB_SCALE = 100,
	FI_RAND_SCALE_UNIT = UINT_MAX / FI_RAND_PROB_SCALE,
};

M0_INTERNAL int m0_fi_init(void)
{
	m0_mutex_init(&fi_states_mutex);
	fi_states_init();
	return 0;
}

M0_INTERNAL void m0_fi_fini(void)
{
	fi_states_fini();
	m0_mutex_fini(&fi_states_mutex);
}

/**
 * Returns random value in range [0..FI_RAND_PROB_SCALE]
 */
M0_INTERNAL uint32_t fi_random(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	u32 rnd     = prandom_u32();
#else
	u32 rnd     = random32();
#endif
	u32 roundup = rnd % FI_RAND_SCALE_UNIT ? 1 : 0;

	return rnd / FI_RAND_SCALE_UNIT + roundup;
}

#else /* ENABLE_FAULT_INJECTION */

M0_INTERNAL int m0_fi_init(void)
{
	return 0;
}

M0_INTERNAL void m0_fi_fini(void)
{
}

#endif /* ENABLE_FAULT_INJECTION */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
