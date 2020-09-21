/* -*- C -*- */
/*
 * Copyright (c) 2019-2020 Seagate Technology LLC and/or its Affiliates
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


/**
 * @addtogroup ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "ha/cookie.h"
#include "ut/ut.h"

#include "lib/memory.h" /* M0_ALLOC_PTR */


void m0_ha_ut_cookie(void)
{
	struct m0_ha_cookie_xc *hc_xc;
	struct m0_ha_cookie    *a;
	struct m0_ha_cookie    *b;

	M0_ALLOC_PTR(a);
	M0_UT_ASSERT(a != NULL);
	m0_ha_cookie_init(a);
	M0_UT_ASSERT(m0_ha_cookie_is_eq(a, &m0_ha_cookie_no_record));
	m0_ha_cookie_record(a);
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, &m0_ha_cookie_no_record));
	m0_ha_cookie_fini(a);
	m0_free(a);

	M0_ALLOC_PTR(hc_xc);
	M0_UT_ASSERT(hc_xc != NULL);
	M0_ALLOC_PTR(a);
	M0_UT_ASSERT(a != NULL);
	M0_ALLOC_PTR(b);
	M0_UT_ASSERT(b != NULL);

	m0_ha_cookie_init(a);
	m0_ha_cookie_init(b);

	M0_UT_ASSERT(m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_to_xc(a, hc_xc);
	m0_ha_cookie_record(a);
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_from_xc(a, hc_xc);
	M0_UT_ASSERT(m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_from_xc(b, hc_xc);
	M0_UT_ASSERT(m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_record(a);
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_record(b);
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_from_xc(a, hc_xc);
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_from_xc(b, hc_xc);
	M0_UT_ASSERT(m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_record(a);
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_record(b);
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_to_xc(a, hc_xc);
	m0_ha_cookie_from_xc(b, hc_xc);
	M0_UT_ASSERT(m0_ha_cookie_is_eq(a, b));
	*a = m0_ha_cookie_no_record;
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_record(a);
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, b));

	m0_ha_cookie_fini(b);
	m0_ha_cookie_fini(a);

	m0_free(b);
	m0_free(a);
	m0_free(hc_xc);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of ha group */

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
