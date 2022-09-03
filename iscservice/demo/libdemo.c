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

#include "lib/trace.h"      /* M0_LOG */
#include "lib/memory.h"     /* m0_alloc */
#include "lib/string.h"     /* m0_strdup */
#include "lib/misc.h"       /* m0_full_name_hash */
#include "lib/buf.h"        /* m0_buf */
#include "fid/fid.h"        /* m0_fid */
#include "fop/fom.h"        /* M0_FSO_AGAIN */
#include "stob/io.h"        /* m0_stob_io_init */
#include "iscservice/isc.h" /* m0_isc_comp_register */

#include "util.h"
#include "common.h"
#include "libdemo.h"
#include "libdemo_xc.h"

static bool is_valid_string(struct m0_buf *in)
{
	return m0_buf_streq(in, "hello") || m0_buf_streq(in, "Hello") ||
		m0_buf_streq(in, "HELLO");
}

int hello_world(struct m0_buf *in, struct m0_buf *out,
	        struct m0_isc_comp_private *comp_data, int *rc)
{
	char *out_str;

	if (is_valid_string(in)) {
		/*
		 * Note: The out buffer allocated here is freed
		 * by iscservice, and a computation shall not free
		 * it in the end of computation.
		 */
		out_str = m0_strdup("world");
		if (out_str != NULL) {
			m0_buf_init(out, out_str, strlen(out_str));
			*rc = 0;
		} else
			*rc = -ENOMEM;
	} else
		*rc = -EINVAL;

	return M0_FSO_AGAIN;
}

enum op {
	MIN, MAX,  /* data format: floating point strings */
	MIN2, MAX2 /* 8-bytes doubles */
};

int launch_io(struct m0_isc_comp_private *pdata, struct m0_buf *in, int *rc)
{
	struct m0_stob_io *stio = (struct m0_stob_io *)pdata->icp_data;
	struct m0_fom     *fom = pdata->icp_fom;
	struct isc_targs   ta = {};

	*rc = m0_xcode_obj_dec_from_buf(&M0_XCODE_OBJ(isc_targs_xc, &ta),
					in->b_addr, in->b_nob);
	if (*rc != 0) {
		M0_LOG(M0_ERROR, "failed to xdecode args: rc=%d", *rc);
		return M0_FSO_AGAIN;
	}

	*rc = m0_isc_io_launch(stio, &ta.ist_cob, &ta.ist_ioiv, fom);
	if (*rc != 0) {
		/*
		 * EAGAIN has a special meaning in the calling isc code,
		 * so make sure we don't return it by accident.
		 */
		if (*rc == -EAGAIN)
			*rc = -EBUSY;
		return M0_FSO_AGAIN;
	}

	*rc = -EAGAIN; /* Wait for the I/O result. */
	return M0_FSO_WAIT;
}

int compute_minmax(enum op op, struct m0_isc_comp_private *pdata,
		   struct m0_buf *out, int *rc)
{
	int               i = 0;
	int               n;
	double            val;
	FILE             *f;
	char             *p;
	m0_bcount_t       len;
	struct mm_result  res = {};
	struct m0_buf     buf = M0_BUF_INIT0;

	len = m0_isc_io_res((struct m0_stob_io *)pdata->icp_data, &p);
	if (len < 0) {
		*rc = M0_ERR_INFO((int)len, "failed to read data");
		return M0_FSO_AGAIN;
	}

	f = fmemopen(p, len, "r");
	if (f == NULL) {
		*rc = M0_ERR(-errno);
		return M0_FSO_AGAIN;
	}

	/*
	 * 1st value should go to the lbuf always, because it can be
	 * the right cut of the last value from the previous unit.
	 *
	 * We count it as the 1st element in the buffer.
	 */
	if (fscanf(f, "%lf %n", &val, &n) < 1) {
		fclose(f);
		*rc = M0_ERR(-EINVAL);
		return M0_FSO_AGAIN;
	}
	res.mr_lbuf.b_addr = p;
	res.mr_lbuf.b_nob = n;
	res.mr_nr = 1;

	/* Read 1st element to compare with (2nd element in the buffer). */
	if (fscanf(f, "%lf ", &val) < 1)
		goto out;
	res.mr_val = val;
	res.mr_idx = 1;

	for (i = 2; fscanf(f, "%lf %n", &val, &n) > 0 && !feof(f); i++) {
		if (op == MIN ? val < res.mr_val :
		                val > res.mr_val) {
			res.mr_idx = i;
			res.mr_val = val;
		}
	}
	res.mr_nr = i;
	/*
	 * Last value should always go to the rbuf, because it can be
	 * the left cut of the first value in the next unit.
	 *
	 * rbuf element is counted as lbuf in the next unit. It may
	 * happen that there is no cut of element on the border. I.e.
	 * the last element is fully present at the end of the unit and
	 * the 1st element is fully present in the beginning of the next
	 * unit. This case must be checked by the client code (it should
	 * assume that there might be two elements present in the result
	 * of gluing rbuf and lbuf of who units). And if so, the index
	 * and the number of elements should be incremented by client.
	 */
	res.mr_rbuf.b_addr = p + ftell(f) - n;
	res.mr_rbuf.b_nob = n;
 out:
	*rc = m0_xcode_obj_enc_to_buf(&M0_XCODE_OBJ(mm_result_xc, &res),
				      &buf.b_addr, &buf.b_nob) ?:
	      m0_buf_copy_aligned(out, &buf, M0_0VEC_SHIFT);

