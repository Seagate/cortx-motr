/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ISCS
#include "lib/trace.h"

#include "reqh/reqh_service.h"
#include "reqh/reqh.h"
#include "iscservice/isc_service.h"
#include "iscservice/isc_fops.h"
#include "lib/hash.h"
#include "module/instance.h"         /* m0_get() */
#include "lib/memory.h"
#include "fid/fid.h"
#include "motr/magic.h"
#include "iscservice/isc.h"

static int iscs_allocate(struct m0_reqh_service **service,
                         const struct m0_reqh_service_type *stype);
static void iscs_fini(struct m0_reqh_service *service);

static int iscs_start(struct m0_reqh_service *service);
static void iscs_stop(struct m0_reqh_service *service);

static bool comp_key_eq(const void *key1, const void *key2)
{
	return m0_fid_eq(key1, key2);
}

static uint64_t comp_hash_func(const struct m0_htable *htable, const void *k)
{
	return m0_fid_hash(k) % htable->h_bucket_nr;
}

M0_HT_DESCR_DEFINE(m0_isc, "Hash table for compute functions", M0_INTERNAL,
		   struct m0_isc_comp, ic_hlink, ic_magic,
		   M0_ISC_COMP_MAGIC, M0_ISC_TLIST_HEAD_MAGIC,
		   ic_fid, comp_hash_func, comp_key_eq);

M0_HT_DEFINE(m0_isc, M0_INTERNAL, struct m0_isc_comp, struct m0_fid);

enum {
	ISC_HT_BUCKET_NR = 100,
};

static const struct m0_reqh_service_type_ops iscs_type_ops = {
	.rsto_service_allocate = iscs_allocate
};

static const struct m0_reqh_service_ops iscs_ops = {
	.rso_start       = iscs_start,
	.rso_start_async = m0_reqh_service_async_start_simple,
	.rso_stop        = iscs_stop,
	.rso_fini        = iscs_fini
};

struct m0_reqh_service_type m0_iscs_type = {
	.rst_name = "M0_CST_ISCS",
	.rst_ops  = &iscs_type_ops,
	.rst_level = M0_RS_LEVEL_NORMAL,
	.rst_typecode = M0_CST_ISCS,
};

M0_INTERNAL struct m0_htable *m0_isc_htable_get(void)
{
	return m0_get()->i_moddata[M0_MODULE_ISC];
}

M0_INTERNAL int m0_isc_mod_init(void)
{
	struct m0_htable *isc_htable;

	M0_ALLOC_PTR(isc_htable);
	if (isc_htable == NULL)
		return M0_ERR(-ENOMEM);
	m0_get()->i_moddata[M0_MODULE_ISC] = isc_htable;
	return 0;
}

M0_INTERNAL void m0_isc_mod_fini(void)
{
	m0_free(m0_isc_htable_get());
}

M0_INTERNAL int m0_iscs_register(void)
{
	int rc;

	M0_ENTRY();
	rc = m0_reqh_service_type_register(&m0_iscs_type);
	M0_ASSERT(rc == 0);
	rc = m0_iscservice_fop_init();
	if (rc != 0)
		return M0_ERR_INFO(rc, "Fop initialization failed");
	m0_isc_fom_type_init();
	M0_LEAVE();
	return M0_RC(0);
}

M0_INTERNAL void m0_iscs_unregister(void)
{
	m0_reqh_service_type_unregister(&m0_iscs_type);
	m0_iscservice_fop_fini();
}

static int iscs_allocate(struct m0_reqh_service **service,
			 const struct m0_reqh_service_type *stype)
{
	struct m0_reqh_isc_service *isc_svc;

	M0_PRE(service != NULL && stype != NULL);

	M0_ALLOC_PTR(isc_svc);
	if (isc_svc == NULL)
		return M0_ERR(-ENOMEM);

	isc_svc->riscs_magic = M0_ISCS_REQH_SVC_MAGIC;

	*service = &isc_svc->riscs_gen;
	(*service)->rs_ops = &iscs_ops;

	return 0;
}

static int iscs_start(struct m0_reqh_service *service)
{
	int rc = 0;

	M0_ENTRY();
	rc = m0_isc_htable_init(m0_isc_htable_get(), ISC_HT_BUCKET_NR);
	if (rc != 0)
		return M0_ERR(rc);
	M0_LEAVE();
	return rc;
}

static void iscs_stop(struct m0_reqh_service *service)
{
	struct m0_isc_comp *isc_comp;

	M0_ENTRY();
	m0_htable_for(m0_isc, isc_comp, m0_isc_htable_get()) {
		M0_ASSERT(isc_comp->ic_ref_count == 0);
		m0_isc_comp_unregister(&isc_comp->ic_fid);
	} m0_htable_endfor;
	m0_isc_htable_fini(m0_isc_htable_get());
	M0_LEAVE();
}

static void iscs_fini(struct m0_reqh_service *service)
{
	struct m0_reqh_isc_service *isc_svc;

	M0_PRE(service != NULL);

	isc_svc = M0_AMB(isc_svc, service, riscs_gen);
	m0_free(isc_svc);
}

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
