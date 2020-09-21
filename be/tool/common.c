/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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
 * @addtogroup be
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "module/instance.h"    /* m0 */

struct m0_betool_common_ctx {
	struct m0 bcc_instance;
};

static struct m0_betool_common_ctx betool_common_ctx;

/* copy-paste from utils/m0ut.c */
void m0_betool_m0_init(void)
{
	int rc;

	m0_instance_setup(&betool_common_ctx.bcc_instance);
	rc = m0_module_init(&betool_common_ctx.bcc_instance.i_self,
			    M0_LEVEL_INST_READY);
	M0_ASSERT(rc == 0);
}

void m0_betool_m0_fini(void)
{
	m0_module_fini(&m0_get()->i_self, M0_MODLEV_NONE);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of be group */

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
