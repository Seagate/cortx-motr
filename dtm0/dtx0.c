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
 * @addtogroup dtm0
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM0
#include "lib/trace.h"

#include "dtm0/dtx0.h"
#include "dtm0/domain.h" /* m0_dtm0_domain */

M0_INTERNAL void m0_dtx0_redo_add_credit(struct m0_dtm0_domain  *dod,
					 struct m0_dtm0_redo    *redo,
					 struct m0_be_tx_credit *accum)
{
	m0_dtm0_log_redo_add_credit(&dod->dod_log, redo, accum);
}

M0_INTERNAL int m0_dtx0_redo_add(struct m0_dtm0_domain *dod,
				 struct m0_be_tx       *tx,
				 struct m0_dtm0_redo   *redo,
				 const struct m0_fid   *sdev)
{
	return m0_dtm0_log_redo_add(&dod->dod_log, tx, redo, sdev);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of dtm0 group */

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
