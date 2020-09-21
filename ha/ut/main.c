/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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

#include "ut/ut.h"

extern void m0_ha_ut_cookie(void);

extern void m0_ha_ut_msg_queue(void);

extern void m0_ha_ut_lq(void);
extern void m0_ha_ut_lq_mark_delivered(void);

extern void m0_ha_ut_link_usecase(void);
extern void m0_ha_ut_link_multithreaded(void);
extern void m0_ha_ut_link_reconnect_simple(void);
extern void m0_ha_ut_link_reconnect_multiple(void);

extern void m0_ha_ut_entrypoint_usecase(void);
extern void m0_ha_ut_entrypoint_client(void);

extern void m0_ha_ut_ha_usecase(void);

struct m0_ut_suite ha_ut = {
	.ts_name = "ha-ut",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "cookie",                 &m0_ha_ut_cookie                  },
		{ "msg_queue",              &m0_ha_ut_msg_queue               },
		{ "lq",                     &m0_ha_ut_lq                      },
		{ "lq-mark_delivered",      &m0_ha_ut_lq_mark_delivered       },
		{ "link-usecase",           &m0_ha_ut_link_usecase            },
		{ "link-multithreaded",     &m0_ha_ut_link_multithreaded      },
		{ "link-reconnect_simple",  &m0_ha_ut_link_reconnect_simple   },
		{ "link-reconnect_multiple",&m0_ha_ut_link_reconnect_multiple },
		{ "entrypoint-usecase",     &m0_ha_ut_entrypoint_usecase      },
		{ "entrypoint-client",      &m0_ha_ut_entrypoint_client       },
		{ "ha-usecase",             &m0_ha_ut_ha_usecase              },
		{ NULL, NULL },
	},
};

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
