/* -*- C -*- */
/*
 * Copyright (c) 2017-2021 Seagate Technology LLC and/or its Affiliates
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


#include <errno.h>
#include "lib/bitmap.h"
#include "lib/trace.h"
#include "lib/time.h"
#include "motr/m0crate/logger.h"
#include "motr/m0crate/crate_client.h"
#include "motr/m0crate/crate_client_utils.h"

/** @defgroup crate Crate
 * Utility for executing simple performance tests.
 */

/** @defgroup dix_workload Workload module for client DIX
 * \ingroup crate
 *
 * Crate index workload overview.
 * -------------------------------
 *
 * Crate execution for index measurements consists of two parts:
 *	1. Warmup.
 *	2. Common operations.
 *
 * Warmup is supposed to prepare index to workload.
 * For example, if we have to measure over fragmented index, we should
 * setup next parameters:
 * ```yaml
 *	NUM_KVP: 8
 *	WARMUP_PUT_CNT: 1024
 *	WARMUP_DEL_RATIO: 2
 * ```
 * It is means, that crate performs next operations:
 * ```
 *	1024 PUT x 8 kv/op (sequentially)
 *	1024/2 DEL x 8 kv/op (randomly)
 * ```
 *
 * Common operations are executed randomly (see ::cr_op_selector).
 * Each operaion has 'percents' of total operations count (OP_COUNT).
 * Total number of keys in use are calculated by OP_COUNT and NUM_KVP.
 * OP_COUNT can be "unlimited" *TODO: for now, it
 * means `INT_MAX / (128 * NUM_KVP)` operations*.
 *
 *
 *
 * Crate index workload detailed description.
 * ------------------------------------------
 *
 * ## Workload has following parameters:
 *
 * * WORKLOAD_TYPE - always 0 (idx operations).
 * * WORKLOAD_SEED - initial value for pseudo-random generator
 *	(int or "tstamp").
 * * KEY_SIZE   - size of the key; can be defined as number or "random", KEY_SIZE should be >= 16
 * * VALUE_SIZE - size of the value; can be defined as number or "random"
 * * MAX_KEY_SIZE - max size of key; used in case, when KEY_SIZE = "random".
 *	Also, see ::CR_MAX_OF_MAX_KEY_SIZE.
 * * MAX_VALUE_SIZE - max size of value; used in case, when VALUE_SIZE = "random".
 *	Also, see ::CR_MAX_OF_MAX_VALUE_SIZE.
 * * OP_COUNT - total operations count; can be defined as number, number with
 *	units (1K), or "unlimited".
 * * PUT,GET,DEL,NEXT - percents of portion operations.
 * * EXEC_TIME - time limit for executing; can be defined as number (seconds),
 *	or "unlimited".
 * * NUM_KVP - number of records, which should be processed at operation.
 * * NXRECORDS - number of records, which should be processed at NEXT operation;
 *	can be defined as number, number with units (1K) or "default"
 *	(NXRECORDS == NUM_KVP).
 * * KEY_PREFIX - number, which suppose to define the key space
 *	(fid.f_container); can be defined as a number or "random".
 * * WARMUP_PUT_CNT - count of PUT operations in "warmup" step; defined as
 *	number of operations or as "all" (it means "fill all keys in
 *	the index").
 * * WARMUP_DEL_RATIO - ratio, which determines, which portion of WARMUP_PUT_CNT
 *	should deleted in random order (int).
 * * KEY_ORDER - defines key ordering in operations ("ordered" or "random").
 * * INDEX_FID - index fid (fid, for example, `<7800000000000001:0>`).
 * * LOG_LEVEL - logging level(err(0), warn(1), info(2), trace(3), debug(4)).
 *
 *
 * ## Operation order (see ::cr_idx_w_select_op)
 * Operations are executed in round-robin manner or randomly.
 * If crate can't find appropriate keys for operation then it marks operation
 * as "pending". Selector knows about it and can unmark pending ops,
 * if it cannot find appropriate operations.
 *
 * ## Keys order
 * Keys can be generated sequentially or randomly. There are two functions:
 * `cr_idx_w_find_rnd_k` and `cr_idx_w_find_seq_k`. They both have the same
 * signature, but different logic.
 *
 * ### ::cr_idx_w_find_seq_k
 * This function selects next keys from starting key to `nr_keys` (the end of
 * keys space). Starting key value depends on operation type: readonly
 * operations (GET,NEXT) should store last key from previous key sequence;
 * other operations (PUT,DEL) starts from first key. Anyway, they iterate over
 * all available keys and fill key list.
 *
 * ### ::cr_idx_w_find_rnd_k
 * This function selects random key, puts it in the key list and does it again
 * until list is full or bitmap is full.
 *
 * ## Different logic for different operations (see struct ::cr_idx_w_ops).
 * This struct describes how operation should be prepared for execution and
 * how they change storage state.
 *
 * ## Measurements
 * Currently, only execution time is measured during the test. It measures with
 * `m0_time*` functions. Crate prints result to stdout when test is finished.
 *
 * ## Logging
 * crate has own logging system, which based on `fprintf(stderr...)`.
 * (see ::crlog and see ::cr_log).
 *
 * ## Execution time limits
 * There are two ways to limit execution time. The first, is
 * operations time limit, which checks after operation executed (as described
 * in HLD). The second way uses the watchdog, which detects staled
 * client operations.
 */

/** Logger prefix for DIX workload */
#define LOG_PREFIX "dix: "

/* XXX: Disable tracking system to increase performance. */
#ifdef NTRACK
#	undef M0_RC
#	define M0_RC(X) (X)
#	undef M0_ERR
#	define M0_ERR(X) (X)
#	undef M0_PRE
#	define M0_PRE(X)
#	undef M0_POST
#	define M0_POST(X)
#	undef M0_ASSERT
#	define M0_ASSERT(X)
#endif

/** Global constants for DIX workload */
enum {
	/** Upper limit for max_key_size parameter. i.e 1MB */
	CR_MAX_OF_MAX_KEY_SIZE	 = (1 << 20),
	/** Lower limit for key_size parameter. i.e 16 Bytes */
	CR_MIN_KEY_SIZE		 = sizeof(struct m0_fid),
	/** Upper limit for max_value_size parameter. i.e 1GB */
	CR_MAX_OF_MAX_VALUE_SIZE = (1 << 30),
	/** Upper limit for next_records parameter. ie 1GB */
	CR_MAX_NEXT_RECORDS	 = (1 << 30),
};

enum cr_op_selector {
	/** Operations should be selected in round-robin maner. */
	CR_OP_SEL_RR,
	/** Operations should be selected randomly */
	CR_OP_SEL_RND,
};

/* RANDOM */
/** Get pseudo-random int32 in positive range [0;end) */
static int cr_rand_pos_range(int end)
{
	return rand() % end;
}

