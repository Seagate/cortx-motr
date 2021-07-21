/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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


#include "ut/ut.h"
#include "lib/vec.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/assert.h"
#include "lib/bitmap.h"
#include "lib/time.h"
#include "lib/arith.h"
#include "lib/errno.h"

static void test_ivec_cursor(void);
static void test_bufvec_cursor(void);
static void test_bufvec_cursor_copyto_copyfrom(void);
static void test_indexvec_varr_cursor(void);
static void test_cmp(void);

enum {
	NR  = 255,
	IT  = 6,
	NR2 = 127
};

static m0_bcount_t segs[NR * IT];

static struct m0_vec t = {
	.v_nr    = NR * IT,
	.v_count = segs
};

void test_vec(void)
{
	int          i;
	int          it;
	m0_bcount_t  count;
	m0_bcount_t  sum0;
	m0_bcount_t  sum1;
	m0_bcount_t  step;
	bool         eov;
	void       **buf;

	struct m0_vec_cursor c;
	struct m0_bufvec     bv;

	M0_ALLOC_ARR(buf, NR2);
	M0_UT_ASSERT(buf != NULL);

	for (count = 0, it = 1, sum0 = i = 0; i < ARRAY_SIZE(segs); ++i) {
		segs[i] = count * it;
		sum0 += segs[i];
		if (++count == NR) {
			count = 0;
			++it;
		}
	};

	M0_UT_ASSERT(m0_vec_count(&t) == sum0);

	m0_vec_cursor_init(&c, &t);
	for (i = 0; i < sum0; ++i) {
		eov = m0_vec_cursor_move(&c, 1);
		M0_UT_ASSERT(eov == (i == sum0 - 1));
	}

	m0_vec_cursor_init(&c, &t);
	count = 0;
	it = 1;
	sum1 = 0;
	while (!m0_vec_cursor_move(&c, 0)) {
		if (count * it != 0) {
			step = m0_vec_cursor_step(&c);
			sum1 += step;
			M0_UT_ASSERT(step == count * it);
			eov = m0_vec_cursor_move(&c, step);
			M0_UT_ASSERT(eov == (sum1 == sum0));
		}
		if (++count == NR) {
			count = 0;
			++it;
		}
	}
	m0_vec_cursor_init(&c, &t);
	m0_vec_cursor_move(&c, sum0);
	M0_UT_ASSERT(m0_vec_cursor_move(&c, 0));

	M0_UT_ASSERT(m0_bufvec_alloc(&bv, NR, M0_SEG_SIZE) == 0);
	M0_UT_ASSERT(bv.ov_vec.v_nr == NR);
	for (i = 0; i < NR; ++i) {
		M0_UT_ASSERT(bv.ov_vec.v_count[i] == M0_SEG_SIZE);
		M0_UT_ASSERT(bv.ov_buf[i] != NULL);
	}
	m0_bufvec_free(&bv);
	M0_UT_ASSERT(bv.ov_vec.v_nr == 0);
	M0_UT_ASSERT(bv.ov_buf == NULL);
	m0_bufvec_free(&bv);    /* no-op */
	m0_fi_enable_once("bufvec_alloc", "buf-alloc-fail");
	M0_UT_ASSERT(m0_bufvec_alloc(&bv, NR, M0_SEG_SIZE) == -ENOMEM);
	m0_fi_enable_once("bufvec_alloc", "empty-fail");
	M0_UT_ASSERT(m0_bufvec_empty_alloc(&bv, NR) == -ENOMEM);
	M0_UT_ASSERT(m0_bufvec_alloc(&bv, NR2, M0_SEG_SIZE) == 0);
	M0_UT_ASSERT(bv.ov_vec.v_nr == NR2);
	for (i = 0; i < NR2; ++i)
		buf[i] = bv.ov_buf[i];
	M0_UT_ASSERT(m0_bufvec_extend(&bv, NR2) == 0);
	M0_UT_ASSERT(bv.ov_vec.v_nr == 2 * NR2);
	for (i = 0; i < 2 * NR2; ++i) {
		M0_UT_ASSERT(bv.ov_vec.v_count[i] == M0_SEG_SIZE);
		M0_UT_ASSERT(bv.ov_buf[i] != NULL);
	}
	for (i = 0; i < NR2; ++i)
		M0_UT_ASSERT(bv.ov_buf[i] == buf[i]);

	m0_bufvec_free(&bv);

	M0_UT_ASSERT(m0_bufvec_alloc_aligned(&bv, NR, M0_SEG_SIZE,
					      M0_SEG_SHIFT) == 0);
	M0_UT_ASSERT(bv.ov_vec.v_nr == NR);
	for (i = 0; i < NR; ++i) {
		M0_UT_ASSERT(bv.ov_vec.v_count[i] == M0_SEG_SIZE);
		M0_UT_ASSERT(bv.ov_buf[i] != NULL);
		M0_UT_ASSERT(m0_addr_is_aligned(bv.ov_buf[i], M0_SEG_SHIFT));
	}
	m0_bufvec_free_aligned(&bv, M0_SEG_SHIFT);
	M0_UT_ASSERT(bv.ov_vec.v_nr == 0);
	M0_UT_ASSERT(bv.ov_buf == NULL);
	m0_bufvec_free_aligned(&bv, M0_SEG_SHIFT);    /* no-op */

	test_bufvec_cursor();
	test_bufvec_cursor_copyto_copyfrom();
	test_ivec_cursor();
	test_indexvec_varr_cursor();
	test_cmp();
}

