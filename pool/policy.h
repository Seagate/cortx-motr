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
#ifndef __MOTR_POOL_POLICY_H__
#define __MOTR_POOL_POLICY_H__

#include "lib/tlist.h"
#include "xcode/xcode.h"  /* M0_XCA_ENUM */

/**
  * @defgroup pool_policy Pool Versions Selection Policy.
  *
  * @{
  */

struct m0_pool;
struct m0_pool_version;
struct m0_pools_common;

/** Pool version policy codes. */
enum m0_pver_policy_code {
	M0_PVER_POLICY_FIRST_AVAILABLE,
	M0_PVER_POLICY_NR
} M0_XCA_ENUM;

/** Pool version policy. */
struct m0_pver_policy {
	const struct m0_pver_policy_type *pp_type;
	const struct m0_pver_policy_ops  *pp_ops;
};

struct m0_pver_policy_type_ops {
	/** Creates concrete pver policy instance. */
	int (*ppto_create)(struct m0_pver_policy **out);
};

struct m0_pver_policy_type {
	uint64_t                              ppt_magic;
	const char                           *ppt_name;
	const enum m0_pver_policy_code        ppt_code;
	const struct m0_pver_policy_type_ops *ppt_ops;
	struct m0_tlink                       ppt_link;
};

/** Pool version policy operations. */
struct m0_pver_policy_ops {
	/** Initialise pool version policy. */
	int (*ppo_init)(struct m0_pver_policy *pver_policy);

	/** Finalise pool version policy. It destruct policy object. */
	void (*ppo_fini)(struct m0_pver_policy *pver_policy);

	/**
	 * It finds pool version depending on pool version
	 * policy. Policy need to implement this function
	 * to find out pool versions for new object.
	 */
	int (*ppo_get)(struct m0_pools_common  *pc,
		       const struct m0_pool    *pool,
		       struct m0_pool_version **pver);
};

 /** Global registry of pool version policies. */
struct m0_pver_policies {
	/** List of known types of pool version policies. */
	struct m0_tl          pp_types;
	/** "Current" pool version policy. */
	struct m0_pver_policy pp_cur_policy;
};

M0_INTERNAL int m0_pver_policies_init(void);
M0_INTERNAL void m0_pver_policies_fini(void);

/** Get pool version policy type. */
M0_INTERNAL struct m0_pver_policy_type *
m0_pver_policy_type_find(enum m0_pver_policy_code code);

/** Number of policies registered */
M0_INTERNAL int m0_pver_policy_types_nr(void);

 /** Register pool version policy type. */
M0_INTERNAL int m0_pver_policy_type_register(struct m0_pver_policy_type *type);

/** Unregister pool version policy type. */
M0_INTERNAL
void m0_pver_policy_type_deregister(struct m0_pver_policy_type *type);

/** @} pool_policy */
#endif /* __MOTR_POOL_POLICY_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
