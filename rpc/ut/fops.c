/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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


#include "lib/memory.h"
#include "fop/fop.h"
#include "fop/fom.h"
#include "fop/fom_generic.h"
#include "rpc/rpc_opcodes.h"

#include "rpc/ut/fops_xc.h"

extern struct m0_reqh_service_type m0_rpc_service_type;

static int arrow_fom_create(struct m0_fop *fop, struct m0_fom **m,
			    struct m0_reqh *reqh);
static void arrow_fom_fini(struct m0_fom *fom);
static int arrow_fom_tick(struct m0_fom *fom);
static size_t arrow_fom_home_locality(const struct m0_fom *fom);

struct m0_fop_type m0_rpc_arrow_fopt;

struct m0_semaphore arrow_hit;
struct m0_semaphore arrow_destroyed;

static const struct m0_fom_type_ops arrow_fom_type_ops = {
	.fto_create = arrow_fom_create,
};

static const struct m0_fom_ops arrow_fom_ops = {
	.fo_fini          = arrow_fom_fini,
	.fo_tick          = arrow_fom_tick,
	.fo_home_locality = arrow_fom_home_locality,
};

M0_INTERNAL void m0_rpc_test_fops_init(void)
{
	m0_xc_rpc_ut_fops_init();
	M0_FOP_TYPE_INIT(&m0_rpc_arrow_fopt,
		.name      = "RPC_arrow",
		.opcode    = M0_RPC_ARROW_OPCODE,
		.xt        = arrow_xc,
		.rpc_flags = M0_RPC_ITEM_TYPE_ONEWAY,
		.fom_ops   = &arrow_fom_type_ops,
		.sm        = &m0_generic_conf,
		.svc_type  = &m0_rpc_service_type);
	m0_semaphore_init(&arrow_hit, 0);
	m0_semaphore_init(&arrow_destroyed, 0);
}

M0_INTERNAL void m0_rpc_test_fops_fini(void)
{
	m0_semaphore_fini(&arrow_destroyed);
	m0_semaphore_fini(&arrow_hit);
	m0_fop_type_fini(&m0_rpc_arrow_fopt);
	m0_xc_rpc_ut_fops_fini();
}


static int arrow_fom_create(struct m0_fop *fop, struct m0_fom **m,
			    struct m0_reqh *reqh)
{
	struct m0_fom *fom;

	M0_ALLOC_PTR(fom);
	M0_ASSERT(fom != NULL);

	m0_fom_init(fom, &m0_rpc_arrow_fopt.ft_fom_type, &arrow_fom_ops,
		    fop, NULL, reqh);
	*m = fom;
	return 0;
}

static void arrow_fom_fini(struct m0_fom *fom)
{
	m0_fom_fini(fom);
	m0_free(fom);
	m0_semaphore_up(&arrow_destroyed);
}

static int arrow_fom_tick(struct m0_fom *fom)
{
	m0_semaphore_up(&arrow_hit);
	m0_fom_phase_set(fom, M0_FOPH_FINISH);
	return M0_FSO_WAIT;
}

static size_t arrow_fom_home_locality(const struct m0_fom *fom)
{
	return 0;
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
