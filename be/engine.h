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

#ifndef __MOTR_BE_ENGINE_H__
#define __MOTR_BE_ENGINE_H__

#include "lib/types.h"          /* bool */
#include "lib/mutex.h"          /* m0_mutex */
#include "lib/tlist.h"          /* m0_tl */
#include "lib/semaphore.h"      /* m0_semaphore */

#include "be/log.h"             /* m0_be_log */
#include "be/tx.h"              /* m0_be_tx */
#include "be/tx_credit.h"       /* m0_be_tx_credit */
#include "be/tx_group.h"        /* m0_be_tx_group_cfg */

struct m0_reqh_service;
struct m0_be_tx_group;
struct m0_be_domain;
struct m0_be_engine;
struct m0_reqh;
struct m0_stob;
struct m0_be_log_discard;
struct m0_be_pd;

/**
 * @defgroup be Meta-data back-end
 *
 * @{
 */

struct m0_be_engine_cfg {
	/**
	 * Number of transactions in ACTIVE state couldn't exceed this value,
	 * even if more transactions could be added to a group.
	 */
	uint64_t                   bec_tx_active_max;
	/** Number of groups. */
	size_t			   bec_group_nr;
	/**
	 * Group configuration.
	 *
	 * The following fields should be set by the user:
	 * - m0_be_tx_group_cfg::tgc_tx_nr_max;
	 * - m0_be_tx_group_cfg::tgc_seg_nr_max;
	 * - m0_be_tx_group_cfg::tgc_size_max;
	 * - m0_be_tx_group_cfg::tgc_payload_max.
	 */
	struct m0_be_tx_group_cfg  bec_group_cfg;
	/** Maximum transaction size. */
	struct m0_be_tx_credit	   bec_tx_size_max;
	/** Maximum transaction payload size. */
	m0_bcount_t		   bec_tx_payload_max;
	/*
	 * The following 3 parameters define group freeze timeout.
	 * @see be_engine_group_timer_arm().
	 */
	m0_time_t		   bec_group_freeze_timeout_min;
	m0_time_t		   bec_group_freeze_timeout_max;
	m0_time_t                  bec_group_freeze_timeout_limit;
	/** Request handler for group foms and engine timeouts */
	struct m0_reqh		  *bec_reqh;
	/** Wait in m0_be_engine_start() until recovery is finished. */
	bool			   bec_wait_for_recovery;
	/** BE domain the engine belongs to. */
	struct m0_be_domain	  *bec_domain;
	struct m0_be_log_discard  *bec_log_discard;
	struct m0_be_pd           *bec_pd;
	/** Configuration for each group. It is set by the engine. */
	struct m0_be_tx_group_cfg *bec_groups_cfg;
	/** The engine lock. Protects all fields of m0_be_engine. */
	struct m0_mutex           *bec_lock;
};

struct m0_be_engine {
	struct m0_be_engine_cfg   *eng_cfg;
	/**
	 * Per-state lists of transaction. Each non-failed transaction is in one
	 * of these lists.
	 */
	struct m0_tl               eng_txs[M0_BTS_NR + 1];
	struct m0_tl               eng_groups[M0_BGS_NR];
	/** Transactional log. */
	struct m0_be_log           eng_log;
	/** Transactional group. */
	struct m0_be_tx_group     *eng_group;
	size_t                     eng_group_nr;
	struct m0_reqh_service    *eng_service;
	uint64_t                   eng_tx_id_next;
	/**
	 * Indicates BE-engine has a transaction opened with
	 * m0_be_tx_exclusive_open() and run under exclusive conditions: no
	 * other transactions are running while @eng_exclusive_mode is set.
	 */
	bool                       eng_exclusive_mode;
	struct m0_be_domain       *eng_domain;
	struct m0_semaphore        eng_recovery_wait_sem;
	bool                       eng_recovery_finished;
};

M0_INTERNAL bool m0_be_engine__invariant(struct m0_be_engine *en);

M0_INTERNAL int m0_be_engine_init(struct m0_be_engine     *en,
				  struct m0_be_domain     *dom,
				  struct m0_be_engine_cfg *en_cfg);
M0_INTERNAL void m0_be_engine_fini(struct m0_be_engine *en);

M0_INTERNAL int m0_be_engine_start(struct m0_be_engine *en);
M0_INTERNAL void m0_be_engine_stop(struct m0_be_engine *en);

/* next functions should be called from m0_be_tx implementation */
M0_INTERNAL void m0_be_engine__tx_init(struct m0_be_engine *en,
				       struct m0_be_tx     *tx,
				       enum m0_be_tx_state  state);

M0_INTERNAL void m0_be_engine__tx_fini(struct m0_be_engine *en,
				       struct m0_be_tx     *tx);

M0_INTERNAL void m0_be_engine__tx_state_set(struct m0_be_engine *en,
					    struct m0_be_tx     *tx,
					    enum m0_be_tx_state  state);
/**
 * Forces the tx group fom to move to LOGGING state and eventually
 * commits all txs to disk.
 */
M0_INTERNAL void m0_be_engine__tx_force(struct m0_be_engine *en,
					struct m0_be_tx     *tx);

M0_INTERNAL void m0_be_engine__tx_group_ready(struct m0_be_engine   *en,
					      struct m0_be_tx_group *gr);
M0_INTERNAL void m0_be_engine__tx_group_discard(struct m0_be_engine   *en,
						struct m0_be_tx_group *gr);

M0_INTERNAL void m0_be_engine_got_log_space_cb(struct m0_be_log *log);
M0_INTERNAL void m0_be_engine_full_log_cb(struct m0_be_log *log);

M0_INTERNAL struct m0_be_tx *m0_be_engine__tx_find(struct m0_be_engine *en,
						   uint64_t             id);
M0_INTERNAL int
m0_be_engine__exclusive_open_invariant(struct m0_be_engine *en,
				       struct m0_be_tx     *excl);

M0_INTERNAL void m0_be_engine_tx_size_max(struct m0_be_engine    *en,
                                          struct m0_be_tx_credit *cred,
                                          m0_bcount_t            *payload_size);

M0_INTERNAL void m0_be_engine__group_limits(struct m0_be_engine *en,
                                            uint32_t            *group_nr,
                                            uint32_t            *tx_per_group);

/** @} end of be group */
#endif /* __MOTR_BE_ENGINE_H__ */

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