/** Get pseudo-random uint64 in positive range [0;end) */
static size_t cr_rand_pos_range_l(size_t end)
{
	size_t val_h;
	size_t val_l;
	size_t res;

	val_l = rand();
	val_h = rand();

	res = (val_h << 32) | val_l;

	return res % end;
}

/** Get random boolean value */
static bool cr_rand_bool()
{
	return rand() % 2 == 1;
}

/** Get random string */
void cr_get_random_string(char *dest, size_t length)
{
	char set[] = "0123456789"
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	const size_t set_len = strlen(set);
	int index;
	int i;

	for (i = 0; i < length  - 1; i++)
	{
		index = cr_rand_pos_range(set_len);
		dest[i] = set[index];
	}

	dest[length - 1] = '\0';
}


/* TIME MEASUREMENTS */
struct cr_time_measure_ctx {
	/** First timestamp. */
	m0_time_t	ts;
	/** Last timestamp. */
	m0_time_t	ts_next;
	/** Delta between last and first. */
	double		elapsed;
};

/* CLIENT INDEX RESULT MEASUREMENTS */
char *cr_idx_op_labels[CRATE_OP_NR] = {
	"PUT",
	"GET",
	"NEXT",
	"DEL"
};
struct cr_idx_ops_result {
	/**  op label */
	const char	*cior_op_label;
	/** comes from cr_opcode::CRATE_OP_PUT */
	int          	 cior_op_type;
	/** total ops count per op type */
	int	     	 cior_op_count;
	/** total op time in  m0_time_t */
	m0_time_t    	 cior_ops_total_time_m0;
	/** total op time in seconds */
	double      	 cior_ops_total_time_s;
	/** per op time in nanoseconds */
	double       	 cior_time_per_op_ns;
};

struct cr_idx_w_results {
	/** entire workload time in m0 */
	m0_time_t                   ciwr_total_time_m0;
	/** entire workload time in seconds */
	double                      ciwr_total_time_s;
	/** entire workload per op time in ns */
	double              	    ciwr_time_per_op_ns;
	/** per op result for each put, get, next and del */
	struct cr_idx_ops_result    ciwr_ops_result[CRATE_OP_NR];
};

static double cr_time_in_seconds(m0_time_t mtime)
{
	return (m0_time_seconds(mtime)) +
		( m0_time_nanoseconds(mtime) / (double) M0_TIME_ONE_SECOND );
}

static void cr_time_measure_begin(struct cr_time_measure_ctx *t)
{
	*t = (struct cr_time_measure_ctx) {};
	t->ts = m0_time_now();
}

static double cr_time_measure_elapsed_now(struct cr_time_measure_ctx *t)
{
	m0_time_t d = m0_time_sub(m0_time_now(), t->ts);
	return (m0_time_seconds(d)) +
		( m0_time_nanoseconds(d) / (double) M0_TIME_ONE_SECOND );
}

static void cr_time_measure_end(struct cr_time_measure_ctx *t)
{
	t->elapsed = cr_time_measure_elapsed_now(t);
	t->ts_next = m0_time_now();
}

/* BITMAP */
static void m0_bitmap_print(struct m0_bitmap *bm)
{
	int i;
	int nr = bm->b_nr;

	if (nr <= 1024) {
		cr_log_ex(CLL_DEBUG, LOG_PREFIX, "", "len = %d, map = {", nr);
		for (i = 0; i < nr; i++)
			cr_log_ex(CLL_SAME, "", "", "%d", m0_bitmap_get(bm, i));
		cr_log_ex(CLL_SAME, "", "", "}\n");
	} else
		crlog(CLL_DEBUG, "Bitmap is too large to print.");
}

/* WATCHDOG */
struct cr_watchdog *cr_watchdog = NULL;
static int  cr_watchdog_init(struct m0_workload_index *wt);
static void cr_watchdog_fini();
static void cr_watchdog_touch();


/* CLIENT OPCODES UTILS */
static const char *crate_op_to_string(enum cr_opcode op);
static bool cr_opcode_valid(enum cr_opcode opcode);


/* CLIENT INDEX WORKLOAD */
struct cr_ops_counter {
	/** number of remaining ops */
	int	nr;
	/** is pending op ? */
	bool	pending;
	/** prevous key index */
	int	prev_key;
	/** count of previous keys */
	int	prev_key_nr;
};

/** Client index workload descriptor */
struct cr_idx_w {
	struct m0_workload_index       *wit;
	struct m0_bitmap		bm;
	struct cr_ops_counter		nr_ops[CRATE_OP_NR];
	int				nr_ops_total;
	bool				ordered_keys;
	int				warmup_put_cnt;
	int				warmup_del_cnt;
	struct m0_fid			key_prefix;
	int				nr_keys;
	int				nr_kv_per_op;
	bool				prev_key_used;
	struct cr_time_measure_ctx	exec_time_ctx;
	size_t				exec_time;
	enum cr_op_selector		op_selector;
	struct cr_idx_w_results	        ciw_results;
};

static int cr_idx_w_init(struct cr_idx_w *ciw,
			 struct m0_workload_index *wit);
static void cr_idx_w_fini(struct cr_idx_w *ciw);
static void cr_idx_w_seq_keys_init(struct cr_idx_w *w, int keys_nr);
static void cr_idx_w_seq_keys_fini(struct cr_idx_w *w);
static int cr_idx_w_seq_keys_get_last(struct cr_idx_w *w,
				      int nr, enum cr_opcode op);
static bool cr_idx_w_seq_keys_enabled(struct cr_idx_w *w);
static void cr_idx_w_seq_keys_save(struct cr_idx_w *w,
				   int *keys, size_t keys_nr,
				   enum cr_opcode op);
static size_t cr_idx_w_get_value_size(struct cr_idx_w *w);
static int cr_idx_w_get_nr_keys_per_op(struct cr_idx_w *w, enum cr_opcode op);
static int cr_idx_w_find_seq_k(struct cr_idx_w *w,
			       enum cr_opcode opcode,
			       int *keys, size_t nr_keys);
static int cr_idx_w_find_rnd_k(struct cr_idx_w *w,
			       enum cr_opcode opcode,
			       int *keys, size_t nr_keys);
static bool cr_idx_w_timeout_expired(struct cr_idx_w *w);
static int cr_idx_w_execute(struct cr_idx_w *w,
			    enum cr_opcode opcode,
			    bool random,
			    size_t nr_keys,
			    bool *missing_key);
