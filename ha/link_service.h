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

#ifndef __MOTR_HA_LINK_SERVICE_H__
#define __MOTR_HA_LINK_SERVICE_H__

/**
 * @defgroup ha
 *
 * @{
 */

struct m0_chan;
struct m0_reqh;
struct m0_reqh_service;
struct m0_reqh_service_type;
struct m0_ha_link;

extern struct m0_reqh_service_type m0_ha_link_service_type;

M0_INTERNAL int  m0_ha_link_service_init(struct m0_reqh_service **hl_service,
                                         struct m0_reqh          *reqh);
M0_INTERNAL void m0_ha_link_service_fini(struct m0_reqh_service *hl_service);

/** Find link by link id and increase the link reference counter */
M0_INTERNAL struct m0_ha_link *
m0_ha_link_service_find_get(struct m0_reqh_service  *service,
                            const struct m0_uint128 *link_id,
                            struct m0_uint128       *connection_id);
M0_INTERNAL void m0_ha_link_service_put(struct m0_reqh_service *service,
                                        struct m0_ha_link      *hl);

M0_INTERNAL void
m0_ha_link_service_register(struct m0_reqh_service  *service,
                            struct m0_ha_link       *hl,
                            const struct m0_uint128 *link_id,
                            const struct m0_uint128 *connection_id);
M0_INTERNAL void m0_ha_link_service_deregister(struct m0_reqh_service *service,
                                               struct m0_ha_link      *hl);
M0_INTERNAL void m0_ha_link_service_quiesce(struct m0_reqh_service *service,
                                            struct m0_ha_link      *hl,
                                            struct m0_chan         *chan);

M0_INTERNAL int  m0_ha_link_service_mod_init(void);
M0_INTERNAL void m0_ha_link_service_mod_fini(void);

/** @} end of ha group */
#endif /* __MOTR_HA_LINK_SERVICE_H__ */

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
