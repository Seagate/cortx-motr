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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "ha/cookie.h"

#include "lib/misc.h"   /* M0_IS0 */
#include "lib/thread.h" /* m0_process */
#include "lib/types.h"  /* UINT64_MAX */
#include "lib/uuid.h"   /* m0_uuid_generate */


const struct m0_ha_cookie m0_ha_cookie_no_record = {
	.hc_pid        = UINT64_MAX,
	.hc_time_start = UINT64_MAX,
	.hc_uptime     = UINT64_MAX,
	.hc_uuid       = M0_UINT128(UINT64_MAX, UINT64_MAX),
};

#define HACO_F "(pid=%" PRIu64 " time_start="TIME_F" uptime="TIME_F" " \
	       "uuid="U128X_F")"
#define HACO_P(_hc)         (_hc)->hc_pid,         \
		     TIME_P((_hc)->hc_time_start), \
		     TIME_P((_hc)->hc_uptime),     \
		    U128_P(&(_hc)->hc_uuid)


M0_INTERNAL void m0_ha_cookie_init(struct m0_ha_cookie *hc)
{
	M0_PRE(M0_IS0(hc));
	*hc = m0_ha_cookie_no_record;
	M0_LEAVE("hc="HACO_F, HACO_P(hc));
}

M0_INTERNAL void m0_ha_cookie_fini(struct m0_ha_cookie *hc)
{
	M0_ENTRY("hc="HACO_F, HACO_P(hc));
}

M0_INTERNAL void m0_ha_cookie_record(struct m0_ha_cookie *hc)
{
	M0_PRE(m0_ha_cookie_is_eq(hc, &m0_ha_cookie_no_record));
	*hc = (struct m0_ha_cookie){
		hc->hc_pid        = m0_process(),
		hc->hc_time_start = m0_time_now(),
		hc->hc_uptime     = M0_TIME_NEVER, // XXX FIXME
	};
	m0_uuid_generate(&hc->hc_uuid);
	M0_LEAVE("hc="HACO_F, HACO_P(hc));
}

M0_INTERNAL bool m0_ha_cookie_is_eq(const struct m0_ha_cookie *a,
                                    const struct m0_ha_cookie *b)
{
	M0_LOG(M0_DEBUG, "a="HACO_F, HACO_P(a));
	M0_LOG(M0_DEBUG, "b="HACO_F, HACO_P(b));
	return a->hc_pid        == b->hc_pid &&
	       a->hc_time_start == b->hc_time_start &&
	       a->hc_uptime     == b->hc_uptime &&
	       m0_uint128_eq(&a->hc_uuid, &b->hc_uuid);
}

M0_INTERNAL void m0_ha_cookie_from_xc(struct m0_ha_cookie          *hc,
                                      const struct m0_ha_cookie_xc *hc_xc)
{
	*hc = (struct m0_ha_cookie){
		.hc_pid        = hc_xc->hcx_pid,
		.hc_time_start = hc_xc->hcx_time_start,
		.hc_uptime     = hc_xc->hcx_uptime,
		.hc_uuid       = hc_xc->hcx_uuid,
	};
}

M0_INTERNAL void m0_ha_cookie_to_xc(const struct m0_ha_cookie *hc,
                                    struct m0_ha_cookie_xc    *hc_xc)
{
	*hc_xc = (struct m0_ha_cookie_xc){
		.hcx_pid        = hc->hc_pid,
		.hcx_time_start = hc->hc_time_start,
		.hcx_uptime     = hc->hc_uptime,
		.hcx_uuid       = hc->hc_uuid,
	};
}

#undef HACO_P
#undef HACO_F

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