static int cr_idx_w_get_nr_remained_ops(struct cr_idx_w *w);
static enum cr_opcode cr_idx_w_select_op(struct cr_idx_w *w);
static int cr_idx_w_get_nr_remained_op_types(struct cr_idx_w *w);
static void cr_idx_w_print_ops_table(struct cr_idx_w *w);
static bool cr_idx_w_rebalance_ops(struct cr_idx_w *w, enum cr_opcode op);
static int cr_idx_w_common(struct cr_idx_w *w);
static int cr_idx_w_warmup(struct cr_idx_w *w);

/** capture final results */
static void cr_time_capture_results(struct cr_time_measure_ctx *t,
					struct cr_idx_w *ciw)
{
	int i 			  	     = 0;
	struct cr_idx_ops_result *op_results = ciw->ciw_results.ciwr_ops_result;
	ciw->ciw_results.ciwr_total_time_s   = t->elapsed;
	ciw->ciw_results.ciwr_total_time_m0  = m0_time_sub(t->ts_next, t->ts);
	ciw->ciw_results.ciwr_time_per_op_ns =  m0_time_nanoseconds(
			ciw->ciw_results.ciwr_total_time_m0 / ciw->nr_ops_total);

	/* Calculate Results for each op (PUT, GET, NEXT and DEL) */
	for (i = 0; i < CRATE_OP_NR; i++) {
		if (op_results[i].cior_op_count) {
			op_results[i].cior_ops_total_time_s =
				cr_time_in_seconds(op_results[i].cior_ops_total_time_m0);
			op_results[i].cior_time_per_op_ns   = m0_time_nanoseconds(
				op_results[i].cior_ops_total_time_m0 /
				op_results[i].cior_op_count);
		}
	}

}
/** RESULT REPORT */
static void cr_time_measure_report(struct cr_time_measure_ctx *t,
					struct cr_idx_w w)
{
	int i			     	     = 0;
	struct cr_idx_ops_result *op_results = w.ciw_results.ciwr_ops_result;

	/* Results in parsable format */
	fprintf(stdout, "result: total_s, %f, avg_time_per_op_ns, %.1f,"
		" key_size_bytes, %d, value_size_bytes, %d, ops, %d\n",
		t->elapsed, w.ciw_results.ciwr_time_per_op_ns,
		w.wit->key_size, w.wit->value_size, w.nr_ops_total);

	for (i = 0; i < CRATE_OP_NR; i++) {
		fprintf(stdout, "result: %s, total_time_s, %f, avg_time_per_op_ns, "
			"%.1f, ops, %d\n",
			op_results[i].cior_op_label,
			op_results[i].cior_ops_total_time_s,
			op_results[i].cior_time_per_op_ns,
			op_results[i].cior_op_count);
	}

	/* Results in m0crate format */
	fprintf(stdout, "\nTotal: time="TIME_F" ("TIME_F" per op) ops=%d\n",
                       TIME_P(w.ciw_results.ciwr_total_time_m0),
                       TIME_P(w.ciw_results.ciwr_total_time_m0 /
		       w.nr_ops_total),
		       w.nr_ops_total);

	for (i = 0; i < CRATE_OP_NR; i++) {
		if (op_results[i].cior_op_count) {
			fprintf(stdout, "%s: "TIME_F" ("TIME_F" per op) ops=%d\n",
				op_results[i].cior_op_label,
				TIME_P(op_results[i].cior_ops_total_time_m0),
				TIME_P(op_results[i].cior_ops_total_time_m0 /
				op_results[i].cior_op_count),
				op_results[i].cior_op_count);
		}
	}
}


/* IMPLEMENTATION */

static int cr_idx_w_init(struct cr_idx_w *ciw,
			 struct m0_workload_index *wit)
{
	int rc;
	int i;

	M0_PRE(wit->max_key_size < CR_MAX_OF_MAX_KEY_SIZE);
	M0_PRE(wit->max_value_size < CR_MAX_OF_MAX_VALUE_SIZE);
	M0_PRE(wit->next_records < CR_MAX_NEXT_RECORDS);

	*ciw = (typeof(*ciw)) {};

	ciw->wit = wit;
	ciw->nr_ops_total = 0;

	/* Setting up min_key_size default to MIN_KEY_SIZE */
	ciw->wit->min_key_size = CR_MIN_KEY_SIZE;

	/* Init crate index result */
	for (i = 0; i < CRATE_OP_NR; i++) {
		ciw->ciw_results.ciwr_ops_result[i].cior_op_label =
			cr_idx_op_labels[i];
		ciw->ciw_results.ciwr_ops_result[i].cior_op_type = i;
		ciw->ciw_results.ciwr_ops_result[i].cior_ops_total_time_s = 0;
		ciw->ciw_results.ciwr_ops_result[i].cior_time_per_op_ns = 0;
		ciw->ciw_results.ciwr_ops_result[i].cior_ops_total_time_m0 = 0;
	}

	/* XXX: If opcount is unlimited, then make it limited */
	if (wit->op_count < 0) {
		wit->op_count = INT_MAX / (128 * wit->num_kvs);
	}

	for (i = 0; i < ARRAY_SIZE(ciw->nr_ops); i++) {
		ciw->nr_ops[i].nr = wit->op_count *
			((double) wit->opcode_prcnt[i] / 100);
		ciw->nr_ops_total += ciw->nr_ops[i].nr;
		ciw->ciw_results.ciwr_ops_result[i].cior_op_count =
			ciw->nr_ops[i].nr;
	}
	if (wit->warmup_put_cnt == -1) {
		ciw->warmup_put_cnt = ciw->nr_ops_total;
		ciw->nr_keys = ciw->nr_ops_total;
	} else {
		ciw->warmup_put_cnt = wit->warmup_put_cnt;
		ciw->nr_keys = ciw->warmup_put_cnt > ciw->nr_ops_total ?
			ciw->warmup_put_cnt : ciw->nr_ops_total;
	}

	ciw->nr_kv_per_op = wit->num_kvs;
	ciw->nr_keys *= ciw->nr_kv_per_op;
	ciw->ordered_keys = wit->keys_ordered;

	/* If key size is random, generate it using rand(). */
	if (ciw->wit->key_size < 0)
		ciw->wit->key_size = ciw->wit->min_key_size +
			cr_rand_pos_range(ciw->wit->max_key_size - ciw->wit->min_key_size);

	if (wit->warmup_del_ratio > 0)
		ciw->warmup_del_cnt =
			ciw->warmup_put_cnt / wit->warmup_del_ratio;
	else
		ciw->warmup_del_cnt = 0;

	rc = m0_bitmap_init(&ciw->bm, ciw->nr_keys);
	if (rc != 0)
		return M0_ERR(rc);

	srand(wit->seed);

	ciw->key_prefix = wit->key_prefix;

	if (ciw->key_prefix.f_container == -1)
		ciw->key_prefix.f_container = cr_rand_pos_range_l(UINT64_MAX);

	cr_time_measure_begin(&ciw->exec_time_ctx);
	ciw->exec_time = wit->exec_time;

