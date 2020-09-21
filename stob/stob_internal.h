/* -*- C -*- */
/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
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
#ifndef __MOTR_STOB_STOB_INTERNAL_H__
#define __MOTR_STOB_STOB_INTERNAL_H__

/**
 * @defgroup stob Storage object
 *
 * @{
 */

#include "lib/types.h"	/* uint64_t */

enum m0_stob_state;
struct m0_stob;
struct m0_stob_domain;

M0_INTERNAL void m0_stob__id_set(struct m0_stob *stob,
				 const struct m0_fid *stob_fid);
M0_INTERNAL void m0_stob__state_set(struct m0_stob *stob,
				    enum m0_stob_state state);

M0_INTERNAL void m0_stob__cache_evict(struct m0_stob *stob);

/** @} end of stob group */
#endif /* __MOTR_STOB_STOB_INTERNAL_H__ */

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
