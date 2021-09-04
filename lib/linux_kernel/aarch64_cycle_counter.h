/*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 */

#pragma once

#ifndef __MOTR_LIB_LINUX_CCNT_A_H__
#define __MOTR_LIB_LINUX_CCNT_A_H__

/**
 *   This function starts the cycle counter functionlity in each CPU on the node
 *   at the time of cluster startup
 *   This is needed for performance measurement.
 */
int  start_cycle_counter(void);

/**
 *  This function stops the cycle counter functionlity in each CPU on the node
 *  at the time of cluster shutdown
 */
void finish_cycle_counter(void);

#endif /* __MOTR_LIB_LINUX_CCNT_A_H__ */
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
