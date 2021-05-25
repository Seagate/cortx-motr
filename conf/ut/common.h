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

#ifndef __MOTR_CONF_UT_COMMON_H__
#define __MOTR_CONF_UT_COMMON_H__

#include "conf/confc.h"  /* m0_confc_ctx */
#include "lib/chan.h"    /* m0_clink */

#define SERVER_ENDPOINT_ADDR "0@lo:12345:34:1"
#define SERVER_ENDPOINT      "lnet:" SERVER_ENDPOINT_ADDR
#define CLIENT_ENDPOINT_ADDR "0@lo:12345:34:*"

extern struct m0_conf_cache m0_conf_ut_cache;
extern struct m0_sm_group   m0_conf_ut_grp;

struct m0_conf_ut_waiter {
	struct m0_confc_ctx w_ctx;
	struct m0_clink     w_clink;
};

M0_INTERNAL void m0_conf_ut_waiter_init(struct m0_conf_ut_waiter *w,
					struct m0_confc *confc);
M0_INTERNAL void m0_conf_ut_waiter_fini(struct m0_conf_ut_waiter *w);
M0_INTERNAL int m0_conf_ut_waiter_wait(struct m0_conf_ut_waiter *w,
				       struct m0_conf_obj **result);

M0_INTERNAL int m0_conf_ut_ast_thread_init(void);
M0_INTERNAL int m0_conf_ut_ast_thread_fini(void);

M0_INTERNAL int m0_conf_ut_cache_init(void);
M0_INTERNAL int m0_conf_ut_cache_fini(void);

#ifndef __KERNEL__
M0_INTERNAL void m0_conf_ut_cache_from_file(struct m0_conf_cache *cache,
					    const char *path);
#endif

#endif /* __MOTR_CONF_UT_COMMON_H__ */