	if (cr_idx_w_get_nr_remained_op_types(ciw) > 1)
		ciw->op_selector = CR_OP_SEL_RND;
	else
		ciw->op_selector = CR_OP_SEL_RR;

	/* if DEL or GET or NEXT and PUT given, then we have to
	* use round-robin selector to avoid cases when
	* we perform GET/DEL/NEXT on empty index.
	*/
	if ((ciw->nr_ops[CRATE_OP_DEL].nr != 0 || ciw->nr_ops[CRATE_OP_GET].nr != 0 ||
		ciw->nr_ops[CRATE_OP_NEXT].nr != 0) && ciw->nr_ops[CRATE_OP_PUT].nr != 0) {
		ciw->op_selector = CR_OP_SEL_RR;
	}

	M0_POST(ciw->nr_kv_per_op < ciw->nr_keys);
	M0_POST(ciw->nr_kv_per_op > 0);
	M0_POST(ciw->nr_keys > 0);

	return M0_RC(0);
}

static void cr_idx_w_fini(struct cr_idx_w *ciw)
{
	m0_bitmap_fini(&ciw->bm);
}

/* Seq keys */
static void cr_idx_w_seq_keys_init(struct cr_idx_w *w, int keys_nr)
{
	int i;

	w->prev_key_used = true;

	for (i = 0; i < ARRAY_SIZE(w->nr_ops); i++)
		w->nr_ops[i].prev_key = -1;
}

static void cr_idx_w_seq_keys_fini(struct cr_idx_w *w)
{
	w->prev_key_used = false;
}

static int cr_idx_w_seq_keys_get_last(struct cr_idx_w *w,
				      int nr, enum cr_opcode op)
{
	M0_PRE(nr > 0);
	M0_PRE(cr_opcode_valid(op));

	return (w->nr_ops[op].prev_key + nr > w->nr_keys) ?
		0 : w->nr_ops[op].prev_key + 1;
}

static bool cr_idx_w_seq_keys_enabled(struct cr_idx_w *w)
{
	return w->prev_key_used;
}

static void cr_idx_w_seq_keys_save(struct cr_idx_w *w,
				   int *keys, size_t keys_nr,
				   enum cr_opcode op)
{
	M0_PRE(ergo(w->prev_key_used, w->ordered_keys));

	if (w->prev_key_used) {
		w->nr_ops[op].prev_key = keys[keys_nr - 1];
		w->nr_ops[op].prev_key_nr = keys_nr;
	}
}


/* Key-Value pairs (records) */
struct kv_pair {
	struct m0_bufvec *k;
	struct m0_bufvec *v;
};

static void idx_bufvec_free(struct m0_bufvec *bv)
{
	uint32_t i;

	if (bv == NULL)
		return;

	if (bv->ov_buf != NULL) {
		for (i = 0; i < bv->ov_vec.v_nr; ++i)
			free(bv->ov_buf[i]);
		free(bv->ov_buf);
	}
	free(bv->ov_vec.v_count);
	free(bv);
}

static void kv_pair_fini(struct kv_pair *p)
{
	idx_bufvec_free(p->k);
	idx_bufvec_free(p->v);
}

static struct m0_bufvec* idx_bufvec_alloc(int nr)
{
	struct m0_bufvec *bv;

	M0_ALLOC_PTR(bv);
	if (bv == NULL)
		return NULL;

	bv->ov_vec.v_nr = nr;

	M0_ALLOC_ARR(bv->ov_vec.v_count, bv->ov_vec.v_nr);
	if (bv->ov_vec.v_count == NULL)
		goto do_free;

	M0_ALLOC_ARR(bv->ov_buf, nr);
	if (bv->ov_buf == NULL)
		goto do_free;

	return bv;

do_free:
	m0_bufvec_free(bv);
	return NULL;
}

static size_t cr_idx_w_get_value_size(struct cr_idx_w *w)
{
	int vlen;
	/* If value_size is random, generate it using rand() */
	if (w->wit->value_size < 0)
		vlen = w->wit->key_size +
			cr_rand_pos_range(w->wit->max_value_size - w->wit->key_size);
	else
		vlen = w->wit->value_size;

	M0_POST(vlen < CR_MAX_OF_MAX_VALUE_SIZE);
	M0_POST(vlen > 0);
	return vlen;
}

static int cr_idx_w_get_nr_keys_per_op(struct cr_idx_w *w, enum cr_opcode op)
{
	size_t result;

	if (op == CRATE_OP_NEXT && w->wit->next_records > 0) {
		result = 1 + cr_rand_pos_range(w->wit->next_records);
		M0_POST(result < CR_MAX_NEXT_RECORDS);
	} else
		result = w->nr_kv_per_op;

	return result;
}

static int fill_kv_del(struct cr_idx_w *w,
		       struct m0_fid *k, struct kv_pair *p, size_t nr,
		       int kpart_one_size, char *kpart_one)
{
	int i;

	p->k = idx_bufvec_alloc(nr);
	if (p->k == NULL)
		return M0_ERR(-ENOMEM);

	p->v = NULL;

	M0_ASSERT(p->k->ov_vec.v_nr == nr);

	for (i = 0; i < nr; i++) {
		p->k->ov_vec.v_count[i] = w->wit->key_size;
		p->k->ov_buf[i] = m0_alloc(w->wit->key_size);
		memcpy(p->k->ov_buf[i], (void*)kpart_one, kpart_one_size);
		memcpy(p->k->ov_buf[i] + kpart_one_size, &k[i], sizeof(*k));

		crlog(CLL_DEBUG, "Generated k=%s:" FID_F, kpart_one, FID_P(&k[i]));
	}
	return M0_RC(0);
}

static int fill_kv_next(struct cr_idx_w *w,
			struct m0_fid *k, struct kv_pair *p, size_t nr,
			int kpart_one_size, char *kpart_one)
{
	p->k = idx_bufvec_alloc(nr);
	if (p->k == NULL)
		return M0_ERR(-ENOMEM);

	p->v = idx_bufvec_alloc(nr);
	if (p->v == NULL) {
		idx_bufvec_free(p->k);
		return M0_ERR(-ENOMEM);
	}

	M0_ASSERT(p->k->ov_vec.v_nr == nr);
	M0_ASSERT(p->v->ov_vec.v_nr == nr);

	p->k->ov_vec.v_count[0] = w->wit->key_size;
	p->k->ov_buf[0] = m0_alloc(w->wit->key_size);
	memcpy(p->k->ov_buf[0], (void*)kpart_one, kpart_one_size);
	memcpy(p->k->ov_buf[0] + kpart_one_size, &k[0], sizeof(*k));
	crlog(CLL_DEBUG, "Generated next k=%s:" FID_F, kpart_one, FID_P(&k[0]));

	return M0_RC(0);
}

