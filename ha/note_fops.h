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

#ifndef __MOTR___HA_NOTE_FOPS_H__
#define __MOTR___HA_NOTE_FOPS_H__

#include "fop/fop.h"
#include "xcode/xcode_attr.h"

#include "ha/note.h"
#include "ha/note_xc.h"

/**
 * @addtogroup ha-note
 * @{
 */

/**
 * FOP sent between Motr and Halon to exchange object state changes.
 * See ha/note.h.
 */
struct m0_ha_state_fop {
	/** Error code for reply, ignored in request. */
	int32_t           hs_rc;
	/** Objects and (optionally) their states. */
	struct m0_ha_nvec hs_note;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

M0_INTERNAL int m0_ha_state_fop_init(void);
M0_INTERNAL void m0_ha_state_fop_fini(void);

extern struct m0_fop_type m0_ha_state_get_fopt;
extern struct m0_fop_type m0_ha_state_get_rep_fopt;
extern struct m0_fop_type m0_ha_state_set_fopt;

extern const struct m0_fom_type_ops *m0_ha_state_get_fom_type_ops;
extern const struct m0_fom_type_ops *m0_ha_state_set_fom_type_ops;

/** @} END of ha-note */
#endif /* __MOTR___HA_NOTE_FOPS_H__ */

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
