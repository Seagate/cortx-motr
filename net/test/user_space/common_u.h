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

#ifndef __MOTR_NET_TEST_USER_SPACE_COMMON_U_H__
#define __MOTR_NET_TEST_USER_SPACE_COMMON_U_H__

#include <stdio.h>		/* printf */

#include "lib/time.h"		/* m0_time_t */

/**
   @defgroup NetTestUCommonDFS Common user-space routines
   @ingroup NetTestDFS

   @see @ref net-test

   @{
 */

extern bool m0_net_test_u_printf_verbose;

#define M0_VERBOSEFLAGARG M0_FLAGARG('v', "Verbose output",		      \
				     &m0_net_test_u_printf_verbose)
#define M0_IFLISTARG(pflag) M0_FLAGARG('l', "List available LNET interfaces", \
				       pflag)

char *m0_net_test_u_str_copy(const char *str);
void m0_net_test_u_str_free(char *str);
/** perror */
void m0_net_test_u_print_error(const char *s, int code);
void m0_net_test_u_print_s(const char *fmt, const char *str);
void m0_net_test_u_print_time(char *name, m0_time_t time);
void m0_net_test_u_lnet_info(void);
void m0_net_test_u_print_bsize(double bsize);

int m0_net_test_u_printf(const char *fmt, ...);
int m0_net_test_u_printf_v(const char *fmt, ...);

/**
   @} end of NetTestUCommonDFS group
 */

#endif /* __MOTR_NET_TEST_USER_SPACE_COMMON_U_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
