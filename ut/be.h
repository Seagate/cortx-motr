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

#ifndef __MOTR_UT_BE_H__
#define __MOTR_UT_BE_H__

#include "lib/types.h"  /* m0_bcount_t */

/* import */
struct m0_be_tx_credit;
struct m0_be_tx;
struct m0_be_seg;
struct m0_be_ut_backend;
struct m0_be_ut_seg;
struct m0_reqh;
struct m0_sm_group;

/**
 * @addtogroup ut
 *
 * BE helper functions that unit tests of other (non-BE) Motr subsystems
 * are allowed to use.
 *
 * The API declared in be/ut/helper.h is supposed to be used by BE unit
 * tests only. The UTs of other subsystems should not #include that file
 * (though they do, hee hee hee).
 *
 * @{
 */

M0_INTERNAL void m0_ut_backend_init(struct m0_be_ut_backend *be,
				    struct m0_be_ut_seg *seg);

M0_INTERNAL void m0_ut_backend_fini(struct m0_be_ut_backend *be,
				    struct m0_be_ut_seg *seg);

/** Initialises, prepares, and opens the transaction. */
M0_INTERNAL void m0_ut_be_tx_begin(struct m0_be_tx *tx,
				   struct m0_be_ut_backend *ut_be,
				   struct m0_be_tx_credit *cred);

M0_INTERNAL void m0_ut_be_tx_begin2(struct m0_be_tx *tx,
				   struct m0_be_ut_backend *ut_be,
				   struct m0_be_tx_credit *cred,
				   m0_bcount_t payload_cred);

/** Closes the transaction and waits for its completion. */
M0_INTERNAL void m0_ut_be_tx_end(struct m0_be_tx *tx);

/** @} end of ut group */
#endif /* __MOTR_UT_BE_H__ */

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
