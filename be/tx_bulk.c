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
 * @addtogroup be
 *
 * Worker SM
 * @verbatim
 *  m0_be_tx_bulk_run()
 *   |  - m0_be_op_active(tb->btb_op)
 *   v
 *   \>------------------+
 *                       |     +<---------------------------------------------<\
 *                       v     v                                               ^
 *    /<------- be_tx_bulk_queue_get_cb()                                      |
 *    v                     |    - asynchronously get an item                  |
 *    |                     |      with M0_BE_QUEUE_GET()                      |
 *    |                     v                                                  |
 *    |    /<--  be_tx_bulk_queue_get_done_cb()                                |
 *    |    v                |     - m0_sm_ast_post() to get out of the locks   |
 *    |    |                v                                                  |
 *    |    |       be_tx_bulk_init_cb()                                        |
 *    |    |                |     - get more items with M0_BE_QUEUE_PEEK() and |
 *    |    |                |       M0_BE_QUEUE_GET() without blocking         |
 *    |    |                |     - m0_be_tx_init()                            |
 *    |    |                |     - m0_be_tx_prep()                            |
 *    |    |                |     - m0_be_tx_open()                            |
 *    |    |                v                                                  |
 *    |    |     be_tx_bulk_close_cb()                                         |
 *    |    |                |     - tbc_do() for the items                     |
 *    |    |                |     - m0_be_tx_close()                           |
 *    |    |                |                                                  |
 *    |    |                v                                                  |
 *    |    |       be_tx_bulk_gc_cb()                                          |
 *    |    |                v     - tbc_done() for the items                   ^
 *    |    |                \>------------------------------------------------>/
 *    |    |
 *    v    v
 *  be_tx_bulk_finish_cb()
 *          - m0_be_op_done(tb->btb_op)
 *
 * @endverbatim
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/tx_bulk.h"

#include "lib/memory.h"         /* M0_ALLOC_ARR */
#include "lib/locality.h"       /* m0_locality_get */
#include "lib/chan.h"           /* m0_clink */
#include "lib/errno.h"          /* ENOENT */

#include "be/tx.h"              /* m0_be_tx */
#include "be/domain.h"          /* m0_be_domain__group_limits */ /* XXX */

#include "sm/sm.h"              /* m0_sm_ast */


enum {
	/**
	 * Maximum number of be_tx_bulk_worker-s in m0_be_tx_bulk.
	 *
	 * This value can be tuned to increase performance.
	 */
	BE_TX_BULK_WORKER_MAX = 0x40,
};

struct be_tx_bulk_item {
	void                   *bbd_user;
	struct m0_be_tx_credit  bbd_credit;
	m0_bcount_t             bbd_payload_size;
};

#define BTBI_F "(qdata=%p bbd_user=%p bbd_credit="BETXCR_F" " \
	"bbd_payload_size=%"PRIu64")"

#define BTBI_P(btbi) (btbi), (btbi)->bbd_user, BETXCR_P(&(btbi)->bbd_credit), \
	(btbi)->bbd_payload_size

struct be_tx_bulk_worker {
	uint64_t                tbw_index;
	uint64_t                tbw_partition;
	uint64_t                tbw_locality;
	struct m0_be_tx         tbw_tx;
	struct m0_be_tx_bulk   *tbw_tb;
	struct be_tx_bulk_item *tbw_item;
	uint64_t                tbw_items_nr;
	bool                    tbw_queue_get_successful;
	struct m0_sm_ast        tbw_queue_get;
	struct m0_sm_ast        tbw_init;
	struct m0_sm_ast        tbw_close;
	struct m0_sm_ast        tbw_finish;
	void                   *tbw_user;
	struct m0_clink         tbw_clink;
	struct m0_sm_group     *tbw_grp;
	int                     tbw_rc;
	struct m0_be_op         tbw_op;
	bool                    tbw_failed;
	bool                    tbw_done;
	bool                    tbw_terminate_order;
};

static void be_tx_bulk_finish_cb(struct m0_sm_group *grp,
				 struct m0_sm_ast   *ast);
static void be_tx_bulk_queue_get_cb(struct m0_sm_group *grp,
				    struct m0_sm_ast   *ast);
static void be_tx_bulk_init_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast);
static bool be_tx_bulk_open_cb(struct m0_clink *clink);
static void be_tx_bulk_close_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast);
static void be_tx_bulk_gc_cb(struct m0_be_tx *tx, void *param);
static void be_tx_bulk_queue_get_done_cb(struct m0_be_op *op, void *param);