static void test_indexvec_varr_cursor(void)
{
	struct m0_indexvec_varr *ivv;
	struct m0_ivec_varr_cursor ivc;
	m0_bcount_t  c;
	int          nr;
	int          rc;

	M0_ALLOC_PTR(ivv);
	M0_UT_ASSERT(ivv != NULL);

	M0_SET0(ivv);
	rc = m0_indexvec_varr_alloc(ivv, 4);
	M0_UT_ASSERT(rc == 0);

	/* data initialization begins */
	*(m0_bindex_t*)(m0_varr_ele_get(&ivv->iv_index, 0)) = 0;
	*(m0_bindex_t*)(m0_varr_ele_get(&ivv->iv_index, 1)) = 1;
	*(m0_bindex_t*)(m0_varr_ele_get(&ivv->iv_index, 2)) = 2;
	*(m0_bindex_t*)(m0_varr_ele_get(&ivv->iv_index, 3)) = 8;
	*(m0_bcount_t*)(m0_varr_ele_get(&ivv->iv_count, 0)) = 2;
	*(m0_bcount_t*)(m0_varr_ele_get(&ivv->iv_count, 1)) = 3; /*overlapping*/
	*(m0_bcount_t*)(m0_varr_ele_get(&ivv->iv_count, 2)) = 1; /*overlapping*/
	*(m0_bcount_t*)(m0_varr_ele_get(&ivv->iv_count, 3)) = 4;
	/* data initialization ends */

	m0_varr_for(&ivv->iv_count, uint64_t *, i, countp) {
		/*printf("data[%d] = %d\n", (int)i, (int)*(uint64_t*)countp);*/
	} m0_varr_endfor;
	m0_varr_for(&ivv->iv_index, uint64_t *, i, indexp) {
		/*printf("data[%d] = %d\n", (int)i, (int)*(uint64_t*)indexp);*/
	} m0_varr_endfor;

	/* test move */
	m0_ivec_varr_cursor_init(&ivc, ivv);
	M0_UT_ASSERT(ivc.vc_ivv    == ivv);
	M0_UT_ASSERT(ivc.vc_seg    == 0);
	M0_UT_ASSERT(ivc.vc_offset == 0);

	M0_UT_ASSERT( m0_ivec_varr_cursor_index(&ivc) == 0);
	M0_UT_ASSERT( m0_ivec_varr_cursor_step (&ivc) == 2);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 0) == 0);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 1) == 1);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 2) == 2);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 3) == 3);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 4) == 4);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 5) == 4);

	M0_UT_ASSERT(!m0_ivec_varr_cursor_move (&ivc, 1)  );
	M0_UT_ASSERT( m0_ivec_varr_cursor_index(&ivc) == 1);
	M0_UT_ASSERT( m0_ivec_varr_cursor_step (&ivc) == 1);

	M0_UT_ASSERT(!m0_ivec_varr_cursor_move (&ivc, 1)  );
	M0_UT_ASSERT( m0_ivec_varr_cursor_index(&ivc) == 1); /* seg[1] */
	M0_UT_ASSERT( m0_ivec_varr_cursor_step (&ivc) == 3);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 1) == 1);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 2) == 2);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 3) == 3);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 4) == 4);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 5) == 4);

	M0_UT_ASSERT(!m0_ivec_varr_cursor_move (&ivc, 2)  );
	M0_UT_ASSERT( m0_ivec_varr_cursor_index(&ivc) == 3);
	M0_UT_ASSERT( m0_ivec_varr_cursor_step (&ivc) == 1);

	M0_UT_ASSERT(!m0_ivec_varr_cursor_move (&ivc, 1)  );
	M0_UT_ASSERT( m0_ivec_varr_cursor_index(&ivc) == 2); /* seg[2] */
	M0_UT_ASSERT( m0_ivec_varr_cursor_step (&ivc) == 1);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 2) == 2);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 3) == 3);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 4) == 3);

	M0_UT_ASSERT(!m0_ivec_varr_cursor_move (&ivc, 1)  );
	M0_UT_ASSERT( m0_ivec_varr_cursor_index(&ivc) == 8); /* seg[3] */
	M0_UT_ASSERT( m0_ivec_varr_cursor_step (&ivc) == 4);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 8) == 8);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 9) == 9);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 12) == 12);
	M0_UT_ASSERT( m0_ivec_varr_cursor_conti(&ivc, 13) == 12);

	M0_UT_ASSERT( m0_ivec_varr_cursor_move (&ivc, 4)  ); /* at the end*/

	/* test move_to */
	m0_ivec_varr_cursor_init(&ivc, ivv);
	M0_UT_ASSERT(!m0_ivec_varr_cursor_move_to(&ivc, 1));
	M0_UT_ASSERT( m0_ivec_varr_cursor_index(&ivc) == 1);
	M0_UT_ASSERT( m0_ivec_varr_cursor_step (&ivc) == 1);

	M0_UT_ASSERT(!m0_ivec_varr_cursor_move_to(&ivc, 3));
	M0_UT_ASSERT( m0_ivec_varr_cursor_index(&ivc) == 3);
	M0_UT_ASSERT( m0_ivec_varr_cursor_step (&ivc) == 1);
	M0_UT_ASSERT(!m0_ivec_varr_cursor_move_to(&ivc, 10));
	M0_UT_ASSERT( m0_ivec_varr_cursor_index(&ivc) ==10);
	M0_UT_ASSERT( m0_ivec_varr_cursor_step (&ivc) == 2);
	M0_UT_ASSERT( m0_ivec_varr_cursor_move_to(&ivc, 12)); /* at the end*/

	m0_ivec_varr_cursor_init(&ivc, ivv);
	c = 0;
	nr = 0;
	while (!m0_ivec_varr_cursor_move(&ivc, c)) {
		c = m0_ivec_varr_cursor_step(&ivc);
		++nr;
	}
	M0_UT_ASSERT(nr == 4);

	m0_indexvec_varr_free(ivv);
	m0_free(ivv);
}

