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


/**
 * @addtogroup addb2
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ADDB

#include "lib/trace.h"
#include "lib/assert.h"
#include "lib/chan.h"
#include "lib/mutex.h"
#include "lib/memory.h"
#include "lib/errno.h"                  /* ENOMEM */
#include "lib/misc.h"                   /* M0_AMB */
#include "reqh/reqh_service.h"
#include "reqh/reqh.h"
#include "fop/fom.h"
#include "fop/fop.h"

#include "addb2/addb2.h"
#include "addb2/addb2_xc.h"
#include "addb2/internal.h"
#include "addb2/consumer.h"
#include "addb2/sys.h"
#include "addb2/service.h"

struct addb2_service {
	struct m0_reqh_service   ase_service;
	struct m0_addb2_source   ase_src;
};

struct addb2_fom {
	struct m0_fom          a2_fom;
	struct m0_addb2_cursor a2_cur;
};

static int    addb2_service_start(struct m0_reqh_service *service);
static void   addb2_service_stop(struct m0_reqh_service *service);
static void   addb2_service_fini(struct m0_reqh_service *service);
static void   addb2_done(struct m0_addb2_trace_obj *obj);
static size_t addb2_fom_home_locality(const struct m0_fom *fom);
static int    addb2_service_type_allocate(struct m0_reqh_service **service,
				      const struct m0_reqh_service_type *stype);

static const struct m0_reqh_service_ops       addb2_service_ops;
static const struct m0_reqh_service_type_ops  addb2_service_type_ops;
static const struct m0_fom_ops                addb2_fom_ops;

M0_INTERNAL int m0_addb2_service_module_init(void)
{
	return m0_reqh_service_type_register(&m0_addb2_service_type);
}

M0_INTERNAL void m0_addb2_service_module_fini(void)
{
	m0_reqh_service_type_unregister(&m0_addb2_service_type);
}

static int addb2_service_start(struct m0_reqh_service *svc)
{
	M0_PRE(m0_reqh_service_state_get(svc) == M0_RST_STARTING);
	return 0;
}

static void addb2_service_stop(struct m0_reqh_service *svc)
{
	struct addb2_service *service = M0_AMB(service, svc, ase_service);

	M0_PRE(m0_reqh_service_state_get(svc) == M0_RST_STOPPED);
}

static void addb2_service_fini(struct m0_reqh_service *svc)
{
	struct addb2_service *service = M0_AMB(service, svc, ase_service);

	M0_PRE(m0_reqh_service_state_get(svc) == M0_RST_STOPPED);
	m0_addb2_source_fini(&service->ase_src);
	m0_free(service);
}

static int addb2_service_type_allocate(struct m0_reqh_service **svc,
				       const struct m0_reqh_service_type *stype)
{
	struct addb2_service *service;

	M0_ALLOC_PTR(service);
	if (service != NULL) {
		*svc = &service->ase_service;
		(*svc)->rs_type = stype;
		(*svc)->rs_ops  = &addb2_service_ops;
		m0_addb2_source_init(&service->ase_src);
		return M0_RC(0);
	} else
		return M0_ERR(-ENOMEM);
}

static int addb2_fom_create(struct m0_fop *fop,
			    struct m0_fom **out, struct m0_reqh *reqh)
{
	struct addb2_fom *fom;

	M0_ALLOC_PTR(fom);
	if (fom != NULL) {
		*out = &fom->a2_fom;
		m0_fom_init(*out, &fop->f_type->ft_fom_type,
			    &addb2_fom_ops, fop, NULL, reqh);
		return M0_RC(0);
	} else
		return M0_ERR(-ENOMEM);
}

enum {
	ADDB2_CONSUME = M0_FOM_PHASE_INIT,
	ADDB2_DONE    = M0_FOM_PHASE_FINISH,
	ADDB2_SUBMIT
};

