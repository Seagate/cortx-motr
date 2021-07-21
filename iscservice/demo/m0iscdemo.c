/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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
 */

#include <unistd.h>      /* getopt */

#include "lib/memory.h"  /* m0_alloc m0_free */
#include "rpc/conn.h"    /* m0_rpc_conn_addr */
#include "rpc/session.h"    /* iop_session */
#include "xcode/xcode.h"
#include "conf/schema.h" /* M0_CST_ISCS */
#include "layout/layout.h"  /* M0_DEFAULT_LAYOUT_ID */
#include "layout/plan.h"  /* m0_layout_plan_build */

#include "util.h"
#include "common.h"
#include "libdemo.h"      /* mm_result */
#include "libdemo_xc.h"   /* isc_args_xc */


enum isc_comp_type {
	ICT_PING,
	ICT_MIN,
	ICT_MAX,
};

static int op_type_parse(const char *op_name)
{
	if (op_name == NULL)
		return -EINVAL;
	else if (!strcmp(op_name, "ping"))
		return ICT_PING;
	else if (!strcmp(op_name, "min"))
		return ICT_MIN;
	else if (!strcmp(op_name, "max"))
		return ICT_MAX;
	else
		return -EINVAL;

}

static int minmax_input_prepare(struct m0_buf *out, struct m0_fid *comp_fid,
				struct m0_layout_io_plop *iop,
				uint32_t *reply_len, enum isc_comp_type type)
{
	int           rc;
	struct m0_buf buf = M0_BUF_INIT0;
	struct isc_targs ta = {};

	if (iop->iop_ext.iv_vec.v_nr == 0) {
		fprintf(stderr, "at least 1 segment required\n");
		return -EINVAL;
	}
	ta.ist_cob = iop->iop_base.pl_ent;
	rc = m0_indexvec_mem2wire(&iop->iop_ext, iop->iop_ext.iv_vec.v_nr, 0,
				  &ta.ist_ioiv);
	if (rc != 0)
		return rc;
	rc = m0_xcode_obj_enc_to_buf(&M0_XCODE_OBJ(isc_targs_xc, &ta),
				     &buf.b_addr, &buf.b_nob);
	if (rc != 0)
		return rc;

	*out = M0_BUF_INIT0; /* to avoid panic */
	rc = m0_buf_copy_aligned(out, &buf, M0_0VEC_SHIFT);
	m0_buf_free(&buf);

	if (type == ICT_MIN)
		isc_fid_get("comp_min", comp_fid);
	else
		isc_fid_get("comp_max", comp_fid);

	*reply_len = CBL_DEFAULT_MAX;

	return rc;
}

static int ping_input_prepare(struct m0_buf *buf, struct m0_fid *comp_fid,
			      uint32_t *reply_len, enum isc_comp_type type)
{
	char *greeting;

	*buf = M0_BUF_INIT0;
	greeting = m0_strdup("Hello");
	if (greeting == NULL)
		return -ENOMEM;

	m0_buf_init(buf, greeting, strlen(greeting));
	isc_fid_get("hello_world", comp_fid);
	*reply_len = CBL_DEFAULT_MAX;

	return 0;
}

static int input_prepare(struct m0_buf *buf, struct m0_fid *comp_fid,
			 struct m0_layout_io_plop *iop, uint32_t *reply_len,
			 enum isc_comp_type type)
{
	switch (type) {
	case ICT_PING:
		return ping_input_prepare(buf, comp_fid, reply_len, type);
	case ICT_MIN:
	case ICT_MAX:
		return minmax_input_prepare(buf, comp_fid, iop,
					    reply_len, type);
	}
	return -EINVAL;
}