M0_INTERNAL int m0_be_tx_bulk_init(struct m0_be_tx_bulk     *tb,
                                   struct m0_be_tx_bulk_cfg *tb_cfg)
{
	struct be_tx_bulk_worker *worker;
	uint64_t                  worker_partition;
	uint64_t                  workers_per_partition;
	uint64_t                  partitions_per_locality;
	uint64_t                  localities_nr;
	uint64_t                  i;
	uint64_t                  j;
	uint64_t                  k;
	int                       rc;
	M0_ENTRY("tb = %p", tb);

	M0_PRE(M0_IS0(tb));
	M0_PRE(tb_cfg->tbc_partitions_nr > 0);
	/*
	 * Can't have more partitions than workers because each worker handles
	 * only one partition.
	 */
	M0_PRE(tb_cfg->tbc_partitions_nr <= tb_cfg->tbc_workers_nr);
	M0_PRE(tb_cfg->tbc_work_items_per_tx_max > 0);
	M0_PRE(tb_cfg->tbc_q_cfg.bqc_item_length == 0);
	/*
	 * Consumers for the queues are workers, so this value will be set by
	 * tx_bulk for each queue separately.
	 */
	M0_PRE(tb_cfg->tbc_q_cfg.bqc_consumers_nr_max == 0);