static int fill_kv_get(struct cr_idx_w *w,
		       struct m0_fid *k, struct kv_pair *p, size_t nr,
		       int kpart_one_size, char *kpart_one)
{
	int i;

	p->k = idx_bufvec_alloc(nr);
	if (p->k == NULL)
		return M0_ERR(-ENOMEM);

	p->v = idx_bufvec_alloc(nr);
	if (p->v == NULL) {
		idx_bufvec_free(p->k);
		return M0_ERR(-ENOMEM);
	}

	M0_ASSERT(p->k->ov_vec.v_nr == nr);
	M0_ASSERT(p->v->ov_vec.v_nr == nr);

	for (i = 0; i < nr; i++) {
		p->k->ov_vec.v_count[i] = w->wit->key_size;
		p->k->ov_buf[i] = m0_alloc(w->wit->key_size);
		memcpy(p->k->ov_buf[i], (void*)kpart_one, kpart_one_size);
		memcpy(p->k->ov_buf[i] + kpart_one_size, &k[i], sizeof(*k));
		crlog(CLL_DEBUG, "Generated k=%s:" FID_F, kpart_one, FID_P(&k[i]));
	}

	return M0_RC(0);
}
static int fill_kv_put(struct cr_idx_w *w,
		       struct m0_fid *k, struct kv_pair *p, size_t nr,
		       int kpart_one_size, char *kpart_one)
{
	int vlen;
	int i;

	p->k = idx_bufvec_alloc(nr);
	if (p->k == NULL)
		return M0_ERR(-ENOMEM);

	p->v = idx_bufvec_alloc(nr);
	if (p->v == NULL) {
		idx_bufvec_free(p->k);
		return M0_ERR(-ENOMEM);
	}

	M0_ASSERT(p->k->ov_vec.v_nr == nr);
	M0_ASSERT(p->v->ov_vec.v_nr == nr);

	for (i = 0; i < nr; i++) {
		p->k->ov_vec.v_count[i] = w->wit->key_size;
		p->k->ov_buf[i] = m0_alloc_aligned(w->wit->key_size,
						   m0_pageshift_get());
		memcpy(p->k->ov_buf[i], (void*)kpart_one, kpart_one_size);
		memcpy(p->k->ov_buf[i] + kpart_one_size, &k[i], sizeof(*k));
		vlen = cr_idx_w_get_value_size(w);
		p->v->ov_vec.v_count[i] = vlen;
		p->v->ov_buf[i] = m0_alloc_aligned(vlen, m0_pageshift_get());
		cr_get_random_string(p->v->ov_buf[i], vlen);
		crlog(CLL_DEBUG, "Generated k=%s:" FID_F ",v=%s",
		      kpart_one,
		      FID_P(&k[i]),
		      vlen > 128 ? "<...>": (char *) p->v->ov_buf[i]);
	}

	return M0_RC(0);
}

typedef int (*cr_idx_w_find_k_t)(struct cr_idx_w *w, enum cr_opcode opcode,
				 int *keys, size_t nr_keys);

struct cr_idx_w_ops {
	enum cr_opcode     op;
	/** 0 - for DEL, 1 for another ops */
	bool               empty_bit;
	/** how to fill kv pair */
	int (*fill_kv)(struct cr_idx_w *w,
		       struct m0_fid *k,
		       struct kv_pair *p,
		       size_t nr,
		       int kpart_one_size,
		       char *kpart_one);
	/** is op readonly? */
	bool               readonly;
	/** op filling bit (for !readonly) */
	bool               is_set_op;
	/** client opcode corresponding to op */
	enum m0_idx_opcode m0_op;
	/** humand readable name of op */
	const char        *name;
};

struct cr_idx_w_ops cr_idx_w_ops[CRATE_OP_NR] = {
	[CRATE_OP_PUT] = {
		.op = CRATE_OP_PUT,
		.empty_bit = false,
		.fill_kv = fill_kv_put,
		.readonly = false,
		.is_set_op = true,
		.m0_op = M0_IC_PUT,
		.name = "put",
	},
	[CRATE_OP_DEL] = {
		.op = CRATE_OP_DEL,
		.empty_bit = true,
		.fill_kv = fill_kv_del,
		.readonly = false,
		.is_set_op = false,
		.m0_op = M0_IC_DEL,
		.name = "del",
	},
	[CRATE_OP_GET] = {
		.op = CRATE_OP_GET,
		.empty_bit = true,
		.fill_kv = fill_kv_get,
		.readonly = true,
		.is_set_op = false,
		.m0_op = M0_IC_GET,
		.name = "get",
	},
	[CRATE_OP_NEXT] = {
		.op = CRATE_OP_NEXT,
		.empty_bit = true,
		.fill_kv = fill_kv_next,
		.readonly = true,
		.is_set_op = false,
		.m0_op = M0_IC_NEXT,
		.name = "next",
	},
};

static const char *crate_op_to_string(enum cr_opcode op)
{
	return cr_idx_w_ops[op].name;
}

static bool cr_opcode_valid(enum cr_opcode opcode)
{
	return opcode != CRATE_OP_INVALID;
}

/** Check if all values in array is unique */
static M0_UNUSED bool int_array_is_set(int *vals, size_t nr)
{
	int i;
	int j;

	for (i = 0; i < nr; i++) {
		for (j = 0; j < nr; j++) {
			if (i != j && vals[i] == vals[j])
				return false;
		}
	}

	return true;
}

static int cr_idx_w_find_seq_k(struct cr_idx_w *w,
			       enum cr_opcode opcode,
			       int *keys, size_t nr_keys)
{
	struct cr_idx_w_ops *op = &cr_idx_w_ops[opcode];
	int key_iter = 0;
	int start_key = 0;
	int rc = 0;
	int i;

	M0_PRE(w->nr_keys > 0);
	M0_PRE(nr_keys != 0);

	if (cr_idx_w_seq_keys_enabled(w)) {
		start_key = cr_idx_w_seq_keys_get_last(w, nr_keys, opcode);
		crlog(CLL_DEBUG, "starting_key_num=%d", start_key);
	}

	for (i = start_key; i < w->nr_keys; i++) {
		if (m0_bitmap_get(&w->bm, i) == op->empty_bit) {
			keys[key_iter++] = i;
			if (key_iter == nr_keys)
				break;
		}
	}

	if (key_iter != nr_keys) {
		crlog(CLL_ERROR, "Unable to find enough keys for opcode '%s'",
		      crate_op_to_string(opcode));
		m0_bitmap_print(&w->bm);
		rc = M0_ERR(-ENOENT);
	} else
		M0_POST(int_array_is_set(keys, nr_keys));

	return M0_RC(rc);
}

