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

#ifndef __MOTR___DTM0_DOMAIN_H__
#define __MOTR___DTM0_DOMAIN_H__

#include "dtm0/log.h"      /* m0_dtm0_log */
#include "dtm0/pruner.h"   /* m0_dtm0_pruner */
#include "dtm0/pmach.h"    /* m0_dtm0_pmach */
#include "dtm0/remach.h"   /* m0_dtm0_remach */
#include "dtm0/net.h"      /* m0_dtm0_net */
#include "module/module.h" /* m0_module */

struct m0_reqh;
/**
 * @defgroup dtm0
 *
 * @{
 */

/*
 * DTM0 domain overview
 * --------------------
 *
 *   DTM0 domain is a container for DTM0 log, pruner, recovery machine,
 * persistent machine, network. It serves as an entry point for any
 * other component that wants to interact with DTM0. For example,
 * distributed transactions are created within the scope of DTM0 domain.
 */

struct m0_dtm0_domain_cfg {
	struct m0_dtm0_log_cfg    dodc_log;
	struct m0_dtm0_pruner_cfg dodc_pruner;
	struct m0_dtm0_remach_cfg dodc_remach;
	struct m0_dtm0_pmach_cfg  dodc_pmach;
	struct m0_dtm0_net_cfg    dodc_net;
};

struct m0_dtm0_domain_create_cfg {
	struct m0_dtm0_log_create_cfg dcc_log;
};

struct m0_dtm0_domain {
	struct m0_dtm0_log        dod_log;
	struct m0_dtm0_pruner     dod_pruner;
	struct m0_dtm0_remach     dod_remach;
	struct m0_dtm0_pmach      dod_pmach;
	struct m0_dtm0_net        dod_net;
	struct m0_dtm0_domain_cfg dod_cfg;
	struct m0_module          dod_module;
	uint64_t                  dod_magix;
};

M0_INTERNAL int m0_dtm0_domain_init(struct m0_dtm0_domain     *dod,
				    struct m0_dtm0_domain_cfg *dod_cfg);

M0_INTERNAL void m0_dtm0_domain_fini(struct m0_dtm0_domain *dod);

M0_INTERNAL int
m0_dtm0_domain_create(struct m0_dtm0_domain            *dod,
		      struct m0_dtm0_domain_create_cfg *dcc_cfg);

M0_INTERNAL void m0_dtm0_domain_destroy(struct m0_dtm0_domain *dod);

M0_INTERNAL void m0_dtm0_domain_recovered_wait(struct m0_dtm0_domain *dod);

/**
 * Check if this process must send DTM_RECOVERED process event to the HA.
 * As per current protocol, Motr skips sending of DTM_RECOVERED when the
 * configuration does not have DTM0 service (for example, confd process cannot
 * be recovered by DTM0, therefore it does not send DTM_RECOVERED).
 *
 * XXX: reqh argument must be removed when domain configuration gets properly
 * initialised with reqh. Until that moment, we just pass it explicitely.
 *
 * @see ::M0_CONF_HA_PROCESS_DTM_RECOVERED
 */
M0_INTERNAL bool
m0_dtm0_domain_is_recoverable(struct m0_dtm0_domain *dod,
				     struct m0_reqh        *reqh);

/** @} end of dtm0 group */
#endif /* __MOTR___DTM0_DOMAIN_H__ */

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
