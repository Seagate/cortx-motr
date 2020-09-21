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

#ifndef __MOTR_DESIM_CNT_H__
#define __MOTR_DESIM_CNT_H__

/**
   @addtogroup desim desim
   @{
 */

#include "lib/tlist.h"

typedef unsigned long long cnt_t;

struct cnt {
	cnt_t               c_sum;
	cnt_t               c_min;
	cnt_t               c_max;
	cnt_t               c_nr;
	double              c_sq;
	char               *c_name;
	struct m0_tlink     c_linkage;
	struct cnt         *c_parent;
	uint64_t            c_magic;
};

M0_INTERNAL void
cnt_init(struct cnt *cnt, struct cnt *parent, const char *name, ...)
              __attribute__((format(printf, 3, 4)));
M0_INTERNAL void cnt_fini(struct cnt *cnt);
M0_INTERNAL void cnt_dump(struct cnt *cnt);
M0_INTERNAL void cnt_dump_all(void);

M0_INTERNAL void cnt_mod(struct cnt *cnt, cnt_t val);

M0_INTERNAL void cnt_global_init(void);
M0_INTERNAL void cnt_global_fini(void);

#endif /* __MOTR_DESIM_CNT_H__ */

/** @} end of desim group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
