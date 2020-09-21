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


#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/semaphore.h"
#include "rpc/rpc_opcodes.h"
#include "fop/fom.h"
#include "fop/fom_generic.h"
#include "fop/fom_interpose.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "ut/ut.h"

struct ms_fom {
	/* Generic m0_fom object. */
        struct m0_fom    msf_fom;
	/* UT test data. */
	int             *msf_test_counter;
	/* To implement fake sleeping. */
	struct m0_sm_ast msf_wakeup;
};

enum leader_fom_phase {
	START_FOLLOWER = M0_FOM_PHASE_INIT,
	FINISH         = M0_FOM_PHASE_FINISH,
	CHECK_RESULT   = M0_FOM_PHASE_NR
};

enum follower_fom_phase {
	FIRST_PHASE  = M0_FOM_PHASE_INIT,
	FINISH_PHASE = M0_FOM_PHASE_FINISH,
	SECOND_PHASE = M0_FOM_PHASE_NR,
	THIRD_PHASE,
};

static struct m0_sm_state_descr leader_fom_phases[] = {
	[START_FOLLOWER] = {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "start-follower",
		.sd_allowed = M0_BITS(CHECK_RESULT)
	},
	[CHECK_RESULT] = {
		.sd_name    = "check-result",
		.sd_allowed = M0_BITS(FINISH)
	},
	[FINISH] = {
		.sd_name    = "finish",
		.sd_flags   = M0_SDF_TERMINAL,
	}
};

static struct m0_sm_state_descr follower_fom_phases[] = {
	[FIRST_PHASE] = {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "first-phase",
		.sd_allowed = M0_BITS(SECOND_PHASE)
	},
	[SECOND_PHASE] = {
		.sd_name    = "second-phase",
		.sd_allowed = M0_BITS(THIRD_PHASE)
	},
	[THIRD_PHASE] = {
		.sd_name    = "third-phase",
		.sd_allowed = M0_BITS(FINISH_PHASE)
	},
	[FINISH_PHASE] = {
		.sd_name    = "finish-phase",
		.sd_flags   = M0_SDF_TERMINAL,
	}
};

static struct m0_sm_conf leader_sm_conf = {
	.scf_name      = "leader_fom",
	.scf_nr_states = ARRAY_SIZE(leader_fom_phases),
	.scf_state     = leader_fom_phases,
};

static struct m0_sm_conf follower_sm_conf = {
	.scf_name      = "follower_fom",
	.scf_nr_states = ARRAY_SIZE(follower_fom_phases),
	.scf_state     = follower_fom_phases,
};

static struct m0_fom_type leader_fomt;
static struct m0_fom_type follower_fomt;

static struct m0_semaphore     sem;
static struct m0_reqh          msreqh;
static struct m0_reqh_service *mssvc;
uint64_t                       msfom_id = 0;
struct m0_fom_thralldom        thrall;
static int                     test_counter;
static bool                    use_same_locality = false;


static size_t ms_fom_home_locality(const struct m0_fom *fom);

static int leader_fom_tick(struct m0_fom *fom);
static int follower_fom_tick (struct m0_fom *fom);

static void leader_fom_create(struct m0_fom **out, struct m0_reqh *reqh);
static void follower_fom_create (struct m0_fom **out, struct m0_reqh *reqh);
static void ms_fom_fini      (struct m0_fom *fom);


/*************************************************/
/*                  UT service                   */
/*************************************************/

static int  mssvc_start(struct m0_reqh_service *svc);
static void mssvc_stop (struct m0_reqh_service *svc);
static void mssvc_fini (struct m0_reqh_service *svc);

static const struct m0_reqh_service_ops mssvc_ops = {
	.rso_start_async = &m0_reqh_service_async_start_simple,
	.rso_start       = &mssvc_start,
	.rso_stop        = &mssvc_stop,
	.rso_fini        = &mssvc_fini
};

static int mssvc_start(struct m0_reqh_service *svc)
{
	return 0;
}

static void mssvc_stop(struct m0_reqh_service *svc)
{
}

