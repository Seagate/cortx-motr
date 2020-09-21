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


#pragma once

#ifndef __MOTR_ST_ST_H__
#define __MOTR_ST_ST_H__

#include "motr/client.h"
#include "motr/client_internal.h"
#include "module/instance.h"

#ifdef __KERNEL__
#include <asm-generic/errno-base.h>
#else
#include <errno.h>
#endif

/**
 * @defgroup Client ST framework
 *
 * A simple system test framework for Client which supports:
 * (1) Test suites can be run as a whole or partially
 * (2) multiple test threads:
 *       - currently, only support same client instance for all threads
 *       - The thread main loop is controlled by either the number of test
 *         rounds or time (can be forever)
 *       - fixed number of threads only (configurable)
 * (3) speed control:
 *       - fixed test rate (useful for load test)
 *       - automatic load test mode: increase test rate till it reaches peak.
 * (4) Tests from multi suites can be mixed.
 *
 * Why not use existing motr ut?(1) to simulate a "real" Client app environment
 * in which only m0tr is available (2) to support those features above.
 *
 * @{
 * */

enum {
	ST_MAX_WORKER_NUM = 32
};

enum {
	DEFAULT_PARGRP_UNIT_SIZE     = 4096,
	DEFAULT_PARGRP_DATA_UNIT_NUM = 3,
	DEFAULT_PARGRP_DATA_SIZE     = DEFAULT_PARGRP_UNIT_SIZE *
				       DEFAULT_PARGRP_DATA_UNIT_NUM
};

/*
 * structure to define test in test suite.
 */
struct st_test {
	/** name of the test, must be unique */
	const char *st_name;
	/** pointer to testing procedure */
	void      (*st_proc)(void);

	/* a test is selected in a CLIENT SELECTED MODE */
	int	    st_enabled;
};

struct st_suite {
	/** name of a suite */
	const char           *ss_name;

	/** functions for suite */
	int                 (*ss_init)(void);
	int                 (*ss_fini)(void);

	/** tests in suite */
	struct st_test ss_tests[];
};

/**
 * Configuration parameters for Client ST
 */
enum st_mode {
	ST_SEQ_MODE = 0,
	ST_RAND_MODE,
	ST_MIXED_MODE
};

struct st_cfg {
	/*number of test threads*/
	int                 sc_nr_threads;

	/*if this flag is set, only specified tests are run*/
	int                 sc_run_selected;
	const char         *sc_tests;

	/**
	 * number of test rounds and expected completion time. Test
	 * ends when it reaches either of them.
	 */
	int                 sc_nr_rounds;
	uint64_t            sc_deadline;

	/* test mode, see above*/
	enum st_mode sc_mode;

	/**
	 * control how fast we issue each single test
	 *   -  0: forward as fast as it can
	 *   - -n: automatically adjust the pace to find the saturation point
	 *   - +n: speed limit (tests/s)
	 */
	int                 sc_pace;
};

/*
 * Client related details
 */
struct m0_instance {
	char		 *si_laddr;
	char		 *si_ha_addr;
	char		 *si_confd_addr;
	char		 *si_prof;

	struct m0_client *si_instance;
};

/**
 * Test statistics data (thread wise)
 */
struct st_worker_stat {
	int64_t sws_nr_asserts;
	int64_t sws_nr_failed_asserts;
};

struct m0_st_ctx {
	struct st_cfg           sx_cfg;

	/* Client instance */
	struct m0_instance      sx_instance;

	/* Test suites */
	int                     sx_nr_all;
	int                     sx_nr_selected;
	struct st_suite       **sx_all;
	struct st_suite       **sx_selected;

	/* Worker thread ID */
	pid_t                  *sx_worker_tids;

	/* Statistics data */
	struct st_worker_stat  *sx_worker_stats;

	/* Maximum length of test name (used for output)*/
	int                     sx_max_name_len;
};

enum {
	ST_MAX_SUITE_NUM = 4096
};

/**
 * Return Motr instance.
 */
struct m0* st_get_motr(void);

/**
 * Setter and getter for Client instance
 */
void st_set_instance(struct m0_client *instance);
struct m0_client* st_get_instance(void);