static void test_ivec_cursor(void)
{
	int                   nr;
	m0_bindex_t           indexes[4];
	m0_bcount_t           counts[4];
	m0_bcount_t           c;
	struct m0_indexvec    ivec;
	struct m0_ivec_cursor cur;

	indexes[0] = 0, counts[0] = 2;
	indexes[1] = 1, counts[1] = 3; /* overlapping segment */
	indexes[2] = 2, counts[2] = 1; /* overlapping segment */
	indexes[3] = 8, counts[3] = 4;

	ivec.iv_index       = indexes;
	ivec.iv_vec.v_nr    = 4;
	ivec.iv_vec.v_count = counts;

	m0_ivec_cursor_init(&cur, &ivec);
	M0_UT_ASSERT(cur.ic_cur.vc_vec    == &ivec.iv_vec);
	M0_UT_ASSERT(cur.ic_cur.vc_seg    == 0);
	M0_UT_ASSERT(cur.ic_cur.vc_offset == 0);

	M0_UT_ASSERT(m0_ivec_cursor_index(&cur) == 0);
	M0_UT_ASSERT(m0_ivec_cursor_step(&cur)  == 2);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 0) == 0);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 1) == 1);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 2) == 2);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 3) == 3);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 4) == 4);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 5) == 4);

	M0_UT_ASSERT(!m0_ivec_cursor_move(&cur, 1));
	M0_UT_ASSERT(m0_ivec_cursor_index(&cur) == 1);
	M0_UT_ASSERT(m0_ivec_cursor_step(&cur)  == 1);

	M0_UT_ASSERT(!m0_ivec_cursor_move(&cur, 1));
	M0_UT_ASSERT(m0_ivec_cursor_index(&cur) == 1); /* seg[1] */
	M0_UT_ASSERT(m0_ivec_cursor_step(&cur)  == 3);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 1) == 1);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 2) == 2);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 3) == 3);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 4) == 4);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 5) == 4);

	M0_UT_ASSERT(!m0_ivec_cursor_move(&cur, 2));
	M0_UT_ASSERT(m0_ivec_cursor_index(&cur) == 3);
	M0_UT_ASSERT(m0_ivec_cursor_step(&cur)  == 1);

	M0_UT_ASSERT(!m0_ivec_cursor_move(&cur, 1));
	M0_UT_ASSERT(m0_ivec_cursor_index(&cur) == 2); /* seg[2] */
	M0_UT_ASSERT(m0_ivec_cursor_step(&cur)  == 1);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 2) == 2);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 3) == 3);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 4) == 3);

	M0_UT_ASSERT(!m0_ivec_cursor_move(&cur, 1));
	M0_UT_ASSERT(m0_ivec_cursor_index(&cur) == 8); /* seg[3] */
	M0_UT_ASSERT(m0_ivec_cursor_step(&cur)  == 4);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 8) == 8);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 9) == 9);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 12) == 12);
	M0_UT_ASSERT(m0_ivec_cursor_conti(&cur, 13) == 12);
	M0_UT_ASSERT(m0_ivec_cursor_move(&cur, 4));

	/* test move_to */
	m0_ivec_cursor_init(&cur, &ivec);
	M0_UT_ASSERT(!m0_ivec_cursor_move_to(&cur, 1));
	M0_UT_ASSERT(m0_ivec_cursor_index(&cur) == 1);
	M0_UT_ASSERT(m0_ivec_cursor_step (&cur) == 1);

	M0_UT_ASSERT(!m0_ivec_cursor_move_to(&cur, 3));
	M0_UT_ASSERT(m0_ivec_cursor_index(&cur) == 3);
	M0_UT_ASSERT(m0_ivec_cursor_step (&cur) == 1);
	M0_UT_ASSERT(!m0_ivec_cursor_move_to(&cur, 10));
	M0_UT_ASSERT(m0_ivec_cursor_index(&cur) ==10);
	M0_UT_ASSERT(m0_ivec_cursor_step (&cur) == 2);
	M0_UT_ASSERT(m0_ivec_cursor_move_to(&cur, 12)); /* at the end*/

	m0_ivec_cursor_init(&cur, &ivec);
	c = 0;
	nr = 0;
	while (!m0_ivec_cursor_move(&cur, c)) {
		c = m0_ivec_cursor_step(&cur);
		++nr;
	}
	M0_UT_ASSERT(nr == 4);

	M0_UT_ASSERT(m0_vec_count(&ivec.iv_vec) == 10);
	M0_UT_ASSERT(m0_indexvec_pack(&ivec) == 2);
	M0_UT_ASSERT(m0_vec_count(&ivec.iv_vec) == 8);
	M0_UT_ASSERT(ivec.iv_vec.v_nr == 2);
	M0_UT_ASSERT(ivec.iv_index[0] == 0);
	M0_UT_ASSERT(ivec.iv_vec.v_count[0] == 4);
	M0_UT_ASSERT(ivec.iv_index[1] == 8);
	M0_UT_ASSERT(ivec.iv_vec.v_count[1] == 4);
	M0_UT_ASSERT(m0_indexvec_pack(&ivec) == 0);
	M0_UT_ASSERT(m0_vec_count(&ivec.iv_vec) == 8);
}

