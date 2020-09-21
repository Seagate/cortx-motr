/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_ADDB2_GLOBAL_H__
#define __MOTR_ADDB2_GLOBAL_H__

/**
 * @defgroup addb2
 *
 * @{
 */

struct m0_addb2_sys;
struct m0_addb2_counter;

M0_INTERNAL int  m0_addb2_global_init(void);
M0_INTERNAL void m0_addb2_global_fini(void);
M0_INTERNAL struct m0_addb2_sys *m0_addb2_global_get(void);
M0_INTERNAL void m0_addb2_global_thread_enter(void);
M0_INTERNAL void m0_addb2_global_thread_leave(void);

/** @} end of addb2 group */
#endif /* __MOTR_ADDB2_GLOBAL_H__ */

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
