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

#ifndef __MOTR_DTM_DTX_H__
#define __MOTR_DTM_DTX_H__


/**
 * @addtogroup dtm
 *
 * @{
 */

/* import */
#include "lib/types.h"                /* m0_uint128, uint32_t, uint64_t */
struct m0_dtm_dtx_party;
struct m0_dtm;

/* export */
struct m0_dtm_dtx;

struct m0_dtm_dtx {
	struct m0_dtm           *dt_dtm;
	struct m0_uint128        dt_id;
	uint32_t                 dt_nr;
	uint32_t                 dt_nr_max;
	uint32_t                 dt_nr_fixed;
	struct m0_dtm_dtx_party *dt_party;
};

struct m0_dtm_dtx_srv {
	struct m0_uint128     ds_id;
	struct m0_dtm_history ds_history;
};

M0_INTERNAL int m0_dtm_dtx_init(struct m0_dtm_dtx *dtx,
				const struct m0_uint128 *id,
				struct m0_dtm *dtm, uint32_t nr_max);
M0_INTERNAL void m0_dtm_dtx_fini(struct m0_dtm_dtx *dtx);

M0_INTERNAL void m0_dtm_dtx_add(struct m0_dtm_dtx *dtx,
				struct m0_dtm_oper *oper);
M0_INTERNAL void m0_dtm_dtx_close(struct m0_dtm_dtx *dtx);

M0_EXTERN const struct m0_dtm_history_type m0_dtm_dtx_htype;
M0_EXTERN const struct m0_dtm_history_type m0_dtm_dtx_srv_htype;

/** @} end of dtm group */

#endif /* __MOTR_DTM_DTX_H__ */

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