	tb->btb_cfg = *tb_cfg;
	tb->btb_tx_open_failed = false;
	tb->btb_done = false;
	tb->btb_termination_in_progress = false;
	m0_mutex_init(&tb->btb_lock);
	M0_ALLOC_ARR(tb->btb_worker, tb->btb_cfg.tbc_workers_nr);
	rc = tb->btb_worker == NULL ? -ENOMEM : 0;
	if (rc != 0) {
		m0_mutex_fini(&tb->btb_lock);
		return M0_ERR(rc);
	}
	M0_ALLOC_ARR(tb->btb_q, tb->btb_cfg.tbc_partitions_nr);
	M0_ASSERT(tb->btb_q != NULL);   /* XXX handle allocation error */
	tb->btb_cfg.tbc_q_cfg.bqc_item_length = sizeof(struct be_tx_bulk_item);
	tb->btb_cfg.tbc_q_cfg.bqc_consumers_nr_max = tb->btb_cfg.tbc_workers_nr;
	for (i = 0; i < tb_cfg->tbc_partitions_nr; ++i) {
		rc = m0_be_queue_init(&tb->btb_q[i], &tb->btb_cfg.tbc_q_cfg);
		if (rc != 0)
			break;
	}
	if (rc != 0) {
		m0_free(tb->btb_worker);
		m0_mutex_fini(&tb->btb_lock);
		return M0_ERR(rc);
	}
	m0_be_op_init(&tb->btb_kill_put_op);
	/* XXX take it from elsewhere */
	localities_nr = m0_fom_dom()->fd_localities_nr;
	for (i = 0; i < tb->btb_cfg.tbc_workers_nr; ++i) {
		worker = &tb->btb_worker[i];
		*worker = (struct be_tx_bulk_worker){
			.tbw_index                = i,
			.tbw_tb                   = tb,
			.tbw_items_nr             = 0,
			.tbw_queue_get_successful = false,
			.tbw_grp                  =
				m0_locality_get(worker->tbw_locality)->lo_grp,
			.tbw_rc                   = 0,
			.tbw_queue_get            = {
				.sa_cb    = &be_tx_bulk_queue_get_cb,
				.sa_datum = worker,
			},
			.tbw_init                 = {
				.sa_cb    = &be_tx_bulk_init_cb,
				.sa_datum = worker,
			},
			.tbw_close                = {
				.sa_cb    = &be_tx_bulk_close_cb,
				.sa_datum = worker,
			},
			.tbw_finish               = {
				.sa_cb    = &be_tx_bulk_finish_cb,
				.sa_datum = worker,
			},
			.tbw_failed               = false,
			.tbw_done                 = false,
		};
		m0_be_op_init(&worker->tbw_op);
		m0_be_op_callback_set(&worker->tbw_op,
		                      &be_tx_bulk_queue_get_done_cb,
		                      worker, M0_BOS_DONE);
		M0_ALLOC_ARR(worker->tbw_item,
			     tb->btb_cfg.tbc_work_items_per_tx_max);
		M0_ASSERT(worker->tbw_item != NULL);  /* XXX handle error */
	}
	if (tb->btb_cfg.tbc_partitions_nr <= localities_nr) {
		/*
		 * - each locality has a partition assigned;
		 * - each worker takes it's work from one of such partitions.
		 */
		for (i = 0; i < tb->btb_cfg.tbc_workers_nr; ++i) {
			worker = &tb->btb_worker[i];
			worker->tbw_locality = i % localities_nr;
			worker->tbw_partition = worker->tbw_locality %
						tb->btb_cfg.tbc_partitions_nr;
		}
	} else {
		/*
		 * - each partition has a single locality assigned;
		 * - each partition has several workers assigned;
		 * - workers are running in the locality, assigned to the
		 *   partition the worker takes it's work from.
		 */
		workers_per_partition = tb->btb_cfg.tbc_workers_nr /
					tb->btb_cfg.tbc_partitions_nr;
		partitions_per_locality = (tb->btb_cfg.tbc_partitions_nr +
					   localities_nr - 1) / localities_nr;
		worker = &tb->btb_worker[0];
		worker_partition = 0;
		for (i = 0; i < localities_nr; ++i) {
			if (i != 0 &&
			    i == tb->btb_cfg.tbc_partitions_nr % localities_nr)
				--partitions_per_locality;
			for (j = 0; j < partitions_per_locality; ++j) {
				if (worker_partition ==
				    tb->btb_cfg.tbc_partitions_nr -
				    tb->btb_cfg.tbc_workers_nr %
				    tb->btb_cfg.tbc_partitions_nr)
					++workers_per_partition;
				for (k = 0; k < workers_per_partition; ++k) {
					worker->tbw_locality = i;
					worker->tbw_partition =
						worker_partition;
					++worker;
				}
				++worker_partition;
			}
		}
		M0_ASSERT(worker_partition == tb->btb_cfg.tbc_partitions_nr);
		M0_ASSERT(worker ==
			  &tb->btb_worker[tb->btb_cfg.tbc_workers_nr]);
		M0_ASSERT(m0_forall(i, tb->btb_cfg.tbc_partitions_nr,
		    M0_IN(m0_reduce(j, tb->btb_cfg.tbc_workers_nr, 0,
		                    + (tb->btb_worker[j].tbw_partition == i)),
		          (workers_per_partition, workers_per_partition - 1))));
	}
	for (i = 0; i < tb->btb_cfg.tbc_workers_nr; ++i) {
		worker = &tb->btb_worker[i];
		worker->tbw_grp = m0_locality_get(worker->tbw_locality)->lo_grp;
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_be_tx_bulk_fini(struct m0_be_tx_bulk *tb)
{
	struct be_tx_bulk_worker *worker;
	uint32_t                  i;

	for (i = 0; i < tb->btb_cfg.tbc_workers_nr; ++i) {
		worker = &tb->btb_worker[i];
		m0_free(worker->tbw_item);
		m0_be_op_fini(&worker->tbw_op);
	}
	m0_be_op_fini(&tb->btb_kill_put_op);
	for (i = 0; i < tb->btb_cfg.tbc_partitions_nr; ++i)
		m0_be_queue_fini(&tb->btb_q[i]);
	m0_free(tb->btb_q);
	m0_mutex_fini(&tb->btb_lock);
	m0_free(tb->btb_worker);
}

static void be_tx_bulk_lock(struct m0_be_tx_bulk *tb)
{
	m0_mutex_lock(&tb->btb_lock);
}

static void be_tx_bulk_unlock(struct m0_be_tx_bulk *tb)
{
	m0_mutex_unlock(&tb->btb_lock);
}


static void be_tx_bulk_queues_drain(struct m0_be_tx_bulk *tb)
{
	struct be_tx_bulk_item data;
	uint64_t               i;
	bool                   successful;

	for (i = 0; i < tb->btb_cfg.tbc_partitions_nr; ++i) {
		m0_be_queue_lock(&tb->btb_q[i]);
		while (M0_BE_QUEUE_PEEK(&tb->btb_q[i], &data)) {
			M0_BE_OP_SYNC(op,
			              M0_BE_QUEUE_GET(&tb->btb_q[i], &op, &data,
			                              &successful));
			M0_ASSERT(successful);
		}
		m0_be_queue_unlock(&tb->btb_q[i]);
	}
}

static void be_tx_bulk_finish_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct be_tx_bulk_worker *worker = ast->sa_datum;
	struct m0_be_tx_bulk     *tb = worker->tbw_tb;
	uint32_t                  done_nr = 0;
	uint32_t                  i;
	bool                      done;

	M0_ENTRY("tb=%p worker=%p tbw_index=%"PRIu64" tbw_partition=%"PRIu64" "
	         "tbw_locality=%"PRIu64, tb, worker, worker->tbw_index,
		 worker->tbw_partition, worker->tbw_locality);
	M0_PRE(ast == &worker->tbw_finish);

	be_tx_bulk_lock(tb);
	M0_ASSERT(!worker->tbw_done);
	worker->tbw_done = true;
	for (i = 0; i < tb->btb_cfg.tbc_workers_nr; ++i)
		done_nr += tb->btb_worker[i].tbw_done;
	M0_LOG(M0_DEBUG, "done_nr=%"PRIu32, done_nr);
	done = done_nr == tb->btb_cfg.tbc_workers_nr;
	if (done) {
		tb->btb_rc = 0;
		for (i = 0; i < tb->btb_cfg.tbc_workers_nr; ++i)
			tb->btb_rc = tb->btb_worker[i].tbw_rc ?: tb->btb_rc;
		tb->btb_done = true;
		M0_LOG(M0_DEBUG, "setting tb=%p btb_done = true", tb);
	}
	be_tx_bulk_unlock(tb);
	if (done) {
		be_tx_bulk_queues_drain(tb);
		m0_be_op_done(tb->btb_op);
	}
	M0_LEAVE();
}

static void be_tx_bulk_queue_get_cb(struct m0_sm_group *grp,
				    struct m0_sm_ast   *ast)
{
	struct be_tx_bulk_worker *worker = ast->sa_datum;
	struct m0_be_tx_bulk     *tb = worker->tbw_tb;
	struct m0_be_queue       *bq = &tb->btb_q[worker->tbw_partition];

