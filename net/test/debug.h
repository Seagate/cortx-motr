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


#include "lib/trace.h"	/* M0_LOG */

/**
   @defgroup NetTestDebugDFS Debugging tools
   @ingroup NetTestDFS

   Usage recipe
   0. in .c file
   1. #define NET_TEST_MODULE_NAME name_of_net_test_module
   2. #include "net/test/debug.h"
   3. Use LOGD() macro for regular debug output.
   4. #undef NET_TEST_MODULE_NAME
      in the end of file (or scope in which LOGD() is needed)
      (because of altogether build mode).
   5. Enable/disable debug output from any point using
      LOGD_VAR_NAME(some_module_name) variable
      (declared using LOGD_VAR_DECLARE(some_module_name))
      Debug output is disabled by default.

   Macro names used: LOGD, NET_TEST_MODULE_NAME, LOGD_VAR_DECLARE, LOGD_VAR_NAME
   @note Include guards are not needed in this file because
   LOGD_VAR_NAME(NET_TEST_MODULE_NAME) variable should be defined
   for each module that includes this file.

   @{
 */

#ifndef NET_TEST_MODULE_NAME
M0_BASSERT("NET_TEST_MODULE_NAME should be defined "
	   "before including debug.h" == NULL);
#endif

#ifndef LOGD_VAR_NAME
#define LOGD_VAR_NAME(module_name)					\
	M0_CAT(m0_net_test_logd_, module_name)
#endif

/**
   There is one variable per inclusion of this file.
   It is useful to enable/disable module debug messages from any point in code.
 */
bool LOGD_VAR_NAME(NET_TEST_MODULE_NAME) = false;

#ifndef LOGD_VAR_DECLARE
#define LOGD_VAR_DECLARE(module_name)					\
	extern bool LOGD_VAR_NAME(module_name);
#endif

#undef LOGD
#define LOGD(...)							\
	do {								\
		if (LOGD_VAR_NAME(NET_TEST_MODULE_NAME))		\
			M0_LOG(M0_DEBUG, __VA_ARGS__);			\
	} while (0)


/**
   @} end of NetTestDebugDFS group
 */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
