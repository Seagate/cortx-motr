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

#pragma once
#ifndef __MOTR_CONF_WALK_H__
#define __MOTR_CONF_WALK_H__

struct m0_conf_obj;

/**
 * @defgroup conf_walk
 *
 * @{
 */

/**
 * fn() parameter of m0_conf_walk() should return one of these values.
 * In case of error, negative error code (-Exxx) should be returned.
 */
enum {
	/** Return immediately. */
	M0_CW_STOP,
	/** Continue normally. */
	M0_CW_CONTINUE,
	/**
	 * Skip the subtree that begins at the current entry.
	 * Continue processing with the next sibling.
	 */
	M0_CW_SKIP_SUBTREE,
	/**
	 * Skip siblings of the current entry.
	 * Continue processing in the parent.
	 */
	M0_CW_SKIP_SIBLINGS
};

/**
 * Performs depth-first traversal of the tree of conf objects,
 * starting from `origin', and calls fn() once for each conf object
 * in the tree.
 *
 * fn() should return one of M0_CW_* values (see enum above), or -Exxx
 * in case of error.
 *
 * @pre  m0_conf_cache_is_locked(origin->co_cache)
 */
M0_INTERNAL int m0_conf_walk(int (*fn)(struct m0_conf_obj *obj, void *args),
			     struct m0_conf_obj *origin, void *args);

/** @} conf_walk */
#endif /* __MOTR_CONF_WALK_H__ */