static bool m0_bitmap_is_fulfilled(struct m0_bitmap *bm, bool fill)
{
	return (m0_forall(i, bm->b_nr, fill == m0_bitmap_get(bm, i)));
}

static int cr_idx_w_find_rnd_k(struct cr_idx_w *w,
				enum cr_opcode opcode,
				int *keys, size_t nr_keys)
{
	struct cr_idx_w_ops *op = &cr_idx_w_ops[opcode];
	int                  rc = 0;
	int                  r;
	int                  key_iter = 0;
	size_t               attempts = 0;

	M0_PRE(w->nr_keys > 0);

	while (key_iter != nr_keys) {
		if (attempts > w->bm.b_nr &&
		    m0_bitmap_is_fulfilled(&w->bm, !op->empty_bit)) {
			crlog(CLL_ERROR, "Map hasn't available bits.");
			rc = M0_ERR(-ENOENT);
			break;
		}

		r = cr_rand_pos_range_l(w->nr_keys);
		M0_ASSERT(r < w->nr_keys);
		attempts++;

		/* skip key if it already present in 'keys' or
		 * or if it can not be used for op */
		if (!m0_exists(i, key_iter, keys[i] == r) &&
		    m0_bitmap_get(&w->bm, r) == op->empty_bit) {
			attempts = 0;
			keys[key_iter] = r;
			key_iter++;
		}
	}

	if (rc != 0) {
		crlog(CLL_ERROR, "Unable to find key for opcode '%s'",
		      crate_op_to_string(opcode));
		m0_bitmap_print(&w->bm);
	} else
		M0_POST(int_array_is_set(keys, nr_keys));

	return M0_RC(rc);
}

static int cr_execute_query(struct m0_fid *id,
			     struct kv_pair *p,
			     enum cr_opcode opcode)
{
	struct m0_op         *ops[1] = { [0] = NULL };
	int32_t              *rcs;
	int                   rc;
	struct m0_idx         idx = {};
	int                   kv_nr;
	int                   flags = 0;
	int                   kv_index;
	struct cr_idx_w_ops  *op = &cr_idx_w_ops[opcode];

	kv_nr = p->k->ov_vec.v_nr;

	if (NULL == M0_ALLOC_ARR(rcs, kv_nr))
		return M0_ERR(-ENOMEM);

	m0_idx_init(&idx, crate_uber_realm(), (struct m0_uint128 *) id);

	rc = m0_idx_op(&idx, op->m0_op, p->k, p->v, rcs, flags, &ops[0]);
	if (rc != 0) {
		crlog(CLL_ERROR, "Unable to init Client idx op: %s",
		      strerror(-rc));
		goto end;
	}

	m0_op_launch(ops, 1);

	rc = m0_op_wait(ops[0], M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
			M0_TIME_NEVER);
	if (rc != 0) {
		crlog(CLL_ERROR, "Client op failed: %s", strerror(-rc));
		goto end;
	}

	if (m0_exists(i, kv_nr, (kv_index = i, (rc = rcs[i]) != 0))) {
		/* XXX: client destroys keys, if NEXT op failed */
		if (op->m0_op == M0_IC_NEXT) {
			crlog(CLL_ERROR,
			      "Failed to perform client operation %s: k="
			      FID_F " %s (%d), index=%d",
			      crate_op_to_string(op->op),
			      FID_P((struct m0_fid *) p->k->ov_buf[0]),
			      strerror(-rc), rc, kv_index);
		} else {
			crlog(CLL_ERROR,
			      "Failed to perform client operation %s: k="
			      FID_F " %s (%d)",
			      crate_op_to_string(op->op),
			      FID_P((struct m0_fid *) p->k->ov_buf[kv_index]),
			      strerror(-rc), rc);
		}
	} else
		rc = ops[0]->op_sm.sm_rc;

end:
	if (ops[0] != NULL) {
		m0_op_fini(ops[0]);
		m0_op_free(ops[0]);
	}
	m0_idx_fini(&idx);
	m0_free0(&rcs);

	return M0_RC(rc);
}

static bool cr_idx_w_timeout_expired(struct cr_idx_w *w)
{
	return ergo(w->exec_time > 0,
		    cr_time_measure_elapsed_now(&w->exec_time_ctx) >=
		    w->exec_time);
}

static int cr_idx_w_execute(struct cr_idx_w *w,
			    enum cr_opcode opcode,
			    bool random,
			    size_t nr_keys,
			    bool *missing_key)
{
	struct cr_idx_w_ops	*op = &cr_idx_w_ops[opcode];
	cr_idx_w_find_k_t	 find_fn;
	int			 rc;
	int			*ikeys = NULL;
	struct m0_fid		*keys = NULL;
	struct kv_pair		 kv = {0};
	int			 i;
	/* key will contain <const value:random or sequential generated value> */
	int 			 kpart_one_size = w->wit->key_size - w->wit->min_key_size;
	char 			 kpart_one[kpart_one_size];
	m0_time_t 		 op_start_time;
	m0_time_t 		 op_time;

	M0_PRE(nr_keys > 0);

	cr_watchdog_touch();

	if (cr_idx_w_timeout_expired(w)) {
		crlog(CLL_ERROR, "Timeout expired.");
		return M0_ERR(-ETIME);
	}

	if (NULL == M0_ALLOC_ARR(ikeys, nr_keys)) {
		rc = M0_ERR(-ENOMEM);
		goto do_exit;
	}

	if (NULL == M0_ALLOC_ARR(keys, nr_keys)) {
		rc = M0_ERR(-ENOMEM);
		goto do_exit_ik;
	}

	find_fn = (random ? cr_idx_w_find_rnd_k : cr_idx_w_find_seq_k);
	rc = find_fn(w, opcode, ikeys, nr_keys);

	if (rc != 0) {
		if (missing_key)
			*missing_key = true;
		rc = M0_ERR(rc);
		goto do_exit_keys;
	}

	for (i = 0; i < nr_keys; i++) {
		keys[i].f_key = ikeys[i];
		keys[i].f_container = w->key_prefix.f_container;
	}

	/* populating key prefix */
	memset(kpart_one, 'A', kpart_one_size);

	rc = op->fill_kv(w, keys, &kv, nr_keys, kpart_one_size, kpart_one);
	if (rc != 0) {
		rc = M0_ERR(rc);
		goto do_exit_kv;
	}
	/* accumulate time required by each op on opcode basis. */
	op_start_time = m0_time_now();
	rc = cr_execute_query(&w->wit->index_fid, &kv, opcode);
	op_time = m0_time_sub(m0_time_now(), op_start_time);
	w->ciw_results.ciwr_ops_result[opcode].cior_ops_total_time_m0 =
			m0_time_add(w->ciw_results.ciwr_ops_result[opcode].cior_ops_total_time_m0,
			op_time);
	if (rc != 0) {
		rc = M0_ERR(rc);
		goto do_exit_kv;
	}

	if (op->readonly) {
		if (!random)
			cr_idx_w_seq_keys_save(w, ikeys, nr_keys, opcode);
	} else {
		for (i = 0; i < nr_keys; i++)
			m0_bitmap_set(&w->bm, ikeys[i], op->is_set_op);
	}

	crlog(CLL_TRACE, "Executed op: %s", crate_op_to_string(opcode));

do_exit_kv:
	kv_pair_fini(&kv);
do_exit_keys:
	m0_free0(&keys);
do_exit_ik:
	m0_free0(&ikeys);
do_exit:
	return M0_RC(rc);
}


