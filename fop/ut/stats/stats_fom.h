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

#ifndef __MOTR_FOP_UT_STATS_FOM_H__
#define __MOTR_FOP_UT_STATS_FOM_H__

#include "fop/fom.h"
#include "fop/fom_generic.h"


enum fom_stats_phase {
	PH_INIT = M0_FOM_PHASE_INIT,  /*< fom has been initialised. */
	PH_FINISH = M0_FOM_PHASE_FINISH,  /*< terminal phase. */
	PH_RUN
};

/**
 * Object encompassing FOM for stats
 * operation and necessary context data
 */
struct fom_stats {
	/** Generic m0_fom object. */
	struct m0_fom fs_gen;
};

#endif /* __MOTR_FOP_UT_STATS_FOM_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