	M0_ENTRY("worker=%p tbw_index=%"PRIu64, worker, worker->tbw_index);
	M0_PRE(ast == &worker->tbw_queue_get);
	M0_PRE(worker->tbw_items_nr == 0);
	M0_PRE(ergo(worker->tbw_rc != 0, tb->btb_tx_open_failed));

	if (worker->tbw_rc != 0) {
		/* @see be_tx_bulk_open_cb() */
		m0_be_tx_fini(&worker->tbw_tx);
	}
	if (tb->btb_tx_open_failed) {
		m0_sm_ast_post(worker->tbw_grp, &worker->tbw_finish);
	} else {
		m0_be_op_reset(&worker->tbw_op);
		m0_be_queue_lock(bq);
		M0_BE_QUEUE_GET(bq, &worker->tbw_op, &worker->tbw_item[0],
		                &worker->tbw_queue_get_successful);
		m0_be_queue_unlock(bq);
	}
	M0_LEAVE("worker=%p tbw_index=%"PRIu64" tbw_rc=%d",
		 worker, worker->tbw_index, worker->tbw_rc);
}

static void be_tx_bulk_queue_get_done_cb(struct m0_be_op *op, void *param)
{
	struct be_tx_bulk_worker *worker = param;

	M0_ENTRY("worker=%p tbw_index=%"PRIu64" tbw_queue_get_successful=%d",
		 worker, worker->tbw_index, !!worker->tbw_queue_get_successful);

	if (worker->tbw_queue_get_successful) {
		worker->tbw_items_nr = 1;
		m0_sm_ast_post(worker->tbw_grp, &worker->tbw_init);
	} else {
		m0_sm_ast_post(worker->tbw_grp, &worker->tbw_finish);
	}
}

static void be_tx_bulk_open(struct be_tx_bulk_worker *worker,
                            struct m0_be_tx_credit   *cred,
                            m0_bcount_t               cred_payload)
{
	struct m0_be_tx_bulk     *tb     = worker->tbw_tb;
	struct m0_be_tx_bulk_cfg *tb_cfg = &tb->btb_cfg;
	struct m0_be_tx          *tx     = &worker->tbw_tx;

	M0_SET0(tx);
	m0_be_tx_init(tx, 0, tb_cfg->tbc_dom, worker->tbw_grp,
		      NULL, NULL, NULL, NULL);
	m0_be_tx_gc_enable(tx, &be_tx_bulk_gc_cb, worker);

	M0_SET0(&worker->tbw_clink);
	m0_clink_init(&worker->tbw_clink, &be_tx_bulk_open_cb);
	m0_clink_add(&tx->t_sm.sm_chan, &worker->tbw_clink);

	m0_be_tx_prep(tx, cred);
	m0_be_tx_payload_prep(tx, cred_payload);
	m0_be_tx_open(tx);
}

static void be_tx_bulk_init_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct be_tx_bulk_worker *worker = ast->sa_datum;
	struct m0_be_tx_credit    accum_credit;
	struct be_tx_bulk_item   *data;
	struct m0_be_tx_bulk     *tb = worker->tbw_tb;
	struct m0_be_queue       *bq = &tb->btb_q[worker->tbw_partition];
	m0_bcount_t               accum_payload_size = 0;
	bool                      successful;

