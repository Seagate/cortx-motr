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


#pragma once

#ifndef __MOTR_CONF_FLIP_FOMS_H__
#define __MOTR_CONF_FLIP_FOMS_H__

/**
 * @defgroup spiel_foms Fop State Machines for Spiel FOPs
 *
 * Fop state machine for Spiel operations
 * @see fom
 *
 * FOP state machines for various Spiel operation
 *
 * @note Naming convention: For operation xyz, the FOP is named
 * as m0_fop_xyz, its corresponding reply FOP is named as m0_fop_xyz_rep
 * and FOM is named as m0_fom_xyz. For each FOM type, its corresponding
 * create, state and fini methods are named as m0_fom_xyz_create,
 * m0_fom_xyz_state, m0_fom_xyz_fini respectively.
 *
 *  @{
 */

#include "fop/fop.h"
#include "conf/flip_fop.h"
#include "net/net.h"
#include "fop/fom.h"
#include "fop/fom_generic.h"   /* M0_FOPH_NR */


M0_INTERNAL int m0_conf_flip_fom_create(struct m0_fop   *fop,
					struct m0_fom  **out,
					struct m0_reqh  *reqh);

/**
 * Object encompassing FOM for Spiel flip
 * operation and necessary context data
 */
struct m0_conf_flip_fom {
	/** Generic m0_fom object. */
	struct m0_fom clm_gen;
};

/**
 * The various phases for Confd flip FOM.
 * complete FOM and reqh infrastructure is in place.
 */
enum m0_conf_flip_fom_phase {
	M0_FOPH_CONF_FLIP_PREPARE = M0_FOPH_NR + 1,
	M0_FOPH_CONF_APPLY,
 };

/** @} end of spiel_foms */

#endif /* __MOTR_CONF_FLIP_FOMS_H__ */
 /*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