static void test_bufvec_cursor(void)
{
	/* Create buffers with different shapes but same total size.
	   Also create identical buffers for exact shape testing,
	   and a couple of larger buffers whose bounds won't be reached.
	 */
	enum { NR_BUFS = 10 };
	static struct {
		uint32_t    num_segs;
		m0_bcount_t seg_size;
	} shapes[NR_BUFS] = {
		[0] = { 1, 48 },
		[1] = { 1, 48 },
		[2] = { 2, 24 },
		[3] = { 2, 24 },
		[4] = { 3, 16 },
		[5] = { 3, 16 },
		[6] = { 4, 12 },
		[7] = { 4, 12 },
		[8] = { 6,  8 },
		[9] = { 6,  8 },
	};
	static const char *msg = "abcdefghijklmnopqrstuvwxyz0123456789"
		"ABCDEFGHIJK";
	size_t msglen = strlen(msg)+1;
	struct m0_bufvec bufs[NR_BUFS];
	struct m0_bufvec *b;
	int i;

	M0_SET_ARR0(bufs);
	for (i = 0; i < NR_BUFS; ++i) {
		M0_UT_ASSERT(msglen == shapes[i].num_segs * shapes[i].seg_size);
		M0_UT_ASSERT(m0_bufvec_alloc(&bufs[i],
					     shapes[i].num_segs,
					     shapes[i].seg_size) == 0);
	}
	b = &bufs[0];
	M0_UT_ASSERT(b->ov_vec.v_nr == 1);
	memcpy(b->ov_buf[0], msg, msglen);
	M0_UT_ASSERT(memcmp(b->ov_buf[0], msg, msglen) == 0);
	for (i = 1; i < NR_BUFS; ++i) {
		struct m0_bufvec_cursor s_cur;
		struct m0_bufvec_cursor d_cur;
		int j;
		const char *p = msg;

		m0_bufvec_cursor_init(&s_cur, &bufs[i-1]);
		m0_bufvec_cursor_init(&d_cur, &bufs[i]);
		M0_UT_ASSERT(m0_bufvec_cursor_copy(&d_cur, &s_cur, msglen)
			     == msglen);

		/* verify cursor positions */
		M0_UT_ASSERT(m0_bufvec_cursor_move(&s_cur,0));
		M0_UT_ASSERT(m0_bufvec_cursor_move(&d_cur,0));

		/* verify data */
		for (j = 0; j < bufs[i].ov_vec.v_nr; ++j) {
			int k;
			char *q;
			for (k = 0; k < bufs[i].ov_vec.v_count[j]; ++k) {
				q = bufs[i].ov_buf[j] + k;
				M0_UT_ASSERT(*p++ == *q);
			}
		}
	}

	/* bounded copy - dest buffer smaller */
	{
		struct m0_bufvec buf;
		m0_bcount_t seg_size = shapes[NR_BUFS-1].seg_size - 1;
		uint32_t    num_segs = shapes[NR_BUFS-1].num_segs - 1;
		m0_bcount_t buflen = seg_size * num_segs;
		struct m0_bufvec_cursor s_cur;
		struct m0_bufvec_cursor d_cur;
		int j;
		const char *p = msg;
		int len;

		M0_UT_ASSERT(m0_bufvec_alloc(&buf, num_segs, seg_size) == 0);
		M0_UT_ASSERT(buflen < msglen);

		m0_bufvec_cursor_init(&s_cur, &bufs[NR_BUFS-1]);
		m0_bufvec_cursor_init(&d_cur, &buf);

		M0_UT_ASSERT(m0_bufvec_cursor_copy(&d_cur, &s_cur, msglen)
			     == buflen);

		/* verify cursor positions */
		M0_UT_ASSERT(!m0_bufvec_cursor_move(&s_cur,0));
		M0_UT_ASSERT(m0_bufvec_cursor_move(&d_cur,0));

		/* check partial copy correct */
		len = 0;
		for (j = 0; j < buf.ov_vec.v_nr; ++j) {
			int k;
			char *q;
			for (k = 0; k < buf.ov_vec.v_count[j]; ++k) {
				q = buf.ov_buf[j] + k;
				M0_UT_ASSERT(*p++ == *q);
				len++;
			}
		}
		M0_UT_ASSERT(len == buflen);
		m0_bufvec_free(&buf);
	}

	/* bounded copy - source buffer smaller */
	{
		struct m0_bufvec buf;
		m0_bcount_t seg_size = shapes[NR_BUFS-1].seg_size + 1;
		uint32_t    num_segs = shapes[NR_BUFS-1].num_segs + 1;
		m0_bcount_t buflen = seg_size * num_segs;
		struct m0_bufvec_cursor s_cur;
		struct m0_bufvec_cursor d_cur;
		int j;
		const char *p = msg;
		int len;

		M0_UT_ASSERT(m0_bufvec_alloc(&buf, num_segs, seg_size) == 0);
		M0_UT_ASSERT(buflen > msglen);

		m0_bufvec_cursor_init(&s_cur, &bufs[NR_BUFS-1]);
		m0_bufvec_cursor_init(&d_cur, &buf);

		M0_UT_ASSERT(m0_bufvec_cursor_copy(&d_cur, &s_cur, buflen)
			     == msglen);

		/* verify cursor positions */
		M0_UT_ASSERT(m0_bufvec_cursor_move(&s_cur,0));
		M0_UT_ASSERT(!m0_bufvec_cursor_move(&d_cur,0));

		/* check partial copy correct */
		len = 0;
		for (j = 0; j < buf.ov_vec.v_nr; ++j) {
			int k;
			char *q;
			for (k = 0; k < buf.ov_vec.v_count[j] && len < msglen;
			     k++) {
				q = buf.ov_buf[j] + k;
				M0_UT_ASSERT(*p++ == *q);
				len++;
			}
		}
		m0_bufvec_free(&buf);
	}

	/* free buffer pool */
	for (i = 0; i < ARRAY_SIZE(bufs); ++i)
		m0_bufvec_free(&bufs[i]);
}

