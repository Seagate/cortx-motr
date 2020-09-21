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

#ifndef __MOTR_M0CRATE_CRATE_MOTR_UTILS_H__
#define __MOTR_M0CRATE_CRATE_MOTR_UTILS_H__

#include "motr/m0crate/crate_client.h"

/**
 * @defgroup crate_utils
 *
 * @{
 */
int adopt_motr_thread(struct m0_workload_task *task);

void release_motr_thread(struct m0_workload_task *task);

struct m0_realm *crate_uber_realm();

extern struct m0_client *m0_instance;

int init(struct workload *w);
int fini(struct workload *w);
void check(struct workload *w);

/** @} end of crate_utils group */
#endif /* __MOTR_M0CRATE_CRATE_UTILS_H__ */

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
