/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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
#ifndef __MOTR_DTM0_CLK_SRC_H__
#define __MOTR_DTM0_CLK_SRC_H__

#include "xcode/xcode.h" /* xcode attrs */
#include "lib/time.h"    /* m0_time_t */
#include "lib/mutex.h"   /* m0_mutex */

/*
 * DTM clock source overview
 * -------------------------
 *
 *   Clock source module is designed to be used by DTM0. At this point,
 * only one type of clock is supported -- physical clock. Such a clock
 * provides a way to enforce total ordering in a system with synchronised
 * clocks.
 * XXX: At this point the clock source module is a thin wrapper around
 * ::m0_time_now (which is enough to satisfy the requirements from [1]),
 * however later on it might be extended to support logical clocks [2].
 *
 * @see [1] "A.clock.sync" in the DTM0 HLD.
 * @see [2] https://github.com/Seagate/cortx-motr/blob/clock/mw/clock.h
 *
 * Interface
 * ---------
 *
 * CS.TS          -- A data type that represents a point in time (timestamp).
 * CS.TS.CMP(L,R) -- a 3-way comparison for timestamps; it returns
 *                   -1 when "left" happened before "right", +1 when
 *                   "left" happened after "right", and 0 otherwise.
 * CS.INIT        -- initialise a clock source of the given type.
 * CS.FINI        -- finalise a clock source.
 * CS.NOW         -- get the current value of a clock source.
 *
 * Concurrency
 * -----------
 *
 *   It is safe to call CS.NOW from any context as long as the caller owns the
 * corresponding source clock.
 *   The user must ensure to use only one type of clock source in the system.
 * The module provides no protection against such cases.
 */


/**
 * @defgroup dtm
 *
 * @{
 */


enum m0_dtm0_cs_types {
	/*
	 * Physical clock.
	 * Physical clock supports total ordering and requires
	 * no synchronisation (from DTM0 perspective). But it has
	 * one drawback: two dependent events could have the same
	 * timestamp (it might be resolved with help of unique comparable
	 * clock ids).
	 * It is assumed that the clock drift between nodes does not exceed
	 * some "reasonable" values (for example, the upper bound for the
	 * duration of a transient failure). If such assumption is not enforced
	 * then user should always check the clock drift value.
	 */
	M0_DTM0_CS_PHYS,
};

/** A data type that represents a timestamp (see CS.TS).*/
struct m0_dtm0_ts {
	/* TODO: Think about adding enum cs_types here */
	m0_time_t dts_phys;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Defines the minimal valid value for a CS.TS. */
#define M0_DTM0_TS_MIN (struct m0_dtm0_ts) { .dts_phys = 1 }
/** Defines the maximal valid value for a CS.TS. */
#define M0_DTM0_TS_MAX (struct m0_dtm0_ts) { .dts_phys = (UINT64_MAX - 1) }
/** Defines an invalid (but defined) value for a CS.TS. */
#define M0_DTM0_TS_INIT (struct m0_dtm0_ts) { .dts_phys = UINT64_MAX }

#define DTS0_P(_ts) ((_ts)->dts_phys)
#define DTS0_F "@%" PRIu64

/**
 * Timestamp ordering is the result of comparison of two timestamps
 * (see CS.TS.CMP for the details).
 */
enum m0_dtm0_ts_ord {
	/* Left happened before Right */
	M0_DTS_LT = -1,
	/* Left and Right happened at the same time */
	M0_DTS_EQ = 0,
	/* Left happened after Right */
	M0_DTS_GT = 1,
};

struct m0_dtm_clk_src_ops;

/** Instance of a specific clock source. */
struct m0_dtm0_clk_src {
	const struct m0_dtm0_clk_src_ops *cs_ops;
	struct m0_dtm0_ts                 cs_last;
	struct m0_mutex                   cs_phys_lock;
};

/** Compares two timestamps. See CS.TS.CMP */
M0_INTERNAL enum m0_dtm0_ts_ord m0_dtm0_ts_cmp(const struct m0_dtm0_clk_src *cs,
					       const struct m0_dtm0_ts *left,
					       const struct m0_dtm0_ts *right);

/** Initalises a clock source. See CS.INIT */
M0_INTERNAL void m0_dtm0_clk_src_init(struct m0_dtm0_clk_src *cs,
				      enum m0_dtm0_cs_types   type);

/** Finalises a clock source. See CS.FINI */
M0_INTERNAL void m0_dtm0_clk_src_fini(struct m0_dtm0_clk_src *cs);

/** Returns the current clock source value. See CS.NOW */
M0_INTERNAL void m0_dtm0_clk_src_now(struct m0_dtm0_clk_src *cs,
				     struct m0_dtm0_ts      *now);

M0_INTERNAL bool m0_dtm0_ts__invariant(const struct m0_dtm0_ts *ts);

/** @} end of dtm group */
#endif /* __MOTR_DTM0_CLK_SRC_H__ */

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
