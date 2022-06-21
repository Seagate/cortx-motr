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

#include "dtm0/domain.h"

#include "lib/bob.h"            /* M0_BOB_DEFINE */
#include "module/instance.h"    /* m0_get */
#include "conf/helpers.h"       /* m0_conf_process2service_get */
#include "reqh/reqh.h"          /* m0_reqh2confc */

static const struct m0_bob_type dtm0_domain_bob_type = {
	.bt_name         = "m0_dtm0_domain",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_dtm0_domain, dod_magix),
	.bt_magix        = M0_DTM0_DOMAIN_MAGIC,
};
M0_BOB_DEFINE(static, &dtm0_domain_bob_type, m0_dtm0_domain);

static struct m0_dtm0_domain *dtm0_module2domain(struct m0_module *module)
{
	return bob_of(module, struct m0_dtm0_domain, dod_module,
	              &dtm0_domain_bob_type);
}

static int dtm0_domain_level_enter(struct m0_module *module);
static void dtm0_domain_level_leave(struct m0_module *module);

#define DTM0_DOMAIN_LEVEL(level) [level] = {            \
		.ml_name  = #level,                     \
		.ml_enter = &dtm0_domain_level_enter,    \
		.ml_leave = &dtm0_domain_level_leave,    \
	}

enum dtm0_domain_level {
	M0_DTM0_DOMAIN_LEVEL_INIT,

	M0_DTM0_DOMAIN_LEVEL_LOG_MKFS,
	M0_DTM0_DOMAIN_LEVEL_LOG_INIT,

	M0_DTM0_DOMAIN_LEVEL_REMACH_INIT,
	M0_DTM0_DOMAIN_LEVEL_PMACH_INIT,
	M0_DTM0_DOMAIN_LEVEL_SERVICE_INIT,
	M0_DTM0_DOMAIN_LEVEL_NET_INIT,

	M0_DTM0_DOMAIN_LEVEL_REMACH_START,
	M0_DTM0_DOMAIN_LEVEL_PMACH_START,
	M0_DTM0_DOMAIN_LEVEL_PRUNER_INIT,

	M0_DTM0_DOMAIN_LEVEL_READY,
};

static const struct m0_modlev levels_dtm0_domain[] = {
	DTM0_DOMAIN_LEVEL(M0_DTM0_DOMAIN_LEVEL_INIT),

	DTM0_DOMAIN_LEVEL(M0_DTM0_DOMAIN_LEVEL_LOG_MKFS),
	DTM0_DOMAIN_LEVEL(M0_DTM0_DOMAIN_LEVEL_LOG_INIT),

	DTM0_DOMAIN_LEVEL(M0_DTM0_DOMAIN_LEVEL_REMACH_INIT),
	DTM0_DOMAIN_LEVEL(M0_DTM0_DOMAIN_LEVEL_PMACH_INIT),
	DTM0_DOMAIN_LEVEL(M0_DTM0_DOMAIN_LEVEL_SERVICE_INIT),
	DTM0_DOMAIN_LEVEL(M0_DTM0_DOMAIN_LEVEL_NET_INIT),

	DTM0_DOMAIN_LEVEL(M0_DTM0_DOMAIN_LEVEL_REMACH_START),
	DTM0_DOMAIN_LEVEL(M0_DTM0_DOMAIN_LEVEL_PMACH_START),
	DTM0_DOMAIN_LEVEL(M0_DTM0_DOMAIN_LEVEL_PRUNER_INIT),

	DTM0_DOMAIN_LEVEL(M0_DTM0_DOMAIN_LEVEL_READY),
};
#undef DTM0_DOMAIN_LEVEL

static int dtm0_domain_level_enter(struct m0_module *module)
{
	enum dtm0_domain_level     level = module->m_cur + 1;
	struct m0_dtm0_domain     *dod = dtm0_module2domain(module);
	struct m0_dtm0_domain_cfg *cfg = &dod->dod_cfg;

	M0_ENTRY("dod=%p level=%d level_name=%s",
	         dod, level, levels_dtm0_domain[level].ml_name);
	switch (level) {
	case M0_DTM0_DOMAIN_LEVEL_INIT:
		return M0_RC(0);
	case M0_DTM0_DOMAIN_LEVEL_LOG_MKFS:
		return M0_RC(0);
	case M0_DTM0_DOMAIN_LEVEL_LOG_INIT:
		return M0_RC(m0_dtm0_log_init(&dod->dod_log,
					      &cfg->dodc_log));

	case M0_DTM0_DOMAIN_LEVEL_REMACH_INIT:
		return M0_RC(m0_dtm0_remach_init(&dod->dod_remach,
						 &cfg->dodc_remach));
	case M0_DTM0_DOMAIN_LEVEL_PMACH_INIT:
		return M0_RC(m0_dtm0_pmach_init(&dod->dod_pmach,
						&cfg->dodc_pmach));
	case M0_DTM0_DOMAIN_LEVEL_SERVICE_INIT:
		return M0_RC(0);
	case M0_DTM0_DOMAIN_LEVEL_NET_INIT:
		return M0_RC(m0_dtm0_net_init(&dod->dod_net, &cfg->dodc_net));
	case M0_DTM0_DOMAIN_LEVEL_REMACH_START:
		m0_dtm0_remach_start(&dod->dod_remach);
		return M0_RC(0);
	case M0_DTM0_DOMAIN_LEVEL_PMACH_START:
		m0_dtm0_pmach_start(&dod->dod_pmach);
		return M0_RC(0);
	case M0_DTM0_DOMAIN_LEVEL_PRUNER_INIT:
		return M0_RC(m0_dtm0_pruner_init(&dod->dod_pruner,
						 &cfg->dodc_pruner));
	case M0_DTM0_DOMAIN_LEVEL_READY:
		return M0_RC(0);
	default:
		M0_IMPOSSIBLE("Unexpected level: %d", level);
	}
}

