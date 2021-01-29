/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNS
#include "lib/trace.h"
#include "lib/misc.h"
#include "lib/memory.h"
#include "lib/assert.h"
#include "sns/parity_ops.h"

M0_INTERNAL void m0_parity_fini(void)
{
#ifdef __KERNEL__
	galois_calc_tables_release();
#endif /* __KERNEL__ */
}

M0_INTERNAL int m0_parity_init(void)
{
#ifdef __KERNEL__
	int ret = galois_create_mult_tables(M0_PARITY_GALOIS_W);
	M0_ASSERT(ret == 0);
#endif /* __KERNEL__ */
	return 0;
}

M0_INTERNAL m0_parity_elem_t m0_parity_pow(m0_parity_elem_t x,
					   m0_parity_elem_t p)
{
	m0_parity_elem_t ret = x;
	int i = 1;

	if (p == 0)
		return 1;

	for (i = 1; i < p; ++i)
		ret = m0_parity_mul(ret, x);

	return ret;
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
