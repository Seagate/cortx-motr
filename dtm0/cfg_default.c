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

#include "dtm0/cfg_default.h"

#include "dtm0/domain.h"        /* struct m0_dtm0_domain_cfg */

#include "fid/fid.h"            /* M0_FID_TINIT */


M0_INTERNAL int
m0_dtm0_domain_cfg_default_dup(struct m0_dtm0_domain_cfg *dod_cfg, bool mkfs)
{
	*dod_cfg = (struct m0_dtm0_domain_cfg){
		.dodc_log = {
			.dlc_seg0_suffix = "default",
			.dlc_be_domain   = NULL,
			.dlc_btree_fid   = M0_FID_TINIT('b', 1 /* XXX */, 2),
		},
		.dodc_pruner = {
		},
		.dodc_remach = {
		},
		.dodc_pmach = {
		},
		.dodc_net = {
		},
		.dod_create = mkfs,
		.dod_destroy = false,
	};
	return M0_RC(0);
}

M0_INTERNAL struct m0_dtm0_domain_cfg *
m0_dtm0_domain_cfg_dup(struct m0_dtm0_domain_cfg *dod_cfg)
{
	return NULL;
}

M0_INTERNAL void m0_dtm0_domain_cfg_free(struct m0_dtm0_domain_cfg *dod_cfg)
{
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
