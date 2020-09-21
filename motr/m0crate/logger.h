/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_M0CRATE_LOGGER_H__
#define __MOTR_M0CRATE_LOGGER_H__

#include <stdarg.h>       /* va_list */
#include <stdio.h>        /* vfprintf(), stderr */

/**
 * @defgroup crate_logger
 *
 * @{
 */

enum cr_log_level {
	CLL_ERROR	= 0,
	CLL_WARN	= 1,
	CLL_INFO	= 2,
	CLL_TRACE	= 3,
	CLL_DEBUG	= 4,
	CLL_SAME	= -1,
};
void cr_log(enum cr_log_level lev, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
void cr_log_ex(enum cr_log_level lev,
	       const char *pre,
	       const char *post,
	       const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));
void cr_vlog(enum cr_log_level lev, const char *fmt, va_list args);
void cr_set_debug_level(enum cr_log_level level);

#define crlog(level, ...) cr_log_ex(level, LOG_PREFIX, "\n", __VA_ARGS__)

/** @} end of crate_logger group */
#endif /* __MOTR_M0CRATE_LOGGER_H__ */

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
