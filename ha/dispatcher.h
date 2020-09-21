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

#ifndef __MOTR_HA_DISPATCHER_H__
#define __MOTR_HA_DISPATCHER_H__

/**
 * @defgroup ha-dispatcher
 *
 * @{
 */

#include "lib/types.h"          /* bool */
#include "lib/tlist.h"          /* m0_tl */
#include "module/module.h"      /* m0_module */

struct m0_ha;
struct m0_ha_link;
struct m0_ha_msg;
struct m0_ha_note_handler;
struct m0_ha_keepalive_handler;
struct m0_ha_fvec_handler;

struct m0_ha_dispatcher_cfg {
	bool hdc_enable_note;
	bool hdc_enable_keepalive;
	bool hdc_enable_fvec;
};

struct m0_ha_dispatcher {
	struct m0_ha_dispatcher_cfg     hds_cfg;
	struct m0_module                hds_module;
	/*
	 * Is not protected by any lock.
	 * User is responsible for non-concurrent modifications.
	 * Handlers can be added only between m0_motr_ha_init() and
	 * m0_motr_ha_start().
	 */
	struct m0_tl                    hds_handlers;
	/* m0_ha_note_set(), m0_ha_note_get() handler */
	struct m0_ha_note_handler      *hds_note_handler;
	struct m0_ha_fvec_handler      *hds_fvec_handler;
	struct m0_ha_keepalive_handler *hds_keepalive_handler;
};

struct m0_ha_handler {
	struct m0_tlink   hh_link;
	uint64_t          hh_magic;
	void             *hh_data;
	void            (*hh_msg_received_cb)(struct m0_ha_handler *hh,
	                                      struct m0_ha         *ha,
	                                      struct m0_ha_link    *hl,
	                                      struct m0_ha_msg     *msg,
	                                      uint64_t              tag,
	                                      void                 *data);
};

M0_INTERNAL int m0_ha_dispatcher_init(struct m0_ha_dispatcher     *hd,
                                      struct m0_ha_dispatcher_cfg *hd_cfg);
M0_INTERNAL void m0_ha_dispatcher_fini(struct m0_ha_dispatcher *hd);

M0_INTERNAL void m0_ha_dispatcher_attach(struct m0_ha_dispatcher *hd,
                                         struct m0_ha_handler    *hh);
M0_INTERNAL void m0_ha_dispatcher_detach(struct m0_ha_dispatcher *hd,
                                         struct m0_ha_handler    *hh);

M0_INTERNAL void m0_ha_dispatcher_handle(struct m0_ha_dispatcher *hd,
                                         struct m0_ha            *ha,
                                         struct m0_ha_link       *hl,
                                         struct m0_ha_msg        *msg,
                                         uint64_t                 tag);

/** @} end of ha-dispatcher group */
#endif /* __MOTR_HA_DISPATCHER_H__ */

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
