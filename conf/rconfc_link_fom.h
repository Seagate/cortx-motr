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

#ifndef __MOTR_CONF_RCONFC_LINK_FOM_H__
#define __MOTR_CONF_RCONFC_LINK_FOM_H__

#include "conf/rconfc_internal.h"

enum m0_rconfc_link_fom_states {
	/* Common */
	M0_RLF_INIT = M0_FOM_PHASE_INIT,
	M0_RLF_FINI = M0_FOM_PHASE_FINISH,
	/* Disconnect */
	M0_RLF_SESS_WAIT_IDLE,
	M0_RLF_SESS_TERMINATING,
	M0_RLF_CONN_TERMINATING,
};

/*
 * The fom type and ops below are exposed here for the sake of m0_fom_init()
 * done inside rconfc_herd_link__on_death_cb().
 */
extern struct m0_fom_type rconfc_link_fom_type;
extern const struct m0_fom_ops rconfc_link_fom_ops;

M0_INTERNAL int  m0_rconfc_mod_init(void);
M0_INTERNAL void m0_rconfc_mod_fini(void);

#endif /* __MOTR_CONF_RCONFC_LINK_FOM_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
