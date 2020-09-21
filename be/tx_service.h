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
#ifndef __MOTR_BE_TX_SERVICE_H__
#define __MOTR_BE_TX_SERVICE_H__

#include "reqh/reqh_service.h"

/**
 * @defgroup be Meta-data back-end
 * @{
 */

extern struct m0_reqh_service_type m0_be_txs_stype;

M0_INTERNAL int m0_be_txs_register(void);
M0_INTERNAL void m0_be_txs_unregister(void);

M0_INTERNAL int  m0_be_tx_service_init(struct m0_be_engine *en,
				       struct m0_reqh *reqh);
M0_INTERNAL void m0_be_tx_service_fini(struct m0_be_engine *en);

/** @} end of be group */
#endif /* __MOTR_BE_TX_SERVICE_H__ */

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
