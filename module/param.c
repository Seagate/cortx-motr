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


#include "module/param.h"
#include "module/instance.h"  /* m0_get */
#include "motr/magic.h"       /* M0_PARAM_SOURCE_MAGIC */

/**
 * @addtogroup module
 *
 * @{
 */

M0_TL_DESCR_DEFINE(m0_param_sources, "m0_param_sources", static,
		   struct m0_param_source, ps_link, ps_magic,
		   M0_PARAM_SOURCE_MAGIC, M0_PARAM_SOURCES_MAGIC);
M0_TL_DEFINE(m0_param_sources, M0_INTERNAL, struct m0_param_source);

static struct m0_tl *param_sources(void)
{
	struct m0_tl *list = &m0_get()->i_param_sources;
	M0_ASSERT(list->t_head.l_head != NULL); /* the list is initialised */
	return list;
}

M0_INTERNAL void *m0_param_get(const char *key)
{
	struct m0_param_source *src;

	m0_tl_for(m0_param_sources, param_sources(), src) {
		void *p = src->ps_param_get(src, key);
		if (p != NULL)
			return p;
	} m0_tl_endfor;
	return NULL;
}

M0_INTERNAL void m0_param_source_add(struct m0_param_source *src)
{
	M0_PRE(src->ps_param_get != NULL);
	m0_param_sources_tlink_init_at_tail(src, param_sources());
}

M0_INTERNAL void m0_param_source_del(struct m0_param_source *src)
{
	m0_param_sources_tlink_del_fini(src);
}

/** @} module */
