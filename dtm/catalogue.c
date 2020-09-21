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



/**
 * @addtogroup dtm
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM
#include "lib/trace.h"
#include "lib/assert.h"
#include "lib/errno.h"                 /* ENOENT */
#include "motr/magic.h"

#include "dtm/history.h"
#include "dtm/catalogue.h"

M0_TL_DESCR_DEFINE(cat, "catalogue", M0_INTERNAL,
		   struct m0_dtm_history, h_catlink,
		   h_hi.hi_ups.t_magic, M0_DTM_HI_MAGIX, M0_DTM_CAT_MAGIX);
M0_TL_DEFINE(cat, M0_INTERNAL, struct m0_dtm_history);

M0_INTERNAL void m0_dtm_catalogue_init(struct m0_dtm_catalogue *cat)
{
	cat_tlist_init(&cat->ca_el);
}

M0_INTERNAL void m0_dtm_catalogue_fini(struct m0_dtm_catalogue *cat)
{
	struct m0_dtm_history *history;

	m0_tl_for(cat, &cat->ca_el, history) {
		cat_tlist_del(history);
	} m0_tl_endfor;
	cat_tlist_fini(&cat->ca_el);
}

M0_INTERNAL int m0_dtm_catalogue_create(struct m0_dtm_catalogue *cat)
{
	return 0;
}

M0_INTERNAL int m0_dtm_catalogue_delete(struct m0_dtm_catalogue *cat)
{
	return 0;
}

M0_INTERNAL int m0_dtm_catalogue_lookup(struct m0_dtm_catalogue *cat,
					const struct m0_uint128 *id,
					struct m0_dtm_history **out)
{
	*out = m0_tl_find(cat, history, &cat->ca_el,
			  m0_uint128_eq(history->h_ops->hio_id(history), id));
	return *out != NULL ? 0 : -ENOENT;
}

M0_INTERNAL int m0_dtm_catalogue_add(struct m0_dtm_catalogue *cat,
				     struct m0_dtm_history *history)
{
	cat_tlist_add(&cat->ca_el, history);
	return 0;
}

M0_INTERNAL int m0_dtm_catalogue_del(struct m0_dtm_catalogue *cat,
				     struct m0_dtm_history *history)
{
	cat_tlist_del(history);
	return 0;
}

M0_INTERNAL int m0_dtm_catalogue_find(struct m0_dtm_catalogue *cat,
				      struct m0_dtm *dtm,
				      const struct m0_uint128 *id,
				      m0_dtm_catalogue_alloc_t *alloc,
				      void *datum,
				      struct m0_dtm_history **out)
{
	int result;

	result = m0_dtm_catalogue_lookup(cat, id, out);
	if (result == -ENOENT) {
		*out = alloc(dtm, id, datum);
		if (*out != NULL) {
			m0_dtm_catalogue_add(cat, *out);
			result = 0;
		} else
			result = M0_ERR(-ENOMEM);
	}
	return result;
}

/** @} end of dtm group */

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