static int cr_idx_w_get_nr_remained_ops(struct cr_idx_w *w)
{
	int result = 0;
	int i;

	if (w->nr_ops_total > 0)
		for (i = 0; i < ARRAY_SIZE(w->nr_ops); i++)
			result += w->nr_ops[i].nr;

	return result;
}

static enum cr_opcode cr_idx_w_select_op_rr(struct cr_idx_w *w, int depth)
{
	enum cr_opcode rc = CRATE_OP_START;
	int            max_ops_cnt = 0;
	int            i;

	M0_PRE(depth < CRATE_OP_NR);

	/* find opcode, which has max value of ops */
	for (i = 0; i < ARRAY_SIZE(w->nr_ops); i++) {
		if (w->nr_ops[i].pending)
			continue;
		if (w->nr_ops[i].nr > max_ops_cnt) {
			max_ops_cnt = w->nr_ops[i].nr;
			rc = (enum cr_opcode) i;
		}
	}

	if (max_ops_cnt == 0) {
		rc = CRATE_OP_INVALID;
		/* try to use pending operations */
		for (i = 0; i < ARRAY_SIZE(w->nr_ops); i++) {
			if (w->nr_ops[i].pending) {
				w->nr_ops[i].pending = false;
				rc = cr_idx_w_select_op_rr(w, depth);
				if (cr_opcode_valid(rc))
					break;
			}
		}
	}

	return rc;
}

static enum cr_opcode cr_idx_w_select_op(struct cr_idx_w *w)
{
	enum cr_opcode rc = CRATE_OP_START;

	if (cr_idx_w_get_nr_remained_ops(w) == 0) {
		crlog(CLL_INFO, "End of operations.");
		return CRATE_OP_INVALID;
	}

	switch (w->op_selector) {
	case CR_OP_SEL_RR:
		rc = cr_idx_w_select_op_rr(w, 0);
		break;

	case CR_OP_SEL_RND:
		do {
			rc = cr_rand_pos_range(ARRAY_SIZE(w->nr_ops));
		} while (w->nr_ops[rc].nr == 0);
		break;
	}

	return rc;
}

static int cr_idx_w_get_nr_remained_op_types(struct cr_idx_w *w)
{
	int result = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(w->nr_ops); i++) {
		if (w->nr_ops[i].nr)
			result++;
	}

	return result;

}

static void cr_idx_w_print_ops_table(struct cr_idx_w *w)
{
	M0_PRE(ARRAY_SIZE(w->nr_ops) == 4);
	crlog(CLL_TRACE, "ops remaining: [%d, %d, %d, %d]",
	      w->nr_ops[0].nr,
	      w->nr_ops[1].nr,
	      w->nr_ops[2].nr,
	      w->nr_ops[3].nr);
}

static bool cr_idx_w_rebalance_ops(struct cr_idx_w *w, enum cr_opcode op)
{
	bool result = false;
	int  i;

	if (cr_idx_w_get_nr_remained_ops(w) > 0
	    && cr_idx_w_get_nr_remained_op_types(w) > 1) {
		for (i = 0; i < ARRAY_SIZE(w->nr_ops); i++)
			w->nr_ops[i].pending = cr_rand_bool();
		result = true;
	} else
		crlog(CLL_WARN, "Reached end of keys.");

	return result;
}

static int cr_idx_w_common(struct cr_idx_w *w)
{
	int            rc;
	enum cr_opcode op;
	bool           is_random;
	bool           missing_key = false;
	int            nr_kv_per_op;

	cr_idx_w_seq_keys_init(w, w->nr_kv_per_op);

	while (true) {
		op = cr_idx_w_select_op(w);
		cr_idx_w_print_ops_table(w);

		if (!cr_opcode_valid(op)) {
			rc = M0_RC(0);
			break;
		}

		m0_bitmap_print(&w->bm);

		is_random = !w->wit->keys_ordered;

		nr_kv_per_op = cr_idx_w_get_nr_keys_per_op(w, op);
		crlog(CLL_DEBUG, "nr_kv_per_op: %d", nr_kv_per_op);

		rc = cr_idx_w_execute(w, op, is_random, nr_kv_per_op,
				      &missing_key);
		if (rc != 0) {
			/* try to select another op type */
			if (missing_key && (cr_idx_w_rebalance_ops(w, op)))
				continue;
			break;
		}
		w->nr_ops[op].nr--;
	}

	m0_bitmap_print(&w->bm);
	cr_idx_w_seq_keys_fini(w);

	return M0_RC(rc);
}

static int cr_idx_w_warmup(struct cr_idx_w *w)
{
	int rc = 0;
	int i;

	for (i = 0; i < w->warmup_put_cnt && rc == 0; i++) {
		m0_bitmap_print(&w->bm);
		rc = cr_idx_w_execute(w, CRATE_OP_PUT, false,
				      w->nr_kv_per_op, NULL);
	}

	for (i = 0; i < w->warmup_del_cnt && rc == 0; i++) {
		m0_bitmap_print(&w->bm);
		rc = cr_idx_w_execute(w, CRATE_OP_DEL, true,
				      w->nr_kv_per_op, NULL);
	}

	return M0_RC(rc);
}


static M0_UNUSED int delete_index(struct m0_uint128 id)
{
	int            rc;
	struct m0_op  *ops[1] = { NULL };
	struct m0_idx  idx = {};

	M0_PRE(crate_uber_realm() != NULL);
	M0_PRE(crate_uber_realm()->re_instance != NULL);

	/* Set an index deletion operation. */
	m0_idx_init(&idx, crate_uber_realm(), &id);

	rc = m0_entity_delete(&idx.in_entity, &ops[0]);
	if (rc == 0) {
		/* Launch and wait for op to complete */
		m0_op_launch(ops, 1);
		rc = m0_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					        M0_OS_STABLE),
				        M0_TIME_NEVER);

		if (rc != 0)
			crlog(CLL_ERROR, "Unable to perform index del");
		else if (ops[0]->op_sm.sm_rc != 0) {
			if (ops[0]->op_sm.sm_rc == -ENOENT)
				crlog(CLL_ERROR, "Index not found");
			else
				rc = ops[0]->op_sm.sm_rc;
		}
	} else
		crlog(CLL_ERROR, "Unable to set delete operation.");

	/* fini and release */
	if (ops[0] != NULL) {
		m0_op_fini(ops[0]);
		m0_op_free(ops[0]);
	}
	m0_idx_fini(&idx);

	return M0_RC(rc);
}

