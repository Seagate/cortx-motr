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

#ifndef __MOTR_DTM_DOMAIN_H__
#define __MOTR_DTM_DOMAIN_H__


/**
 * @addtogroup dtm
 *
 * @{
 */

#include "lib/tlist.h"
#include "fid/fid.h"

#include "dtm/nucleus.h"             /* m0_dtm_ver_t */
#include "dtm/history.h"             /* m0_dtm_remote */


/* export */
struct m0_dtm_domain;

struct m0_dtm_domain {
	struct m0_dtm_history  dom_hi;
	struct m0_fid          dom_fid;
	m0_dtm_ver_t           dom_ver;
	uint32_t               dom_cohort_nr;
	struct m0_dtm_remote **dom_cohort;
};

struct m0_dtm_domain_cohort {
	struct m0_dtm_object  dco_object;
};

M0_INTERNAL int  m0_dtm_domain_init(struct m0_dtm_domain *dom, uint32_t nr);
M0_INTERNAL void m0_dtm_domain_fini(struct m0_dtm_domain *dom);

M0_INTERNAL void m0_dtm_domain_add(struct m0_dtm_domain *dom,
				   struct m0_dtm_domain_dest *dest);
M0_INTERNAL void m0_dtm_domain_open(struct m0_dtm_domain *dom);
M0_INTERNAL void m0_dtm_domain_close(struct m0_dtm_domain *dom);
M0_INTERNAL void m0_dtm_domain_connect(struct m0_dtm_domain *dom);
M0_INTERNAL void m0_dtm_domain_disconnect(struct m0_dtm_domain *dom);

M0_INTERNAL void m0_dtm_domain_cohort_init(struct m0_dtm_domain_cohort *coh);
M0_INTERNAL void m0_dtm_domain_cohort_fini(struct m0_dtm_domain_cohort *coh);
M0_INTERNAL int  m0_dtm_domain_cohort_open(struct m0_dtm_domain_cohort *coh);
M0_INTERNAL int  m0_dtm_domain_cohort_restart(struct m0_dtm_domain_cohort *coh);

/** @} end of dtm group */

#endif /* __MOTR_DTM_DOMAIN_H__ */


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
