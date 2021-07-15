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
#include "lib/misc.h"       /* m0_full_name_hash */
#include "fid/fid.h"        /* m0_fid */
#include "lib/buf.h"        /* m0_buf */
#include "lib/string.h"     /* m0_strdup */
#include "iscservice/isc.h" /* m0_isc_comp_register */
#include "fop/fom.h"        /* M0_FSO_AGAIN */
#include "stob/io.h"        /* m0_stob_io_init */
#include "ioservice/fid_convert.h" /* m0_fid_convert_cob2stob */
#include "ioservice/storage_dev.h" /* m0_storage_dev_stob_find */
#include "motr/setup.h"     /* m0_cs_storage_devs_get */

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

enum op {MIN, MAX};

static void stio_fini(struct m0_stob_io *stio, struct m0_stob *stob)
{
	m0_indexvec_free(&stio->si_stob);
	m0_free(stio->si_user.ov_buf[0]);
	m0_bufvec_free2(&stio->si_user);
	m0_stob_io_fini(stio);
	m0_storage_dev_stob_put(m0_cs_storage_devs_get(), stob);
}

static void bufvec_pack(struct m0_bufvec *bv, uint32_t shift)
{
	uint32_t i;

	for (i = 0; i < bv->ov_vec.v_nr; i++) {
		bv->ov_vec.v_count[i] >>= shift;
		bv->ov_buf[i] = m0_stob_addr_pack(bv->ov_buf[i], shift);
	}
}

static void bufvec_open(struct m0_bufvec *bv, uint32_t shift)
{
	uint32_t i;

	for (i = 0; i < bv->ov_vec.v_nr; i++) {
		bv->ov_vec.v_count[i] <<= shift;
		bv->ov_buf[i] = m0_stob_addr_open(bv->ov_buf[i], shift);
	}
}

static int bufvec_alloc_init(struct m0_bufvec *bv, struct m0_io_indexvec *iiv,
			     uint32_t shift)
{
	int   rc;
	int   i;
	char *p;

	p = m0_alloc_aligned(m0_io_count(iiv), shift);
	if (p == NULL)
		return M0_ERR_INFO(-ENOMEM, "failed to allocate buf");

	rc = m0_bufvec_empty_alloc(bv, iiv->ci_nr);
	if (rc != 0) {
		m0_free(p);
		return M0_ERR_INFO(rc, "failed to allocate bufvec");
	}

	for (i = 0; i < iiv->ci_nr; i++) {
		bv->ov_buf[i] = p;
		bv->ov_vec.v_count[i] = iiv->ci_iosegs[i].ci_count;
		p += iiv->ci_iosegs[i].ci_count;
	}

	return 0;
}

int launch_stob_io(struct m0_isc_comp_private *pdata,
		   struct m0_buf *in, int *rc)
{
	uint32_t           shift = 0;
	struct m0_stob_io *stio = (struct m0_stob_io *)pdata->icp_data;
	struct m0_fom     *fom = pdata->icp_fom;
	struct isc_targs   ta = {};
	struct m0_stob_id  stob_id;
	struct m0_stob    *stob = NULL;

	*rc = m0_xcode_obj_dec_from_buf(&M0_XCODE_OBJ(isc_targs_xc, &ta),
					in->b_addr, in->b_nob);
	if (*rc != 0) {
		M0_LOG(M0_ERROR, "failed to xdecode args: rc=%d", *rc);
		return M0_FSO_AGAIN;
	}

	if (ta.ist_ioiv.ci_nr == 0) {
		M0_LOG(M0_ERROR, "no io segments given");
		*rc = -EINVAL;
		goto err;
	}

	m0_stob_io_init(stio);
	stio->si_opcode = SIO_READ;

	m0_fid_convert_cob2stob(&ta.ist_cob, &stob_id);
	*rc = m0_storage_dev_stob_find(m0_cs_storage_devs_get(),
				       &stob_id, &stob);
	if (*rc != 0) {
		M0_LOG(M0_ERROR, "failed to find stob by cob="FID_F": rc=%d",
		       FID_P(&ta.ist_cob), *rc);
		goto err;
	}

	shift = m0_stob_block_shift(stob);

	*rc = m0_indexvec_wire2mem(&ta.ist_ioiv, ta.ist_ioiv.ci_nr,
				   shift, &stio->si_stob);
	if (*rc != 0) {
		M0_LOG(M0_ERROR, "failed to make cob ivec: rc=%d", *rc);
		goto err;
	}

	*rc = bufvec_alloc_init(&stio->si_user, &ta.ist_ioiv, shift);
	if (*rc != 0) {
		M0_LOG(M0_ERROR, "failed to allocate bufvec: rc=%d", *rc);
		goto err;
	}

	bufvec_pack(&stio->si_user, shift);

	*rc = m0_stob_io_private_setup(stio, stob);
	if (*rc != 0) {
		M0_LOG(M0_ERROR, "failed to setup adio for stob="FID_F": rc=%d",
		       FID_P(&stob->so_id.si_fid), *rc);
		goto err;
	}

	/* make sure the fom is waked up on I/O completion */
	m0_mutex_lock(&stio->si_mutex);
	m0_fom_wait_on(fom, &stio->si_wait, &fom->fo_cb);
	m0_mutex_unlock(&stio->si_mutex);

	*rc = m0_stob_io_prepare_and_launch(stio, stob, NULL, NULL);
	if (*rc != 0) {
		M0_LOG(M0_ERROR, "failed to launch io for stob="FID_F": rc=%d",
		       FID_P(&stob->so_id.si_fid), *rc);
		m0_mutex_lock(&stio->si_mutex);
		m0_fom_callback_cancel(&fom->fo_cb);
		m0_mutex_unlock(&stio->si_mutex);
	}
 err:
	if (*rc != 0) {
		if (stob != NULL) {
			bufvec_open(&stio->si_user, shift);
			stio_fini(stio, stob);
		}
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
	uint32_t          shift;
	double            val;
	FILE             *f;
	char             *p;
	struct mm_result  res = {};
	struct m0_buf     buf = M0_BUF_INIT0;
	struct m0_stob_io *stio = (struct m0_stob_io *)pdata->icp_data;

	shift = m0_stob_block_shift(stio->si_obj);
	bufvec_open(&stio->si_user, shift);
	p = stio->si_user.ov_buf[0];

	f = fmemopen(p, stio->si_count << shift, "r");
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

int do_minmax(enum op op, struct m0_buf *in, struct m0_buf *out,
	      struct m0_isc_comp_private *data, int *rc)
{
	int                res;
	struct m0_stob_io *stio = (struct m0_stob_io *)data->icp_data;

	if (stio == NULL) {
		M0_ALLOC_PTR(stio);
		if (stio == NULL) {
			*rc = -ENOMEM;
			return M0_FSO_AGAIN;
		}
		data->icp_data = stio;
		res = launch_stob_io(data, in, rc);
		if (*rc != -EAGAIN)
			m0_free(stio);
	} else {
		res = compute_minmax(op, data, out, rc);
		stio_fini(stio, stio->si_obj);
		m0_free(stio);
	}

	return res;
}

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

static void comp_reg(const char *f_name, int (*ftn)(struct m0_buf *arg_in,
						    struct m0_buf *args_out,
					            struct m0_isc_comp_private
					            *comp_data, int *rc))
{
	struct m0_fid comp_fid;
	int           rc;

	m0util_isc_fid_get(f_name, &comp_fid);
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
	m0_xc_iscservice_demo_libdemo_init();
}
