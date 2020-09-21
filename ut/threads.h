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


#pragma once

#ifndef __MOTR_UT_THREADS_H__
#define __MOTR_UT_THREADS_H__

#include "lib/types.h"	/* size_t */

/**
 * @defgroup ut
 *
 * Multithreaded UT helpers.
 *
 * @{
 */

struct m0_thread;

struct m0_ut_threads_descr {
	void		 (*utd_thread_func)(void *param);
	struct m0_thread  *utd_thread;
	int		   utd_thread_nr;
};

#define M0_UT_THREADS_DEFINE(name, thread_func)				\
static struct m0_ut_threads_descr ut_threads_descr_##name = {		\
	.utd_thread_func = (void (*)(void *))thread_func,		\
};

#define M0_UT_THREADS_START(name, thread_nr, param_array)		\
	m0_ut_threads_start(&ut_threads_descr_##name, thread_nr,	\
			    param_array, sizeof(param_array[0]))

#define M0_UT_THREADS_STOP(name)					\
	m0_ut_threads_stop(&ut_threads_descr_##name)

M0_INTERNAL void m0_ut_threads_start(struct m0_ut_threads_descr *descr,
				     int			 thread_nr,
				     void			*param_array,
				     size_t			 param_size);
M0_INTERNAL void m0_ut_threads_stop(struct m0_ut_threads_descr *descr);


/** @} end of ut group */
#endif /* __MOTR_UT_THREADS_H__ */

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
