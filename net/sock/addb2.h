/* -*- C -*- */
/*
 * Copyright (c) 2019-2021 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_NET_SOCK_ADDB2_H__
#define __MOTR_NET_SOCK_ADDB2_H__

#include "addb2/identifier.h"

enum {
	M0_AVI_SOCK_BUF_EVENT = M0_AVI_SOCK_RANGE_START,
	M0_AVI_SOCK_POLLER,
	M0_AVI_SOCK_MOVER,
	M0_AVI_SOCK_MOVER_COUNTER,
	M0_AVI_SOCK_MOVER_COUNTER_END = M0_AVI_SOCK_MOVER_COUNTER + 0x100,
	M0_AVI_SOCK_SOCK,
	M0_AVI_SOCK_SOCK_COUNTER,
	M0_AVI_SOCK_SOCK_COUNTER_END = M0_AVI_SOCK_SOCK_COUNTER + 0x100
};

/** @} end of netsock group */
#endif /* __MOTR_NET_SOCK_ADDB2_H__ */

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
