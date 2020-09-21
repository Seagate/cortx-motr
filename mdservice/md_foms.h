/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_MDSERVICE_MD_FOMS_H__
#define __MOTR_MDSERVICE_MD_FOMS_H__

#include "mdservice/md_fops_xc.h"

struct m0_fom;
struct m0_fop;
struct m0_fid;

struct m0_cob;
struct m0_cob_nskey;
struct m0_cob_oikey;

struct m0_fom_md {
	/** Generic m0_fom object. */
	struct m0_fom      fm_fom;
	/** FOL record fragment to be added for meta-data operations. */
	struct m0_fol_frag fm_fol_frag;
};

enum m0_md_fom_phases {
        M0_FOPH_MD_GENERIC = M0_FOPH_NR + 1
};

M0_INTERNAL int m0_md_fop_init(struct m0_fop *fop, struct m0_fom *fom);

/**
   Init request fom for all types of requests.
*/
M0_INTERNAL int m0_md_req_fom_create(struct m0_fop *fop, struct m0_fom **m,
				     struct m0_reqh *reqh);
M0_INTERNAL int m0_md_rep_fom_create(struct m0_fop *fop, struct m0_fom **m,
				     struct m0_reqh *reqh);

#endif /* __MOTR_MDSERVICE_MD_FOMS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