static int create_index(struct m0_uint128 id)
{
	int            rc;
	struct m0_op  *ops[1] = { NULL };
	struct m0_idx  idx = {};

	M0_PRE(crate_uber_realm() != NULL);
	M0_PRE(crate_uber_realm()->re_instance != NULL);

	/* Set an index creation operation. */
	m0_idx_init(&idx, crate_uber_realm(), &id);

	rc = m0_entity_create(NULL, &idx.in_entity, &ops[0]);
	if (rc == 0) {
		/* Launch and wait for op to complete */
		m0_op_launch(ops, 1);
		rc = m0_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					        M0_OS_STABLE),
				        M0_TIME_NEVER);

		if (rc != 0)
			crlog(CLL_ERROR, "Unable to perform index create");
		else if (ops[0]->op_sm.sm_rc != 0) {
			if (ops[0]->op_sm.sm_rc == -EEXIST)
				crlog(CLL_WARN, "Index alredy exists.");
			else
				rc = ops[0]->op_sm.sm_rc;
		}
	} else
		crlog(CLL_ERROR, "Unable to set index create operation.");

	/* fini and release */
	if (ops[0] != NULL) {
		m0_op_fini(ops[0]);
		m0_op_free(ops[0]);
	}
	m0_idx_fini(&idx);

	return M0_RC(rc);
}

struct cr_watchdog {
	struct m0_mutex lock;
	uint64_t counter;
	uint64_t sleep_sec;
	struct m0_thread thr;
};

static void cr_watchdog_thread(void *arg)
{
	struct cr_watchdog *w = arg;

	crlog(CLL_INFO, "Watchdog started");
	while (true) {
		m0_mutex_lock(&w->lock);
		if (w->counter == 0) {
			crlog(CLL_ERROR, "watchdog: blocked task detected."
			      " Exiting.");
			m0_nanosleep(m0_time(0, 100000), NULL);
			exit(EXIT_FAILURE);
		}
		w->counter = 0;
		m0_mutex_unlock(&w->lock);
		sleep(w->sleep_sec);
	}
}

static void cr_watchdog_touch()
{
	struct cr_watchdog *w = cr_watchdog;

	/* don't touch wg if it isn't exists */
	if (w == NULL)
		return;

	m0_mutex_lock(&w->lock);
	w->counter++;
	m0_mutex_unlock(&w->lock);
}

static int cr_watchdog_init(struct m0_workload_index *wt)
{
	int rc;

	M0_ALLOC_PTR(cr_watchdog);
	if (cr_watchdog == NULL)
		rc = M0_ERR(-ENOMEM);
	else {
		cr_watchdog->sleep_sec = wt->exec_time;
		cr_watchdog->counter = wt->exec_time;
		m0_mutex_init(&cr_watchdog->lock);

		rc = m0_thread_init(&cr_watchdog->thr, NULL, cr_watchdog_thread,
				    cr_watchdog, "watchdog");
	}

	if (rc != 0) {
		m0_free(cr_watchdog);
		crlog(CLL_ERROR, "Unable to init watchdog (%s)", strerror(-rc));
	}

	return M0_RC(rc);
}

static void cr_watchdog_fini()
{
	if (cr_watchdog == NULL)
		return;
	(void) m0_thread_signal(&cr_watchdog->thr, SIGTERM);
	(void) m0_thread_join(&cr_watchdog->thr);
	m0_thread_fini(&cr_watchdog->thr);
	m0_mutex_fini(&cr_watchdog->lock);
	m0_free0(&cr_watchdog);
}

static int index_operation(struct workload *wt,
			   struct m0_workload_task *task)
{
	struct cr_idx_w               w = {};
	struct cr_time_measure_ctx    t;
	struct m0_workload_index     *wit = wt->u.cw_index;
	struct m0_uint128             index_fid;
	int                           rc;

	M0_PRE(crate_uber_realm() != NULL);
	M0_PRE(crate_uber_realm()->re_instance != NULL);

	M0_CASSERT(sizeof(struct m0_uint128) == sizeof(struct m0_fid));
	memcpy(&index_fid, &wit->index_fid, sizeof(index_fid));

	cr_time_measure_begin(&t);

	if (wit->exec_time > 0) {
		rc = cr_watchdog_init(wit);
		if (rc != 0)
			goto do_exit;
	}

	rc = cr_idx_w_init(&w, wit);
	if (rc != 0)
		goto do_exit_wg;

	rc = create_index(index_fid);
	if (rc != 0)
		goto do_exit_idx_w;

	rc = cr_idx_w_warmup(&w);
	if (rc != 0)
		goto do_del_idx;

	rc = cr_idx_w_common(&w);
	if (rc != 0)
		goto do_del_idx;

do_del_idx:
	/* XXX: index deletion disabled. */
#if 0
	rc = delete_index(index_fid);
	M0_ASSERT(rc != 0);
#endif
do_exit_idx_w:
	cr_idx_w_fini(&w);
do_exit_wg:
	cr_watchdog_fini();
	cr_time_measure_end(&t);
	cr_time_capture_results(&t, &w);
	cr_time_measure_report(&t, w);
do_exit:
	return M0_RC(rc);
}

void run_index(struct workload *w, struct workload_task *tasks)
{
	workload_start(w, tasks);
	workload_join(w, tasks);
}

void m0_op_run_index(struct workload *w, struct workload_task *task,
			 const struct workload_op *op)
{
	struct m0_workload_task     *m0_task;
	int                          rc;
	bool			     is_m0_thread;

	M0_PRE(crate_uber_realm() != NULL);
	M0_PRE(crate_uber_realm()->re_instance != NULL);

	if (NULL == M0_ALLOC_PTR(m0_task)) {
		crlog(CLL_ERROR, "Out of memory.");
		exit(EXIT_FAILURE);
	}

	is_m0_thread = m0_thread_tls() != NULL;

	if (!is_m0_thread) {
		rc = adopt_motr_thread(m0_task);
		if (rc != 0) {
			crlog(CLL_ERROR, "Unable to adopt thread (%s)",
			      strerror(-rc));
			goto exit_free;
		}
	}

	rc = index_operation(w, m0_task);
	if (rc != 0) {
		crlog(CLL_ERROR,
		      "Failed to perform index operation (%s)",
		      strerror(-rc));
	}

	if (!is_m0_thread) {
		release_motr_thread(m0_task);
	}

exit_free:
	m0_free(m0_task);
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
