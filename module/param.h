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
#ifndef __MOTR_MODULE_PARAM_H__
#define __MOTR_MODULE_PARAM_H__

#include "lib/tlist.h"  /* m0_tlink */

/**
 * @addtogroup module
 *
 * m0_param API is used to configure initialisation: Motr modules use
 * m0_param_get() to obtain external information.
 *
 * @{
 */

/**
 * Obtains the value of parameter associated with given key.
 * Returns NULL if no such value exists or an error occurs.
 */
M0_INTERNAL void *m0_param_get(const char *key);

/**
 * Source of parameters.
 *
 * Some (but not all) possible sources of parameters are:
 *
 * - "env": ->ps_param_get() uses getenv(3) to obtain the values of
 *   environment variables;
 *
 * - "kv pairs": ->ps_param_get() scans a set of KV pairs, which is
 *   accessible through an object ambient to m0_param_source;
 *
 * - "confc": ->ps_param_get() translates the key into confc path
 *   (a sequence of m0_fids) and accesses conf object and/or its field;
 *
 * - "argv": ->ps_param_get() looks for `-K key=val' in argv[]
 *   and returns the pointer to `val'.
 */
struct m0_param_source {
	void         *(*ps_param_get)(const struct m0_param_source *src,
				      const char *key);
	/** Linkage to m0::i_param_sources. */
	struct m0_tlink ps_link;
	uint64_t        ps_magic;
};

M0_TL_DECLARE(m0_param_sources, M0_INTERNAL, struct m0_param_source);

/** Appends new element to m0::i_param_sources. */
M0_INTERNAL void m0_param_source_add(struct m0_param_source *src);

/** Deletes `src' from m0::i_param_sources. */
M0_INTERNAL void m0_param_source_del(struct m0_param_source *src);

/** @} module */
#endif /* __MOTR_MODULE_PARAM_H__ */
