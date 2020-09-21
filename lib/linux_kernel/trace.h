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



#pragma once

#ifndef __MOTR_LIB_LINUX_KERNEL_TRACE_H__
#define __MOTR_LIB_LINUX_KERNEL_TRACE_H__

#include "lib/atomic.h"
#include "lib/types.h"


/**
 * @addtogroup trace
 *
 * @{
 */

/** Trace statistics */
struct m0_trace_stats {
	/** total number of trace records generated since program start */
	struct m0_atomic64  trs_rec_total;
	/** number of trace records generated withing last second */
	uint32_t            trs_rec_per_sec;
	/** amount of trace data generated withing last second, in bytes */
	uint32_t            trs_bytes_per_sec;
	/** average observed value of trs_rec_per_sec */
	uint32_t            trs_avg_rec_per_sec;
	/** average observed value of trs_bytes_per_sec */
	uint32_t            trs_avg_bytes_per_sec;
	/** maximum observed value of trs_rec_per_sec */
	uint32_t            trs_max_rec_per_sec;
	/** maximum observed value of trs_bytes_per_sec */
	uint32_t            trs_max_bytes_per_sec;
	/** average observed value of trace record size */
	uint32_t            trs_avg_rec_size;
	/** maximum observed value of trace record size */
	uint32_t            trs_max_rec_size;
};

M0_INTERNAL const struct m0_trace_stats *m0_trace_get_stats(void);

/** @} end of trace group */

#endif /* __MOTR_LIB_LINUX_KERNEL_TRACE_H__ */


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
