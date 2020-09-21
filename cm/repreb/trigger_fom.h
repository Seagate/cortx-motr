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

#ifndef __MOTR_CM_REPREB_TRIGGER_FOM_H__
#define __MOTR_CM_REPREB_TRIGGER_FOM_H__

/**
 * @defgroup CM
 *
 * @{
 */
#include "cm/repreb/cm.h"
#include "lib/types.h"
#include "fop/fom_generic.h"   /* M0_FOPH_NR */

struct m0_fom;

struct m0_fom_trigger_ops {
	struct m0_fop_type* (*fto_type)(uint32_t op);
	uint64_t (*fto_progress)(struct m0_fom *fom, bool reinit_counter);
	void (*fto_prepare)(struct m0_fom *fom);
};

struct m0_trigger_fom {
	const struct m0_fom_trigger_ops *tf_ops;
	struct m0_fom                    tf_fom;
};

#ifndef __KERNEL__

enum m0_trigger_phases {
	M0_TPH_PREPARE = M0_FOPH_NR + 1,
	M0_TPH_READY,
	M0_TPH_START,
	M0_TPH_FINI = M0_FOM_PHASE_FINISH
};

#endif

M0_INTERNAL int m0_trigger_fom_create(struct m0_trigger_fom  *tfom,
				      struct m0_fop          *fop,
				      struct m0_reqh         *reqh);
/** @} end of CM group */
#endif /* __MOTR_CM_REPREB_TRIGGER_FOM_H__ */

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