static struct mm_result *
op_result(struct mm_result *x, struct mm_result *y, enum isc_comp_type op_type)
{
	int               rc;
	int               len;
	char             *buf;
	double            x_rval;
	double            y_lval;

	len = x->mr_rbuf.b_nob + y->mr_lbuf.b_nob;
	buf = malloc(x->mr_rbuf.b_nob + y->mr_lbuf.b_nob + 1);
	if (buf == NULL) {
		fprintf(stderr, "failed to allocate %d of memory for result\n",
			                            len);
		return NULL;
	}

	memcpy(buf, x->mr_rbuf.b_addr, x->mr_rbuf.b_nob);
	buf[x->mr_rbuf.b_nob] = '\0';
	DBG2("xrbuf=%s\n", buf);
	memcpy(buf + x->mr_rbuf.b_nob, y->mr_lbuf.b_addr, y->mr_lbuf.b_nob);
	buf[x->mr_rbuf.b_nob + y->mr_lbuf.b_nob] = '\0';

	rc = sscanf(buf, "%lf%n", &x_rval, &len);
	if (rc < 1) {
		fprintf(stderr, "failed to read the resulting xr-value\n");
		m0_free(buf);
		return NULL;
	}

	y->mr_idx += x->mr_nr;
	y->mr_nr  += x->mr_nr;

	DBG2("buf=%s x_rval=%lf\n", buf, x_rval);

	rc = sscanf(buf + len, "%lf", &y_lval);
	if (rc < 1) {
		y_lval = x_rval;
	} else {
		DBG2("y_lval=%lf\n", y_lval);
		y->mr_idx++;
		y->mr_nr++;
	}

	DBG2("xval=%lf yval=%lf\n", x->mr_val, y->mr_val);

	if (ICT_MIN == op_type)
		y->mr_val = min3(min_check(x->mr_val, y->mr_val),
				 x_rval, y_lval);
	else
		y->mr_val = max3(max_check(x->mr_val, y->mr_val),
				 x_rval, y_lval);

	if (y->mr_val == x->mr_val)
		y->mr_idx = x->mr_idx;
	else if (y->mr_val == x_rval)
		y->mr_idx = x->mr_nr;
	else if (y->mr_val == y_lval)
		y->mr_idx = x->mr_nr + 1;

	m0_free(buf);

	return y;
}

enum elm_order {
	ELM_FIRST,
	ELM_LAST
};

static void set_idx(struct mm_result *res, enum elm_order e)
{
	if (ELM_FIRST == e)
		res->mr_idx = 0;
	else
		res->mr_idx = res->mr_nr;
}

static void check_edge_val(struct mm_result *res, enum elm_order e,
			   enum isc_comp_type type)
{
	const char *buf;
	double      val;

	if (ELM_FIRST == e)
		buf = res->mr_lbuf.b_addr;
	else // last
		buf = res->mr_rbuf.b_addr;

	if (sscanf(buf, "%lf", &val) < 1) {
		fprintf(stderr, "failed to parse egde value=%s\n", buf);
		return;
	}

	DBG2("edge val=%lf (%s)\n", val, (ELM_FIRST == e) ? "first" : "last");

	if (res->mr_nr == 1) {
		res->mr_val = val;
		return;
	}

	if (ICT_MIN == type && val < res->mr_val) {
		res->mr_val = val;
		set_idx(res, e);
	} else if (ICT_MAX == type && val > res->mr_val) {
		res->mr_val = val;
		set_idx(res, e);
	}
}

static void mm_result_free_xcode_bufs(struct mm_result *r)
{
	m0_free(r->mr_lbuf.b_addr);
	m0_free(r->mr_rbuf.b_addr);
}

static void *minmax_output_prepare(struct m0_buf *result,
				   bool last_unit,
				   struct mm_result *prev,
				   enum isc_comp_type type)
{
	int               rc;
	struct mm_result  new = {};

	rc = m0_xcode_obj_dec_from_buf(&M0_XCODE_OBJ(mm_result_xc, &new),
				       result->b_addr, result->b_nob);
	if (rc != 0) {
		fprintf(stderr, "failed to parse result: rc=%d\n", rc);
		goto out;
	}
	if (prev == NULL) {
		M0_ALLOC_PTR(prev);
		check_edge_val(&new, ELM_FIRST, type);
		*prev = new;
		goto out;
	}

	if (last_unit)
		check_edge_val(&new, ELM_LAST, type);

	/* Copy the current resulting value. */
	if (op_result(prev, &new, type)) {
		mm_result_free_xcode_bufs(prev);
		*prev = new;
	}
 out:
	/* Print the result. */
	if (last_unit && prev != NULL) {
		printf("idx=%lu val=%lf\n", prev->mr_idx, prev->mr_val);
		mm_result_free_xcode_bufs(prev);
		m0_free(prev);
		prev = NULL;
	}

	return prev;
}

/**
 * Apart from processing the output this can deserialize the buffer into
 * a structure relevant to the result of invoked computation.
 * and return the same.
 */
static void* output_process(struct m0_buf *result, bool last,
			    void *out, enum isc_comp_type type)
{
	switch (type) {
	case ICT_PING:
		printf ("Hello-%s @%s\n", (char*)result->b_addr, (char*)out);
		memset(result->b_addr, 'a', result->b_nob);
		return NULL;
	case ICT_MIN:
	case ICT_MAX:
		return minmax_output_prepare(result, last, out, type);
	}
	return NULL;
}