static void mssvc_fini(struct m0_reqh_service *svc)
{
	m0_free(svc);
}

static int mssvc_type_allocate(struct m0_reqh_service            **svc,
			       const struct m0_reqh_service_type  *stype)
{
	M0_ALLOC_PTR(*svc);
	M0_ASSERT(*svc != NULL);
	(*svc)->rs_type = stype;
	(*svc)->rs_ops = &mssvc_ops;
	return 0;
}

static const struct m0_reqh_service_type_ops mssvc_type_ops = {
	.rsto_service_allocate = &mssvc_type_allocate
};

static struct m0_reqh_service_type ut_ms_service_type = {
	.rst_name     = "ms_ut",
	.rst_ops      = &mssvc_type_ops,
	.rst_level    = M0_RS_LEVEL_NORMAL,
	.rst_typecode = M0_CST_DS1
};

/*************************************************/
/*                FOMs description               */
/*************************************************/

/** Ops object for leader fom */
static const struct m0_fom_ops leader_fom_ops = {
	.fo_fini          = ms_fom_fini,
	.fo_tick          = leader_fom_tick,
	.fo_home_locality = ms_fom_home_locality
};
/** Ops object for follower fom */
static const struct m0_fom_ops follower_fom_ops = {
	.fo_fini          = ms_fom_fini,
	.fo_tick          = follower_fom_tick,
	.fo_home_locality = ms_fom_home_locality
};

const struct m0_fom_type_ops ms_fom_type_ops = {
	.fto_create = NULL
};

static size_t ms_fom_home_locality(const struct m0_fom *fom)
{
	static size_t locality = 0;

	M0_PRE(fom != NULL);
	return use_same_locality ? 0 : locality++;
}

static void wakeup(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
        struct ms_fom *fom = M0_AMB(fom, ast, msf_wakeup);
        m0_fom_wakeup(&fom->msf_fom);
}

static void fake_wait(struct ms_fom *fom)
{
        struct m0_sm_group *grp;

        grp = &fom->msf_fom.fo_loc->fl_group;
        fom->msf_wakeup.sa_cb = wakeup;
        m0_sm_ast_post(grp, &fom->msf_wakeup);
}

static int leader_fom_tick(struct m0_fom *fom0)
{
	struct ms_fom *fom    = M0_AMB(fom, fom0, msf_fom);
	int            phase  = m0_fom_phase(fom0);
	int            result = M0_FSO_AGAIN;
	struct m0_fom *follower_fom;

	switch (phase) {
	case START_FOLLOWER:
		*fom->msf_test_counter = 0;
		follower_fom_create(&follower_fom, &msreqh);
		m0_fom_enthrall(fom0, follower_fom, &thrall, NULL);
		m0_fom_queue(follower_fom);
		m0_fom_phase_set(fom0, CHECK_RESULT);
		result = M0_FSO_WAIT;
		break;
	case CHECK_RESULT:
		M0_ASSERT(*fom->msf_test_counter == 3);
		m0_fom_phase_set(fom0, FINISH);
		result = M0_FSO_WAIT;
		m0_semaphore_up(&sem);
		break;
	}
	return result;
}

static int follower_fom_tick(struct m0_fom *fom0)
{
	struct ms_fom *fom    = M0_AMB(fom, fom0, msf_fom);
	int            phase  = m0_fom_phase(fom0);
	int            result = M0_FSO_AGAIN;

	switch (phase) {
	case FIRST_PHASE:
		(*fom->msf_test_counter)++;
		m0_fom_phase_set(fom0, SECOND_PHASE);
		/* Imitation of waiting of completion of some opereation. */
		fake_wait(fom);
		result = M0_FSO_WAIT;
		break;
	case SECOND_PHASE:
		(*fom->msf_test_counter)++;
		m0_fom_phase_set(fom0, THIRD_PHASE);
		break;
	case THIRD_PHASE:
		(*fom->msf_test_counter)++;
		m0_fom_phase_set(fom0, FINISH_PHASE);
		result = M0_FSO_WAIT;
		break;
	}
	return result;
}

