/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM0
#include "lib/trace.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "reqh/reqh_service.h"
#include "reqh/reqh.h"               /* m0_reqh */
#include "be/domain.h"
#include "be/dtm0_log.h"
#include "module/instance.h"         /* m0_get */


M0_INTERNAL struct m0_reqh_service *
m0_dtm__client_service_start(struct m0_reqh *reqh, struct m0_fid *cli_srv_fid)
{
       struct m0_reqh_service_type *svct;
       struct m0_reqh_service      *reqh_svc;
       int rc;

       svct = m0_reqh_service_type_find("M0_CST_DTM0");
       M0_ASSERT(svct != NULL);

       rc = m0_reqh_service_allocate(&reqh_svc, svct, NULL);
       M0_ASSERT(rc == 0);

       m0_reqh_service_init(reqh_svc, reqh, cli_srv_fid);

       rc = m0_reqh_service_start(reqh_svc);
       M0_ASSERT(rc == 0);

       return reqh_svc;
}

M0_INTERNAL void m0_dtm__client_service_stop(struct m0_reqh_service *svc)
{
       m0_reqh_service_prepare_to_stop(svc);
       m0_reqh_idle_wait_for(svc->rs_reqh, svc);
       m0_reqh_service_stop(svc);
       m0_reqh_service_fini(svc);
}

static int dtm0_log_init(struct m0_be_domain *dom, const char *suffix,
			 const struct m0_buf *data)
{
	struct m0_be_dtm0_log *log0 = *(struct m0_be_dtm0_log**)data->b_addr;
	struct m0_reqh        *reqh = dom->bd_cfg.bc_engine.bec_reqh;
	unsigned               key = m0_get()->i_dtm0_log_key;

	M0_ENTRY("suffix: %s, data: %p, log0: %p", suffix, data->b_addr, log0);

	if (m0_reqh_lockers_get(reqh, key) == NULL)
		m0_reqh_lockers_set(reqh, key, log0);

	return M0_RC(0);
}

static void dtm0_log_fini(struct m0_be_domain *dom, const char *suffix,
			  const struct m0_buf *data)
{
	M0_ENTRY();
	M0_LEAVE();
}

struct m0_be_0type m0_be_dtm0 = {
	.b0_name = "M0_BE:DTM_LOG",
	.b0_init = dtm0_log_init,
	.b0_fini = dtm0_log_fini,
};

M0_INTERNAL int m0_dtm0_log_create(struct m0_sm_group  *grp,
				   struct m0_be_domain *bedom,
				   struct m0_be_seg    *seg)
{
	static const char      *logid = "0001";
	struct m0_be_tx_credit  cred = {};
	struct m0_be_dtm0_log  *log;
	struct m0_be_tx        *tx;
	struct m0_buf           data = {};
	int                     rc;

	M0_ALLOC_PTR(tx);
	if (tx == NULL)
		return M0_ERR(-ENOMEM);

	m0_be_0type_add_credit(bedom, &m0_be_dtm0, logid, &data, &cred);
	M0_BE_ALLOC_CREDIT_PTR(log, seg, &cred);
	m0_be_tx_credit_add(&cred, &M0_BE_TX_CREDIT_PTR(log));

	m0_be_tx_init(tx, 0, bedom, grp, NULL, NULL, NULL, NULL);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_exclusive_open_sync(tx);
	if (rc != 0)
		goto tx_fini;

	M0_BE_ALLOC_PTR_SYNC(log, seg, tx);
	if (log == NULL)
		goto tx_fini;

	/* TODO: as a part of log-related patches add the following code:
	        persistent_log_init(log);
		M0_BE_TX_CAPTURE_PTR(log);
	 */

	data = M0_BUF_INIT_PTR(&log);
	rc = m0_be_0type_add(&m0_be_dtm0, bedom, tx, logid, &data);
	if (log == NULL)
		goto tx_fini;

	m0_be_tx_close_sync(tx);
tx_fini:
	m0_be_tx_fini(tx);
	m0_free(tx);
	return M0_RC(rc);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