char *prog;

const char *help_str = "\
\n\
Usage: %s OPTIONS COMP OBJ_ID LEN\n\
\n\
 Supported COMPutations: ping, min, max\n\
\n\
 OBJ_ID is two uint64 numbers in hi:lo format (dec or hex)\n\
 LEN    is the length of object (in KiB)\n\
\n\
 Motr-related mandatory options:\n\
   -e <addr>  endpoint address\n\
   -x <addr>  ha-agent (hax) endpoint address\n\
   -f <fid>   process fid\n\
   -p <fid>   profile fid\n\
\n\
 Other non-mandatory options:\n\
   -v  increase verbosity (-vv to increase even more)\n\
   -h  this help\n\
\n";

static void usage()
{
	fprintf(stderr, help_str, prog);
	exit(1);
}

/**
 * Read obj id in the format [hi:]lo.
 * Return how many numbers were read.
 */
int read_id(const char *s, struct m0_uint128 *id)
{
	int res;
	long long hi, lo;

	res = sscanf(s, "%lli:%lli", &hi, &lo);
	if (res == 1) {
		id->u_hi = 0;
		id->u_lo = hi;
	} else if (res == 2) {
		id->u_hi = hi;
		id->u_lo = lo;
	}

	return res;
}

int launch_comp(struct m0_layout_plan *plan, int op_type, bool last)
{
	int                    rc;
	int                    reqs_nr = 0;
	uint32_t               reply_len;
	struct isc_req *req;
	struct m0_layout_plop *plop = NULL;
	struct m0_layout_plop *prev_plop;
	struct m0_layout_io_plop *iopl;
	static void           *out_args = NULL; /* computation output */
	const char            *conn_addr = NULL;
	struct m0_fid          comp_fid;
	struct m0_buf          buf;

	for (;;) {
		M0_ALLOC_PTR(req);
		if (req == NULL) {
			fprintf(stderr, "request allocation failed\n");
			break;
		}
		prev_plop = plop;
		rc = m0_layout_plan_get(plan, 0, &plop);
		if (rc != 0) {
			fprintf(stderr, "failed to get plop: rc=%d\n", rc);
			usage();
		}

		if (plop->pl_type == M0_LAT_DONE)
			break;

		if (plop->pl_type == M0_LAT_OUT_READ) {
			/* XXX just to be sure, for now only */
			M0_ASSERT(prev_plop != NULL &&
				  prev_plop->pl_type == M0_LAT_READ);
			continue;
		}

		M0_ASSERT(plop->pl_type == M0_LAT_READ); /* XXX for now */
		m0_layout_plop_start(plop);

		iopl = container_of(plop, struct m0_layout_io_plop, iop_base);

		DBG("req=%d goff=%lu segs=%d\n", reqs_nr, iopl->iop_goff,
		                                 iopl->iop_ext.iv_vec.v_nr);
		/* Prepare arguments for computation. */
		rc = input_prepare(&buf, &comp_fid, iopl, &reply_len, op_type);
		if (rc != 0) {
			m0_layout_plop_done(plop);
			fprintf(stderr, "input preparation failed: %d\n", rc);
			break;
		}
		rc = isc_req_prepare(req, &buf, &comp_fid, iopl,
					    reply_len);
		if (rc != 0) {
			m0_buf_free(&buf);
			m0_layout_plop_done(plop);
			m0_free(req);
			fprintf(stderr, "request preparation failed: %d\n", rc);
			break;
		}

		rc = isc_req_send(req);
		conn_addr = m0_rpc_conn_addr(iopl->iop_session->s_conn);
		if (rc != 0) {
			fprintf(stderr, "error from %s received: rc=%d\n",
				conn_addr, rc);
			break;
		}
		reqs_nr++;
	}

	/* wait for all the replies */
	while (reqs_nr-- > 0)
		m0_semaphore_down(&isc_sem);

	/* process the replies */
	isc_reqs_teardown(req) {
		iopl = M0_AMB(iopl, req->cir_plop, iop_base);
		DBG2("req=%d goff=%lu\n", ++reqs_nr, iopl->iop_goff);
		if (rc == 0 && req->cir_rc == 0) {
			if (op_type == ICT_PING)
				out_args = (void*)conn_addr;
			out_args = output_process(&req->cir_result,
					  last && m0_list_is_empty(&isc_reqs),
						  out_args, op_type);
		}
		m0_layout_plop_done(req->cir_plop);
		isc_req_fini(req);
		m0_free(req);
	}

	return rc;
}