static void test_bufvec_cursor_copyto_copyfrom(void)
{
	struct struct_int {
		uint32_t int1;
		uint32_t int2;
		uint32_t int3;
	};

	struct struct_char {
		char char1;
		char char2;
		char char3;
	};

	static const struct struct_int struct_int1 = {
		.int1 = 111,
		.int2 = 112,
		.int3 = 113
	};

	static const struct struct_char struct_char1 = {
		.char1 = 'L',
		.char2 = 'M',
		.char3 = 'N'
	};

	void                    *area1;
	struct m0_bufvec         bv1;
	struct m0_bufvec_cursor  dcur1;
	struct struct_int       *struct_int2 = NULL;
	struct struct_char      *struct_char2 = NULL;
	struct struct_int        struct_int3;
	struct struct_char       struct_char3;
	m0_bcount_t              nbytes;
	m0_bcount_t              nbytes_copied;
	uint32_t                 i;
	int                      rc;

	/* Prepare for the destination buffer and the cursor. */
	nbytes = sizeof struct_int1 * 4 + sizeof struct_char1 * 4 + 1;

	area1 = m0_alloc(nbytes);
	M0_UT_ASSERT(area1 != NULL);

	bv1 = (struct m0_bufvec) M0_BUFVEC_INIT_BUF(&area1, &nbytes);
	m0_bufvec_cursor_init(&dcur1, &bv1);

	M0_UT_ASSERT(m0_bufvec_cursor_step(&dcur1) == nbytes);

	/*
	 * Copy data to the destination cursor using the API
	 * m0_bufvec_cursor_copyto().
	 */
	for (i = 0; i < 4; ++i) {
		/* Copy struct_int1 to the bufvec. */
		nbytes_copied = m0_bufvec_cursor_copyto(&dcur1,
							(void *)&struct_int1,
							sizeof struct_int1);
		M0_UT_ASSERT(nbytes_copied == sizeof struct_int1);

		/* Copy struct_char1 to the bufvec. */
		nbytes_copied = m0_bufvec_cursor_copyto(&dcur1,
							(void *)&struct_char1,
							 sizeof struct_char1);
		M0_UT_ASSERT(nbytes_copied == sizeof struct_char1);
	}

	/* Rewind the cursor. */
	m0_bufvec_cursor_init(&dcur1, &bv1);
	M0_UT_ASSERT(m0_bufvec_cursor_step(&dcur1) == nbytes);

	/* Verify data from the destination cursor. */
	for (i = 0; i < 3; ++i) {
		/* Read data into struct_int2 and verify it. */
		struct_int2 = m0_bufvec_cursor_addr(&dcur1);
		M0_UT_ASSERT(struct_int2 != NULL);

		M0_UT_ASSERT(struct_int2->int1 == struct_int1.int1);
		M0_UT_ASSERT(struct_int2->int2 == struct_int1.int2);
		M0_UT_ASSERT(struct_int2->int3 == struct_int1.int3);

		rc = m0_bufvec_cursor_move(&dcur1, sizeof *struct_int2);
		M0_UT_ASSERT(rc == 0);

		/* Read data into struct_char2 and verify it. */
		struct_char2 = m0_bufvec_cursor_addr(&dcur1);
		M0_UT_ASSERT(struct_char2 != NULL);

		M0_UT_ASSERT(struct_char2->char1 == struct_char1.char1);
		M0_UT_ASSERT(struct_char2->char2 == struct_char1.char2);
		M0_UT_ASSERT(struct_char2->char3 == struct_char1.char3);

		rc = m0_bufvec_cursor_move(&dcur1, sizeof *struct_char2);
		M0_UT_ASSERT(rc == 0);
	}

	/* Rewind the cursor. */
	m0_bufvec_cursor_init(&dcur1, &bv1);
	M0_UT_ASSERT(m0_bufvec_cursor_step(&dcur1) == nbytes);

	/*
	 * Copy data from the cursor using the API m0_bufvec_cursor_copyfrom().
	 */

	for (i = 0; i < 3; ++i) {
		/* Copy data from dcur into the struct_int3 and verify it. */
		nbytes_copied = m0_bufvec_cursor_copyfrom(&dcur1, &struct_int3,
							  sizeof struct_int3);
		M0_UT_ASSERT(nbytes_copied == sizeof struct_int3);

		M0_UT_ASSERT(struct_int3.int2 == struct_int1.int2);
		M0_UT_ASSERT(struct_int3.int2 == struct_int1.int2);
		M0_UT_ASSERT(struct_int3.int3 == struct_int1.int3);

		/* Copy data from dcur into the struct_char3 and verify it. */
		nbytes_copied = m0_bufvec_cursor_copyfrom(&dcur1, &struct_char3,
							  sizeof struct_char3);
		M0_UT_ASSERT(nbytes_copied == sizeof struct_char3);

		M0_UT_ASSERT(struct_char3.char1 == struct_char1.char1);
		M0_UT_ASSERT(struct_char3.char2 == struct_char1.char2);
		M0_UT_ASSERT(struct_char3.char3 == struct_char1.char3);
	}
}

