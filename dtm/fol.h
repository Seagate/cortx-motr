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

#ifndef __MOTR_DTM_FOL_H__
#define __MOTR_DTM_FOL_H__


/**
 * @addtogroup dtm
 *
 * @{
 */

/* import */
#include "dtm/history.h"
struct m0_dtm;
struct m0_dtm_remote;

/* export */
struct m0_dtm_fol;

struct m0_dtm_fol {
	struct m0_dtm_controlh fo_ch;
};

M0_INTERNAL void m0_dtm_fol_init(struct m0_dtm_fol *fol, struct m0_dtm *dtm);
M0_INTERNAL void m0_dtm_fol_fini(struct m0_dtm_fol *fol);
M0_INTERNAL void m0_dtm_fol_add(struct m0_dtm_fol *fol,
				struct m0_dtm_oper *oper);

M0_EXTERN const struct m0_dtm_history_type m0_dtm_fol_htype;

struct m0_dtm_fol_remote {
	struct m0_dtm_controlh rfo_ch;
};

M0_INTERNAL void m0_dtm_fol_remote_init(struct m0_dtm_fol_remote *frem,
					struct m0_dtm *dtm,
					struct m0_dtm_remote *remote);
M0_INTERNAL void m0_dtm_fol_remote_fini(struct m0_dtm_fol_remote *frem);
M0_INTERNAL void m0_dtm_fol_remote_add(struct m0_dtm_fol_remote *frem,
				       struct m0_dtm_oper *oper);

M0_EXTERN const struct m0_dtm_history_type m0_dtm_fol_remote_htype;

/** @} end of dtm group */

#endif /* __MOTR_DTM_FOL_H__ */

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
