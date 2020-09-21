/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_DTM_CATALOGUE_H__
#define __MOTR_DTM_CATALOGUE_H__


/**
 * @addtogroup dtm
 *
 * @{
 */
/* import */
#include "lib/tlist.h"
struct m0_uint128;
struct m0_dtm_history;
struct m0_dtm;
struct m0_uint128;

/* export */
struct m0_dtm_catalogue;

struct m0_dtm_catalogue {
	struct m0_tl ca_el;
};

M0_INTERNAL void m0_dtm_catalogue_init(struct m0_dtm_catalogue *cat);
M0_INTERNAL void m0_dtm_catalogue_fini(struct m0_dtm_catalogue *cat);
M0_INTERNAL int m0_dtm_catalogue_create(struct m0_dtm_catalogue *cat);
M0_INTERNAL int m0_dtm_catalogue_delete(struct m0_dtm_catalogue *cat);
M0_INTERNAL int m0_dtm_catalogue_lookup(struct m0_dtm_catalogue *cat,
					const struct m0_uint128 *id,
					struct m0_dtm_history **out);
M0_INTERNAL int m0_dtm_catalogue_add(struct m0_dtm_catalogue *cat,
				     struct m0_dtm_history *history);
M0_INTERNAL int m0_dtm_catalogue_del(struct m0_dtm_catalogue *cat,
				     struct m0_dtm_history *history);
typedef struct m0_dtm_history *
m0_dtm_catalogue_alloc_t(struct m0_dtm *, const struct m0_uint128 *, void *);

M0_INTERNAL int m0_dtm_catalogue_find(struct m0_dtm_catalogue *cat,
				      struct m0_dtm *dtm,
				      const struct m0_uint128 *id,
				      m0_dtm_catalogue_alloc_t *alloc,
				      void *datum,
				      struct m0_dtm_history **out);

/** @} end of dtm group */

#endif /* __MOTR_DTM_CATALOGUE_H__ */


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