static int addb2_fom_tick(struct m0_fom *fom0)
{
	struct addb2_fom          *fom     = M0_AMB(fom, fom0, a2_fom);
	struct m0_addb2_trace     *trace   = m0_fop_data(fom0->fo_fop);
	struct m0_reqh_service    *svc     = fom0->fo_service;
	struct addb2_service      *service = M0_AMB(service, svc, ase_service);
	struct m0_addb2_source    *src     = &service->ase_src;
	struct m0_addb2_cursor    *cur     = &fom->a2_cur;
	struct m0_addb2_sys       *sys     = m0_fom_dom()->fd_addb2_sys;
	struct m0_addb2_trace_obj *obj;

	switch (m0_fom_phase(fom0)) {
	case ADDB2_CONSUME:
		m0_addb2_cursor_init(cur, trace);
		while (m0_addb2_cursor_next(cur) > 0)
			m0_addb2_consume(src, &cur->cu_rec);
		m0_addb2_cursor_fini(cur);
		m0_fom_phase_set(fom0, ADDB2_SUBMIT);
		return M0_FSO_AGAIN;
	case ADDB2_SUBMIT:
		M0_ALLOC_PTR(obj);
		if (obj != NULL) {
			obj->o_tr   = *trace;
			obj->o_done = &addb2_done;
			/*
			 * Reset the data, so fom finalisation doesn't free it.
			 *
			 * The trace is freed in addb2_done().
			 */
			fom0->fo_fop->f_data.fd_data = NULL;
			if (m0_addb2_sys_submit(sys, obj) == 0)
				addb2_done(obj);
		} else
			M0_LOG(M0_NOTICE, "Lost trace.");
		m0_fom_phase_set(fom0, ADDB2_DONE);
		return M0_FSO_WAIT;
	default:
		M0_IMPOSSIBLE("Invalid phase");
	}
}

static void addb2_fom_fini(struct m0_fom *fom0)
{
	struct addb2_fom *fom = M0_AMB(fom, fom0, a2_fom);

	m0_fom_fini(fom0);
	m0_free(fom);
}

static void addb2_done(struct m0_addb2_trace_obj *obj)
{
	m0_free(obj->o_tr.tr_body);
	m0_free(obj);
}

static size_t addb2_fom_home_locality(const struct m0_fom *fom)
{
	static size_t seq = 0;
	return seq++;
}

static const struct m0_fom_ops addb2_fom_ops = {
	.fo_tick          = &addb2_fom_tick,
	.fo_home_locality = &addb2_fom_home_locality,
	.fo_fini          = &addb2_fom_fini
};

M0_INTERNAL const struct m0_fom_type_ops m0_addb2__fom_type_ops = {
	.fto_create = &addb2_fom_create
};

static struct m0_sm_state_descr addb2_fom_phases[] = {
	[ADDB2_CONSUME] = {
		.sd_name      = "consume",
		.sd_allowed   = M0_BITS(ADDB2_SUBMIT),
		.sd_flags     = M0_SDF_INITIAL
	},
	[ADDB2_SUBMIT] = {
		.sd_name      = "submit",
		.sd_allowed   = M0_BITS(ADDB2_DONE),
	},
	[ADDB2_DONE] = {
		.sd_name      = "done",
		.sd_flags     = M0_SDF_TERMINAL
	}
};

M0_INTERNAL const struct m0_sm_conf m0_addb2__sm_conf = {
	.scf_name      = "addb2 fom",
	.scf_nr_states = ARRAY_SIZE(addb2_fom_phases),
	.scf_state     = addb2_fom_phases
};

static const struct m0_reqh_service_type_ops addb2_service_type_ops = {
	.rsto_service_allocate = &addb2_service_type_allocate
};

static const struct m0_reqh_service_ops addb2_service_ops = {
	.rso_start_async = &m0_reqh_service_async_start_simple,
	.rso_start       = &addb2_service_start,
	.rso_stop        = &addb2_service_stop,
	.rso_fini        = &addb2_service_fini
};

M0_INTERNAL struct m0_reqh_service_type m0_addb2_service_type = {
	.rst_name     = "M0_CST_ADDB2",
	.rst_ops      = &addb2_service_type_ops,
	.rst_level    = M0_RS_LEVEL_NORMAL,
	.rst_typecode = M0_CST_ADDB2
};

#undef M0_TRACE_SUBSYSTEM

/** @} end of addb2 group */

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