static struct m0_bufvec *split(const char *src, int n)
{
	struct m0_bitmap  map;
	int               result;
	struct m0_bufvec *vec;
	int               i;
	int               len;
	int               pos;
	int               off;
	int               tot  = strlen(src) + 1 + n - 1;
	static uint64_t   seed = 0;

	M0_UT_ASSERT(n > 0);
	if (seed == 0)
		seed = m0_time_now();
	result = m0_bitmap_init(&map, tot + 1);
	M0_UT_ASSERT(result == 0);
	m0_bitmap_set(&map, tot, true);
	for (i = 0; i < n - 1; ++i) {
		int p;

		do {
			p = m0_rnd(tot, &seed);
		} while (m0_bitmap_get(&map, p));
		m0_bitmap_set(&map, p, true);
	}
	M0_ALLOC_PTR(vec);
	M0_UT_ASSERT(vec != NULL);
	M0_ALLOC_ARR(vec->ov_vec.v_count, n);
	M0_UT_ASSERT(vec->ov_vec.v_count != NULL);
	M0_ALLOC_ARR(vec->ov_buf, n);
	M0_UT_ASSERT(vec->ov_buf != NULL);
	vec->ov_vec.v_nr = n;
	for (i = 0, len = 0, pos = 0, off = 0; i <= tot; ++i, ++len) {
		if (m0_bitmap_get(&map, i)) {
			M0_ALLOC_ARR(vec->ov_buf[pos], len);
			M0_UT_ASSERT(vec->ov_buf[pos] != NULL);
			vec->ov_vec.v_count[pos] = len;
			memcpy(vec->ov_buf[pos], src + off, len);
			off += len;
			pos++;
			len = -1;
			continue;
		}
	}
	m0_bitmap_fini(&map);
	return vec;
}