	M0_PRE(ast == &worker->tbw_init);

	M0_ENTRY("worker=%p tbw_index=%"PRIu64, worker, worker->tbw_index);
	M0_PRE(worker->tbw_items_nr == 1);

	accum_credit       = worker->tbw_item[0].bbd_credit;
	accum_payload_size = worker->tbw_item[0].bbd_payload_size;
	/* optimisation: don't take the lock when per tx limit is only 1 item */
	if (tb->btb_cfg.tbc_work_items_per_tx_max > 1) {
		m0_be_queue_lock(bq);
		while (worker->tbw_items_nr <
		       tb->btb_cfg.tbc_work_items_per_tx_max) {
			data = &worker->tbw_item[worker->tbw_items_nr];
			if (!M0_BE_QUEUE_PEEK(bq, data))
				break;
			/* XXX check payload also */
			if (m0_be_should_break(&tb->btb_cfg.tbc_dom->bd_engine,
					       &accum_credit,
					       &data->bbd_credit))
				break;
			M0_BE_OP_SYNC(op, M0_BE_QUEUE_GET(bq, &op, data,
							  &successful));
			M0_ASSERT(successful);
			m0_be_tx_credit_add(&accum_credit, &data->bbd_credit);
			accum_payload_size += data->bbd_payload_size;
			++worker->tbw_items_nr;
		}
		m0_be_queue_unlock(bq);
	}
	be_tx_bulk_open(worker, &accum_credit, accum_payload_size);
	M0_LEAVE("worker=%p tbw_index=%"PRIu64, worker, worker->tbw_index);
}

static bool be_tx_bulk_open_cb(struct m0_clink *clink)
{
	struct be_tx_bulk_worker *worker;
	struct m0_be_tx_bulk     *tb;
	struct m0_be_tx          *tx;

	worker = container_of(clink, struct be_tx_bulk_worker, tbw_clink);
	M0_ENTRY("worker=%p tbw_index=%"PRIu64, worker, worker->tbw_index);
	tx = &worker->tbw_tx;
	tb =  worker->tbw_tb;
	if (M0_IN(m0_be_tx_state(tx), (M0_BTS_ACTIVE, M0_BTS_FAILED))) {
		m0_clink_del(&worker->tbw_clink);
		m0_clink_fini(&worker->tbw_clink);

		if (m0_be_tx_state(tx) == M0_BTS_ACTIVE) {
			m0_sm_ast_post(worker->tbw_grp, &worker->tbw_close);
		} else {
			be_tx_bulk_lock(tb);
			tb->btb_tx_open_failed = true;
			be_tx_bulk_unlock(tb);
			worker->tbw_rc = tx->t_sm.sm_rc;
			M0_LOG(M0_ERROR, "tx=%p rc=%d", tx, worker->tbw_rc);
			be_tx_bulk_queues_drain(tb);
			/* the operation is not going to be executed */
			worker->tbw_items_nr = 0;
			/*
			 * Can't call m0_be_tx_fini(tx) here because
			 * m0_be_tx_put() for M0_BTS_FAILED transaction
			 * is called after worker transition.
			 *
			 * be_tx_bulk_queue_get_cb() will do this.
			 */
			be_tx_bulk_gc_cb(tx, worker);
		}
	}
	M0_LEAVE("worker=%p tbw_index=%"PRIu64, worker, worker->tbw_index);
	return false;
}

