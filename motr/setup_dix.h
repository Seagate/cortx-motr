/* -*- C -*- */
/*
 * Copyright (c) 2018-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_MOTR_SETUP_DIX_H__
#define __MOTR_MOTR_SETUP_DIX_H__

/**
 * @addtogroup m0d
 *
 * @{
 */

/**
 * Initialises DIX meta-indices locally.
 *
 * Creates DIX meta-indices involving only local CAS. Returns immediately if
 * there is no CAS service configured for the current process.
 *
 * After running m0_cs_dix_setup() on all m0d's, the DIX state is the same as
 * after running m0dixinit utility. In opposite to m0dixinit, this function
 * can be used to re-initialise a single m0d.
 *
 * XXX Current solution is temporary and contains copy-paste from DIX code.
 * Proper solution must use DIX interface.
 */
M0_INTERNAL int m0_cs_dix_setup(struct m0_motr *cctx);

/** @} end of m0d group */
#endif /* __MOTR_MOTR_SETUP_DIX_H__ */

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
