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

#ifndef __MOTR_SNS_CM_UT_CP_COMMON_H__
#define __MOTR_SNS_CM_UT_CP_COMMON_H__

#include "ut/ut.h"
#include "sns/cm/cp.h"
#include "sns/cm/ag.h"

extern struct m0_motr sctx;

/* Populates the bufvec with a character value. */
void bv_populate(struct m0_bufvec *b, char data, uint32_t seg_nr,
		 uint32_t seg_size);
void bv_alloc_populate(struct m0_bufvec *b, char data, uint32_t seg_nr,
		       uint32_t seg_size);

/* Compares 2 bufvecs and asserts if not equal. */
void bv_compare(struct m0_bufvec *b1, struct m0_bufvec *b2, uint32_t seg_nr,
		uint32_t seg_size);

void bv_free(struct m0_bufvec *b);

void cp_prepare(struct m0_cm_cp *cp, struct m0_net_buffer *buf,
		uint32_t bv_seg_nr, uint32_t bv_seg_size,
                struct m0_sns_cm_ag *sns_ag, char data,
		struct m0_fom_ops *cp_fom_ops, struct m0_reqh *reqh,
		uint64_t cp_ag_idx, bool is_acc_cp, struct m0_cm *scm);
struct m0_sns_cm *reqh2snscm(struct m0_reqh *reqh);

void layout_gen(struct m0_pdclust_layout **pdlay, struct m0_reqh *reqh);
void layout_destroy(struct m0_pdclust_layout *pdlay);

int cs_init(struct m0_motr *sctx);
int cs_init_with_ad_stob(struct m0_motr *sctx);
void cs_fini(struct m0_motr *sctx);

void pool_mach_transit(struct m0_reqh *reqh, struct m0_poolmach *pm,
			uint64_t fd, enum m0_pool_nd_state state);

#endif /* __MOTR_SNS_CM_UT_CP_COMMON_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