	m0_buf_free(&buf);
	fclose(f);

	return M0_FSO_AGAIN;
}

int compute_minmax2(enum op op, struct m0_isc_comp_private *pdata,
		    struct m0_buf *out, int *rc)
{
	int               i;
	double           *p;
	m0_bcount_t       len;
	struct mm_result  res = {};
	struct m0_buf     buf = M0_BUF_INIT0;

	len = m0_isc_io_res((struct m0_stob_io *)pdata->icp_data, (char**)&p);
	if (len < 0) {
		*rc = M0_ERR_INFO((int)len, "failed to read data");
		return M0_FSO_AGAIN;
	}

	M0_ASSERT((len & 7) == 0); /* unit size is multiple of 8 */
	len /= 8; /* Number of 8-bytes doubles. */

	/* Read 1st element to compare with. */
	res.mr_val = p[0];
	res.mr_idx = 1;

	for (i = 1; i < len; i++) {
		if (op == MIN2 ? p[i] < res.mr_val :
		                 p[i] > res.mr_val) {
			res.mr_idx = i + 1;
			res.mr_val = p[i];
		}
	}
	res.mr_nr = i + 1;

	*rc = m0_xcode_obj_enc_to_buf(&M0_XCODE_OBJ(mm_result_xc, &res),
				      &buf.b_addr, &buf.b_nob) ?:
	      m0_buf_copy_aligned(out, &buf, M0_0VEC_SHIFT);

	m0_buf_free(&buf);

	return M0_FSO_AGAIN;
}

/**
 * Do the computation of min/max.
 *
 * This function is called two times by the ISC-implementation.
 * The 1st time we start the I/O to read the data from the object
 * unit, and return M0_FSO_WAIT and -EAGAIN in rc argument as an
 * indication that the function should be called again when the
 * data is ready. When I/O is complete and the data is ready, the
 * function is called again so we can actually do the computation.
 */
int do_minmax(enum op op, struct m0_buf *in, struct m0_buf *out,
	      struct m0_isc_comp_private *data, int *rc)
{
	int                res;
	struct m0_stob_io *stio = (struct m0_stob_io *)data->icp_data;

	if (stio == NULL) { /* 1st call */
		M0_ALLOC_PTR(stio);
		if (stio == NULL) {
			*rc = -ENOMEM;
			return M0_FSO_AGAIN;
		}
		data->icp_data = stio;
		res = launch_io(data, in, rc);
		if (*rc != -EAGAIN)
			m0_free(stio);
	} else {
		if (op == MIN || op == MAX)
			res = compute_minmax(op, data, out, rc);
		else /* MIN2 || MAX2 */
			res = compute_minmax2(op, data, out, rc);
		m0_isc_io_fini(stio);
		m0_free(stio);
	}

	return res;
}

/**
 * Compute minimum value in the unit data.
 *
 * The computation function interface is defined at isc.h:
 * the arguments and result are xcoded at @in and @out buffers,
 * @comp_data contains private data for the computation which
 * is preserved between the two calls of this function (before
 * reading the data from the storage device and after the read
 * is finished). @rc contains the result of the computation.
 *
 * @return M0_FSO_WAIT after read I/O is started
 *                     (rc should be == -EAGAIN also)
 * @return M0_FSO_AGAIN after the computation is complete
 */
int comp_min(struct m0_buf *in, struct m0_buf *out,
	     struct m0_isc_comp_private *comp_data, int *rc)
{
	return do_minmax(MIN, in, out, comp_data, rc);
}

int comp_max(struct m0_buf *in, struct m0_buf *out,
	     struct m0_isc_comp_private *comp_data, int *rc)
{
	return do_minmax(MAX, in, out, comp_data, rc);
}

int comp_min2(struct m0_buf *in, struct m0_buf *out,
	      struct m0_isc_comp_private *comp_data, int *rc)
{
	return do_minmax(MIN2, in, out, comp_data, rc);
}

int comp_max2(struct m0_buf *in, struct m0_buf *out,
	      struct m0_isc_comp_private *comp_data, int *rc)
{
	return do_minmax(MAX2, in, out, comp_data, rc);
}

static void comp_reg(const char *f_name, int (*ftn)(struct m0_buf *arg_in,
						    struct m0_buf *args_out,
					            struct m0_isc_comp_private
					            *comp_data, int *rc))
{
	struct m0_fid comp_fid;
	int           rc;

	isc_fid_get(f_name, &comp_fid);
	rc = m0_isc_comp_register(ftn, f_name, &comp_fid);
	if (rc == -EEXIST)
		fprintf(stderr, "Computation already exists");
	else if (rc == -ENOMEM)
		fprintf(stderr, "Out of memory");
}

void motr_lib_init(void)
{
	comp_reg("hello_world", hello_world);
	comp_reg("comp_min", comp_min);
	comp_reg("comp_max", comp_max);
	comp_reg("comp_min2", comp_min2);
	comp_reg("comp_max2", comp_max2);
	m0_xc_iscservice_demo_libdemo_init();
}