static void ms_fom_create(struct m0_fom **out, struct m0_reqh *reqh,
                          bool follower)
{
	struct m0_fom *fom0;
	struct ms_fom *fom_obj;

	M0_PRE(out != NULL);

	M0_ALLOC_PTR(fom_obj);
	M0_UT_ASSERT(fom_obj != NULL);

	fom_obj->msf_test_counter = &test_counter;
	fom0 = &fom_obj->msf_fom;
	m0_fom_init(fom0,
		    follower ? &follower_fomt    : &leader_fomt,
		    follower ? &follower_fom_ops : &leader_fom_ops,
		    NULL, NULL, reqh);
	*out = fom0;
}

static void leader_fom_create(struct m0_fom **out, struct m0_reqh *reqh)
{
	m0_fom_type_init(&leader_fomt, M0_UT_LEADER_FOM_OPCODE,
			 &ms_fom_type_ops, &ut_ms_service_type,
			 &leader_sm_conf);
	ms_fom_create(out, reqh, false);
}

static void follower_fom_create(struct m0_fom **out, struct m0_reqh *reqh)
{
	m0_fom_type_init(&follower_fomt, M0_UT_FOLLOWER_FOM_OPCODE,
			 &ms_fom_type_ops, &ut_ms_service_type,
			 &follower_sm_conf);
	ms_fom_create(out, reqh, true);
}

static void ms_fom_fini(struct m0_fom *fom)
{
	struct ms_fom *fom_obj;

	fom_obj = container_of(fom, struct ms_fom, msf_fom);
	m0_fom_fini(fom);
	m0_free(fom_obj);
}

/*************************************************/
/*                 REQH routines                 */
/*************************************************/

static void reqh_init(void)
{
	int rc;

	rc = M0_REQH_INIT(&msreqh,
			  .rhia_dtm     = (void *)1,
			  .rhia_mdstore = (void *)1,
			  .rhia_fid     = &g_process_fid);
	M0_UT_ASSERT(rc == 0);
}

static void reqh_start(void)
{
	int rc;

	rc = m0_reqh_service_allocate(&mssvc, &ut_ms_service_type, NULL);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_service_init(mssvc, &msreqh, NULL);
	m0_reqh_service_start(mssvc);
	m0_reqh_start(&msreqh);
}

static void reqh_stop(void)
{
	m0_reqh_service_prepare_to_stop(mssvc);
	m0_reqh_idle_wait_for(&msreqh, mssvc);
	m0_reqh_service_stop(mssvc);
	m0_reqh_service_fini(mssvc);
}

static void reqh_fini(void)
{
	m0_reqh_services_terminate(&msreqh);
	m0_reqh_fini(&msreqh);
}

/*************************************************/
/*           Leader-Follower UT init/fini            */
/*************************************************/

static void ms_init(void)
{
	reqh_init();
	reqh_start();
}

static void ms_fini(void)
{
	reqh_stop();
	reqh_fini();
}

/*************************************************/
/*                    Test cases                 */
/*************************************************/

static void leader_follower(void)
{
	struct m0_fom *leader_fom;

	ms_init();
	m0_semaphore_init(&sem, 0);
	leader_fom_create(&leader_fom, &msreqh);
	m0_fom_queue(leader_fom);
	m0_semaphore_down(&sem);
	m0_semaphore_fini(&sem);
	ms_fini();
}

static void leader_follower_same_loc(void)
{
	use_same_locality = true;
	leader_follower();
}

static void leader_follower_diff_loc(void)
{
	use_same_locality = false;
	leader_follower();
}

struct m0_ut_suite ms_fom_ut = {
	.ts_name  = "ms-fom-ut",
	.ts_init  = NULL,
	.ts_fini  = NULL,
	.ts_tests = {
		{ "leader-follower-sl", &leader_follower_same_loc, "Sergey" },
		{ "leader-follower-dl", &leader_follower_diff_loc, "Sergey" },
		{ NULL, NULL }
	}
};

M0_EXPORTED(ms_fom_ut);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
