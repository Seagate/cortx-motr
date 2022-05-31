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


#pragma once

#ifndef __MOTR_LIB_LINUX_KERNEL_TIMER_H__
#define __MOTR_LIB_LINUX_KERNEL_TIMER_H__

#include <linux/timer.h>	/* timer_list */

#include "lib/time.h"		/* m0_time_t */

/**
   @addtogroup timer

   <b>Linux kernel timer.</a>
   @{
 */

struct m0_timer {
	uint64_t            t_magix;
	/** Timer type: M0_TIMER_SOFT or M0_TIMER_HARD. */
	enum m0_timer_type  t_type;
	/** Timer triggers this callback. */
	m0_timer_callback_t t_callback;
	/** User data. It is passed to m0_timer::t_callback(). */
	unsigned long	    t_data;
	/** Expire time in future of this timer. */
	m0_time_t	    t_expire;
	/** Timer state.  Used in state changes checking. */
	enum m0_timer_state t_state;

	/** Kernel timer. */
	struct timer_list t_timer;
};

M0_EXTERN const struct m0_timer_operations m0_timer_ops[];

/** @} end of timer group */

/* __MOTR_LIB_LINUX_KERNEL_TIMER_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