static void be_tx_bulk_close_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_be_tx_bulk_cfg  *tb_cfg;
	struct be_tx_bulk_worker  *worker = ast->sa_datum;
	struct m0_be_tx_bulk      *tb;
	uint64_t                   i;

	M0_ENTRY("worker=%p tbw_index=%"PRIu64, worker, worker->tbw_index);
	M0_PRE(ast == &worker->tbw_close);
	tb = worker->tbw_tb;
	tb_cfg = &tb->btb_cfg;
	for (i = 0; i < worker->tbw_items_nr; ++i) {
		M0_LOG(M0_DEBUG, "worker=%p tbw_index=%"PRIu64" bbd_user=%p",
		       worker, worker->tbw_index, worker->tbw_item[i].bbd_user);
		M0_BE_OP_SYNC(op, tb_cfg->tbc_do(tb, &worker->tbw_tx, &op,
		                                 tb_cfg->tbc_datum,
		                                 worker->tbw_item[i].bbd_user,
		                                 worker->tbw_index,
		                                 worker->tbw_partition));
	}
	m0_be_tx_close(&worker->tbw_tx);
	M0_LEAVE("worker=%p tbw_index=%"PRIu64, worker, worker->tbw_index);
}

static void be_tx_bulk_gc_cb(struct m0_be_tx *tx, void *param)
{
	struct be_tx_bulk_worker *worker = param;
	struct m0_be_tx_bulk     *tb;
	uint64_t                  i;

	M0_ENTRY("worker=%p tbw_index=%"PRIu64, worker, worker->tbw_index);
	M0_PRE(tx == &worker->tbw_tx);

	tb = worker->tbw_tb;
	for (i = 0; i < worker->tbw_items_nr; ++i) {
		M0_LOG(M0_DEBUG, "worker=%p tbw_index=%"PRIu64" bbd_user=%p",
		       worker, worker->tbw_index, worker->tbw_item[i].bbd_user);
		tb->btb_cfg.tbc_done(tb, tb->btb_cfg.tbc_datum,
		                     worker->tbw_item[i].bbd_user,
		                     worker->tbw_index, worker->tbw_partition);
	}
	worker->tbw_items_nr = 0;
	m0_sm_ast_post(worker->tbw_grp, &worker->tbw_queue_get);

	M0_LEAVE("worker=%p tbw_index=%"PRIu64, worker, worker->tbw_index);
}

M0_INTERNAL void m0_be_tx_bulk_run(struct m0_be_tx_bulk *tb,
                                   struct m0_be_op      *op)
{
	struct be_tx_bulk_worker *worker;
	uint32_t                  i;

	M0_ENTRY();
	tb->btb_op = op;
	m0_be_op_active(tb->btb_op);
	for (i = 0; i < tb->btb_cfg.tbc_workers_nr; ++i) {
		worker = &tb->btb_worker[i];
		M0_LOG(M0_DEBUG, "worker=%p tbw_index=%"PRIu64,
		       worker, worker->tbw_index);
		m0_sm_ast_post(worker->tbw_grp, &worker->tbw_queue_get);
	}
	M0_LEAVE();
}

M0_INTERNAL bool m0_be_tx_bulk_put(struct m0_be_tx_bulk   *tb,
                                   struct m0_be_op        *op,
                                   struct m0_be_tx_credit *credit,
                                   m0_bcount_t             payload_credit,
                                   uint64_t                partition,
                                   void                   *user)
{
	struct be_tx_bulk_item  data = {
		.bbd_user         = user,
		.bbd_credit       = *credit,
		.bbd_payload_size = payload_credit,
	};
	bool                    put_fail;
	M0_ENTRY("tb=%p", tb);

	M0_PRE(partition < tb->btb_cfg.tbc_partitions_nr);

	be_tx_bulk_lock(tb);
	put_fail = tb->btb_tx_open_failed;

	if (put_fail) {
		be_tx_bulk_unlock(tb);
		m0_be_op_active(op);
		m0_be_op_done(op);
		return M0_RC(false);
	}

	m0_be_queue_lock(&tb->btb_q[partition]);
	M0_BE_QUEUE_PUT(&tb->btb_q[partition], op, &data);
	m0_be_queue_unlock(&tb->btb_q[partition]);

	be_tx_bulk_unlock(tb);

	return M0_RC(true);
}

M0_INTERNAL void m0_be_tx_bulk_end(struct m0_be_tx_bulk *tb)
{
	uint64_t i;

	for (i = 0; i < tb->btb_cfg.tbc_partitions_nr; ++i) {
		m0_be_queue_lock(&tb->btb_q[i]);
		m0_be_queue_end(&tb->btb_q[i]);
		m0_be_queue_unlock(&tb->btb_q[i]);
	}
}

M0_INTERNAL int m0_be_tx_bulk_status(struct m0_be_tx_bulk *tb)
{
	int rc;

	be_tx_bulk_lock(tb);
	M0_PRE(tb->btb_done);
	rc = tb->btb_rc;
	be_tx_bulk_unlock(tb);
	return rc;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of be group */

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
