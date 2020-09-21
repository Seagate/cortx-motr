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

#ifndef __MOTR_LNET_DRV_UT_H__
#define __MOTR_LNET_DRV_UT_H__

enum {
	UT_TEST_NONE       =   0, /**< no test requested, user program idles */
	UT_TEST_DEV        =   1, /**< device registered */
	UT_TEST_OPEN       =   2, /**< open/close */
	UT_TEST_RDWR       =   3, /**< read/write */
	UT_TEST_BADIOCTL   =   4, /**< invalid ioctl */
	UT_TEST_DOMINIT    =   5, /**< open/dominit/close */
	UT_TEST_TMS        =   6, /**< multi-TM start/stop and no cleanup */
	UT_TEST_DUPTM      =   7, /**< duplicate TM start */
	UT_TEST_TMCLEANUP  =   8, /**< multi-TM start with cleanup */
	UT_TEST_MAX        =   8, /**< final implemented test ID */

	UT_TEST_DONE       = 127, /**< done testing, no user response */

	UT_USER_READY = 'r',	  /**< user program is ready */
	UT_USER_SUCCESS = 'y',	  /**< current test succeeded in user space */
	UT_USER_FAIL = 'n',	  /**< current test failed in user space */

	MULTI_TM_NR = 3,
};

/** The /proc file used to coordinate driver unit test */
#define UT_PROC_NAME "m0_lnet_ut"

#endif /* __MOTR_LNET_DRV_UT_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