static const char abc[] = "vextcwmflyjabszingqurdkoph";
static const char aBc[] = "vextcwmflyjabszingQurdkoph";

static void test_cmp(void)
{
	int n;
	int i;

	for (n = 1; n < 15; ++n) {
		for (i = 0; i < 1000; ++i) {
			struct m0_bufvec *v0 = split(abc, n);
			struct m0_bufvec *v1 = split(abc, 2 * n);
			struct m0_bufvec_cursor c0;
			struct m0_bufvec_cursor c1;
			m0_bcount_t             nob;

			M0_UT_ASSERT(m0_vec_count(&v0->ov_vec) == sizeof abc);
			M0_UT_ASSERT(m0_vec_count(&v1->ov_vec) == sizeof abc);
			m0_bufvec_cursor_init(&c0, v0);
			m0_bufvec_cursor_init(&c1, v1);
			M0_UT_ASSERT(m0_bufvec_cursor_cmp(&c0, &c1) == 0);
			m0_bufvec_cursor_init(&c0, v0);
			m0_bufvec_cursor_init(&c1, v1);
			M0_UT_ASSERT(m0_bufvec_cursor_cmp(&c1, &c0) == 0);
			m0_bufvec_cursor_init(&c0, v0);
			m0_bufvec_cursor_init(&c1, v1);
			M0_UT_ASSERT(m0_bufvec_cursor_cmp(&c0, &c0) == 0);
			m0_bufvec_cursor_init(&c0, v0);
			m0_bufvec_cursor_init(&c1, v1);
			M0_UT_ASSERT(m0_bufvec_cursor_cmp(&c1, &c1) == 0);
			m0_bufvec_free(v1);
			v1 = split(aBc, 2 * n);
			m0_bufvec_cursor_init(&c0, v0);
			m0_bufvec_cursor_init(&c1, v1);
			M0_UT_ASSERT(m0_bufvec_cursor_cmp(&c0, &c1) > 0);
			m0_bufvec_cursor_init(&c0, v0);
			m0_bufvec_cursor_init(&c1, v1);
			M0_UT_ASSERT(m0_bufvec_cursor_cmp(&c1, &c0) < 0);
			m0_bufvec_cursor_init(&c0, v0);
			m0_bufvec_cursor_init(&c1, v1);
			M0_UT_ASSERT(m0_bufvec_cursor_prefix(&c1, &c0) == 18);
			m0_bufvec_cursor_init(&c0, v0);
			m0_bufvec_cursor_init(&c1, v1);
			nob = m0_bufvec_cursor_copy(&c1, &c0, sizeof abc - 1);
			M0_UT_ASSERT(nob == sizeof abc - 1);
			M0_UT_ASSERT(m0_bufvec_cursor_cmp(&c0, &c1) == 0);
			m0_bufvec_cursor_init(&c0, v0);
			m0_bufvec_cursor_init(&c1, v1);
			nob = m0_bufvec_cursor_copy(&c1, &c0, sizeof abc);
			M0_UT_ASSERT(nob == sizeof abc);
			M0_UT_ASSERT(m0_bufvec_cursor_cmp(&c0, &c1) == 0);
			m0_bufvec_cursor_init(&c0, v0);
			m0_bufvec_cursor_init(&c1, v1);
			nob = m0_bufvec_cursor_copy(&c1, &c0, sizeof abc + 1);
			M0_UT_ASSERT(nob == sizeof abc);
			M0_UT_ASSERT(m0_bufvec_cursor_cmp(&c0, &c1) == 0);
			m0_bufvec_free(v0);
			m0_bufvec_free(v1);
		}
	}
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
