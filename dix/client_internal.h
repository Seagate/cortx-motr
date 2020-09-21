/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_DIX_CLIENT_INTERNAL_H__
#define __MOTR_DIX_CLIENT_INTERNAL_H__

/**
 * @defgroup dix
 *
 * @{
 */

/* Import */
struct m0_dix_cli;
struct m0_dix;

/**
 * Fills 'out' structure with root index fid and layout descriptor.
 * User is responsible to finalise 'out' after usage.
 */
M0_INTERNAL int m0_dix__root_set(const struct m0_dix_cli *cli,
				 struct m0_dix           *out);

/**
 * Fills 'out' structure with "layout" index fid and layout descriptor.
 * User is responsible to finalise 'out' after usage.
 */
M0_INTERNAL int m0_dix__layout_set(const struct m0_dix_cli *cli,
				   struct m0_dix           *out);

/**
 * Fills 'out' structure with "layout-descr" index fid and layout descriptor.
 * User is responsible to finalise 'out' after usage.
 */
M0_INTERNAL int m0_dix__ldescr_set(const struct m0_dix_cli *cli,
				   struct m0_dix           *out);

/**
 * Finds pool version structure by pool version fid specified in index layout
 * descriptor.
 *
 * @pre dix->dd_layout.dl_type == DIX_LTYPE_DESCR
 */
M0_INTERNAL struct m0_pool_version *m0_dix_pver(const struct m0_dix_cli *cli,
						const struct m0_dix     *dix);

/** @} end of dix group */
#endif /* __MOTR_DIX_CLIENT_INTERNAL_H__ */

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