/**
 * Start runing all Client test suites.
 *
 * @param test_list_str: By default, all tests are included unless a string
 * is provided to specify those selected testss.
 * @return 0 for success
 *
 */
int st_run(const char *test_list_str);

/**
 * Register all test suite
 */
void st_add_suites(void);

/**
 * List avaiable test suites and tests inside each suite.
 */
void st_list(bool);

/**
 * Initialise and finalise client ST
 */
int st_init(void);
void st_fini(void);

/**
 * Client ST assert to allow to collect test failure statistics
 */
bool st_assertimpl(bool c, const char *str_c, const char *file,
		   int lno, const char *func);

/**
 * Retrieve Client ST configuration information.
 */
struct st_cfg st_get_cfg(void);
struct st_worker_stat* st_get_worker_stat(int idx);

/**
 * Getter and setter for number of workers
 */
int  st_get_nr_workers(void);
void st_set_nr_workers(int nr);

/**
 * Getter and setter of selected tests
 */
void         st_set_tests(const char *);
const char*  st_get_tests(void);

/**
 * Getter and setter of test mode
 */
void         st_set_test_mode(enum st_mode);
enum st_mode st_get_test_mode(void);


/**
 * Set the tid of a worker thread
 */
int st_set_worker_tid(int idx, pid_t tid);
int st_get_worker_idx(pid_t tid);

/**
 * Start and stop worker threads for tests
 */
int st_start_workers(void);
int st_stop_workers(void);

/**
 * Object ID allocation and release
 */
int oid_get(struct m0_uint128 *oid);
void oid_put(struct m0_uint128 oid);
uint64_t oid_get_many(struct m0_uint128 *oids, uint64_t nr_oids);
void oid_put_many(struct m0_uint128 *oids, uint64_t nr_oids);

int oid_allocator_init(void);
int oid_allocator_fini(void);

/* Wrappers for Client APIs*/
void st_container_init(struct m0_container     *con,
		       struct m0_realm         *parent,
		       const struct m0_uint128 *id,
		       struct m0_client        *instance);
void st_obj_init(struct m0_obj *obj,
		 struct m0_realm  *parent,
		 const struct m0_uint128 *id, uint64_t layout_id);
void st_obj_fini(struct m0_obj *obj);
int st_entity_create(struct m0_fid *pool,
		     struct m0_entity *entity,
		     struct m0_op **op);
int st_entity_delete(struct m0_entity *entity,
		     struct m0_op **op);
void st_entity_fini(struct m0_entity *entity);
void st_op_launch(struct m0_op **op, uint32_t nr);
int32_t st_op_wait(struct m0_op *op, uint64_t bits, m0_time_t to);
void st_op_fini(struct m0_op *op);
void st_op_free(struct m0_op *op);
void st_entity_open(struct m0_entity *entity);
void st_idx_open(struct m0_entity *entity);
void st_obj_op(struct m0_obj       *obj,
	       enum m0_obj_opcode   opcode,
	       struct m0_indexvec  *ext,
	       struct m0_bufvec    *data,
	       struct m0_bufvec    *attr,
	       uint64_t             mask,
	       uint32_t             flags,
	       struct m0_op       **op);

void st_idx_init(struct m0_idx *idx,
		 struct m0_realm *parent,
		 const struct m0_uint128 *id);
void st_idx_fini(struct m0_idx *idx);
int st_idx_op(struct m0_idx     *idx,
	      enum m0_idx_opcode opcode,
	      struct m0_bufvec  *keys,
	      struct m0_bufvec  *vals,
	      int               *rcs,
	      int                flag,
	      struct m0_op     **op);

int st_layout_op(struct m0_obj *obj,
		 enum m0_entity_opcode opcode,
		 struct m0_client_layout *layout,
		 struct m0_op **op);

/* Allocate aligned memory - user/kernel specific */
void st_alloc_aligned(void **ptr, size_t size, size_t alignment);
void st_free_aligned(void *ptr, size_t size, size_t alignment);

/**
 *	@} client ST end groud
 */

#endif /* __MOTR_ST_ST_H__ */