static void dtm0_domain_level_leave(struct m0_module *module)
{
	enum dtm0_domain_level  level = module->m_cur;
	struct m0_dtm0_domain  *dod = dtm0_module2domain(module);

	M0_ENTRY("dod=%p level=%d level_name=%s",
	         dod, level, levels_dtm0_domain[level].ml_name);
	switch (level) {
	case M0_DTM0_DOMAIN_LEVEL_INIT:
		break;
	case M0_DTM0_DOMAIN_LEVEL_LOG_MKFS:
		break;
	case M0_DTM0_DOMAIN_LEVEL_LOG_INIT:
		m0_dtm0_log_fini(&dod->dod_log);
		break;
	case M0_DTM0_DOMAIN_LEVEL_REMACH_INIT:
		m0_dtm0_remach_fini(&dod->dod_remach);
		break;
	case M0_DTM0_DOMAIN_LEVEL_PMACH_INIT:
		m0_dtm0_pmach_fini(&dod->dod_pmach);
		break;
	case M0_DTM0_DOMAIN_LEVEL_SERVICE_INIT:
		break;
	case M0_DTM0_DOMAIN_LEVEL_NET_INIT:
		m0_dtm0_net_fini(&dod->dod_net);
		break;

	case M0_DTM0_DOMAIN_LEVEL_REMACH_START:
		m0_dtm0_remach_stop(&dod->dod_remach);
		break;
	case M0_DTM0_DOMAIN_LEVEL_PMACH_START:
		m0_dtm0_pmach_stop(&dod->dod_pmach);
		break;
	case M0_DTM0_DOMAIN_LEVEL_PRUNER_INIT:
		m0_dtm0_pruner_fini(&dod->dod_pruner);
		break;

	case M0_DTM0_DOMAIN_LEVEL_READY:
		break;
	default:
		M0_IMPOSSIBLE("Unexpected level: %d", level);
	}
}

M0_INTERNAL int m0_dtm0_domain_init(struct m0_dtm0_domain     *dod,
				    struct m0_dtm0_domain_cfg *dod_cfg)
{
	int rc;

	M0_ENTRY("dod=%p dod_cfg=%p", dod, dod_cfg);
	m0_module_setup(&dod->dod_module, "m0_dtm0_domain module",
	                levels_dtm0_domain, ARRAY_SIZE(levels_dtm0_domain),
	                m0_get());
	/*
	 * TODO use m0_dtm0_domain_cfg_dup to copy the configuration
	 * into dod::dod_cfg.
	 */
	m0_dtm0_domain_bob_init(dod);
	rc = m0_module_init(&dod->dod_module, M0_DTM0_DOMAIN_LEVEL_READY);
	if (rc != 0)
		m0_module_fini(&dod->dod_module, M0_MODLEV_NONE);
	return M0_RC(rc);
}

M0_INTERNAL void m0_dtm0_domain_fini(struct m0_dtm0_domain *dod)
{
	m0_module_fini(&dod->dod_module, M0_MODLEV_NONE);
	m0_dtm0_domain_bob_fini(dod);
}

M0_INTERNAL int m0_dtm0_domain_create(struct m0_dtm0_domain            *dod,
				      struct m0_dtm0_domain_create_cfg *dc_cfg)
{
	return 0;
}

M0_INTERNAL void m0_dtm0_domain_destroy(struct m0_dtm0_domain *dod)
{
}

M0_INTERNAL void m0_dtm0_domain_recovered_wait(struct m0_dtm0_domain *dod)
{
	M0_ENTRY();
	M0_LEAVE();
}

static bool has_in_conf(struct m0_reqh *reqh)
{
	struct m0_confc     *confc = m0_reqh2confc(reqh);
	struct m0_conf_root *root;
	struct m0_fid        svc_fid = {};
	int                  rc;

	rc = m0_confc_root_open(confc, &root);
	if (rc == 0) {
		rc = m0_conf_process2service_get(confc, &reqh->rh_fid,
						 M0_CST_DTM0, &svc_fid);
		m0_confc_close(&root->rt_obj);
	}
	return rc == 0 && m0_fid_is_set(&svc_fid);
}

M0_INTERNAL bool
m0_dtm0_domain_is_recoverable(struct m0_dtm0_domain *dod,
			      struct m0_reqh        *reqh)
{
	struct m0_confc * confc;

	(void)dod;
	/*
	 * XXX:
	 * Recovery machine is the one who sends RECOVERED event
	 * when DTM0 is enabled. Therefore, domain should not send it.
	 * Once recovery machine (dtm/recovery branch) is merged
	 * and enabled by default, this workaround should be removed.
	 */
	if (ENABLE_DTM0)
		return false;

	M0_LOG(M0_DEBUG, "Recovery machine is not here yet.");
	confc = m0_reqh2confc(reqh);
	return confc != NULL && m0_confc_is_inited(confc) && has_in_conf(reqh);
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