static int open_entity(struct m0_entity *entity)
{
	int                  rc;
	struct m0_op *op = NULL;

	m0_entity_open(entity, &op);
	m0_op_launch(&op, 1);
	rc = m0_op_wait(op, M0_BITS(M0_OS_FAILED,
					       M0_OS_STABLE),
			       M0_TIME_NEVER) ?: m0_rc(op);
	m0_op_fini(op);
	m0_op_free(op);

	return rc;
}

int main(int argc, char **argv)
{
	int                    rc;
	int                    opt;
	struct m0_op          *op = NULL;
	struct m0_layout_plan *plan;
	struct m0_uint128      obj_id;
	struct m0_indexvec     ext;
	struct m0_bufvec       data;
	struct m0_bufvec       attr;
	struct m0_config       conf = {};
	int                    op_type;
	int                    unit_sz;
	int                    units_nr;
	m0_bcount_t            len;
	m0_bcount_t            bs;
	m0_bindex_t            off = 0;
	struct m0_obj          obj = {};

	prog = basename(strdup(argv[0]));

	while ((opt = getopt(argc, argv, ":vhe:x:f:p:")) != -1) {
		switch (opt) {
		case 'e':
			conf.mc_local_addr = optarg;
			break;
		case 'x':
			conf.mc_ha_addr = optarg;
			break;
		case 'f':
			conf.mc_process_fid = optarg;
			break;
		case 'p':
			conf.mc_profile = optarg;
			break;
		case 'v':
			trace_level++;
			break;
		case 'h':
			usage();
			break;
		default:
			fprintf(stderr, "unknown option: %c\n", optopt);
			usage();
			break;
		}
	}

	if (conf.mc_local_addr == NULL || conf.mc_ha_addr == NULL ||
	    conf.mc_process_fid == NULL || conf.mc_profile == NULL) {
		fprintf(stderr, "mandatory parameter is missing\n");
		usage();
	}
	if (argc - optind < 3)
		usage();

	op_type = op_type_parse(argv[optind]);
	if (op_type == -EINVAL)
		usage();
	if (read_id(argv[optind + 1], &obj_id) < 1)
		usage();
	len = atoll(argv[optind + 2]);
	if (len < 4) {
		fprintf(stderr, "object length should be at least 4K\n");
		usage();
	}
	len *= 1024;

	m0trace_on = true;

	rc = isc_init(&conf);
	if (rc != 0) {
		fprintf(stderr,"isc_init() failed: %d\n", rc);
		usage();
	}

	m0_xc_iscservice_demo_libdemo_init();

	m0_obj_init(&obj, &uber_realm, &obj_id, M0_DEFAULT_LAYOUT_ID);
	rc = open_entity(&obj.ob_entity);
	if (rc != 0) {
		fprintf(stderr, "failed to open object: rc=%d\n", rc);
		usage();
	}

	bs = isc_m0gs(&obj);
	if (bs == 0) {
		fprintf(stderr, "cannot figure out bs to use\n");
		usage();
	}
	unit_sz = m0_obj_layout_id_to_unit_size(obj.ob_attr.oa_layout_id);

	for (; rc == 0 && len > 0; len -= len < bs ? len : bs, off += bs) {
		if (len < bs)
			bs = (len + unit_sz - 1) / unit_sz * unit_sz;
		units_nr = bs / unit_sz;
		DBG("unit_sz=%d units=%d\n", unit_sz, units_nr);

		rc = alloc_segs(&data, &ext, &attr, unit_sz, units_nr);
		if (rc != 0) {
			fprintf(stderr, "failed to alloc_segs: rc=%d\n", rc);
			usage();
		}
		set_exts(&ext, off, unit_sz);

		rc = m0_obj_op(&obj, M0_OC_READ, &ext, &data, &attr, 0, 0, &op);
		if (rc != 0) {
			fprintf(stderr, "failed to create op: rc=%d\n", rc);
			usage();
		}

		plan = m0_layout_plan_build(op);
		if (plan == NULL) {
			fprintf(stderr, "failed to build access plan\n");
			usage();
		}

		rc = launch_comp(plan, op_type, len <= bs ? true : false);

		m0_layout_plan_fini(plan);
		free_segs(&data, &ext, &attr);
	}

	/* free resources*/
	isc_free();

	return rc;
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
