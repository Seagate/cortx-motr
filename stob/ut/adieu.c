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


#include <stdlib.h>    /* system */
#include <stdio.h>     /* fopen, fgetc, ... */
#include <unistd.h>    /* unlink */
#include <sys/stat.h>  /* mkdir */
#include <sys/types.h> /* mkdir */

#include "lib/misc.h"    /* M0_SET0 */
#include "lib/memory.h"  /* m0_alloc_align */
#include "lib/errno.h"
#include "lib/finject.h" /* M0_FI_ENABLED */
#include "lib/ub.h"
#include "ut/stob.h"
#include "ut/ut.h"
#include "lib/assert.h"
#include "lib/arith.h"
#include "stob/domain.h"
#include "stob/io.h"
#include "stob/stob.h"
#include "fol/fol.h"
#include "balloc/balloc.h" /* M0_BALLOC_NON_SPARE_ZONE */

/**
   @addtogroup stob
   @{
 */

#define AD_ADIEU_CS_SZ 16

enum {
	NR    = 4,
	NR_SORT = 256,
	MIN_BUF_SIZE = 4096,
	MIN_BUF_SIZE_IN_BLOCKS = 4,
};

enum {
	M0_STOB_UT_DOMAIN_KEY = 0x01,
	M0_STOB_UT_STOB_KEY   = 0x02,
};

/** @todo move vars to a context */
static const char linux_location[] = "linuxstob:./__s";
static const char perf_location[] = "perfstob:./__s";
static struct m0_stob_domain *dom;
static struct m0_stob *obj;
static const char linux_path[] = "./__s/o/100000000000000:2";
static const char perf_path[] = "./__s/backstore/o/100000000000000:2";
static struct m0_stob_io io;
static m0_bcount_t user_vec[NR];
static char *user_buf[NR];
static char *user_cksm_buf[NR];
static char *read_buf[NR];
static char *read_cksm_buf[NR];
static char *user_bufs[NR];
static char *read_bufs[NR];
static m0_bindex_t stob_vec[NR];
static struct m0_clink clink;
static FILE *f;
static uint32_t block_shift;
static uint32_t buf_size;

static int test_adieu_init(const char *location,
			   const char *dom_cfg,
			   const char *stob_cfg)
{
	int               i;
	int               rc;
	struct m0_stob_id stob_id;
	char   cs_char = 'a';

	rc = m0_stob_domain_create(location,
				   NULL, M0_STOB_UT_DOMAIN_KEY, dom_cfg, &dom);
	M0_ASSERT(rc == 0);
	M0_ASSERT(dom != NULL);

	m0_stob_id_make(0, M0_STOB_UT_STOB_KEY, &dom->sd_id, &stob_id);
	rc = m0_stob_find(&stob_id, &obj);
	M0_ASSERT(rc == 0);
	rc = m0_stob_locate(obj);
	M0_ASSERT(rc == 0);
	rc = m0_ut_stob_create(obj, stob_cfg, NULL);
	M0_ASSERT(rc == 0);

	block_shift = m0_stob_block_shift(obj);
	/* buf_size is chosen so it would be at least MIN_BUF_SIZE in bytes
	 * or it would consist of at least MIN_BUF_SIZE_IN_BLOCKS blocks */
	buf_size = max_check(MIN_BUF_SIZE,
			     (1 << block_shift) * MIN_BUF_SIZE_IN_BLOCKS);

	for (i = 0; i < ARRAY_SIZE(user_buf); ++i) {
		user_buf[i] = m0_alloc_aligned(buf_size, block_shift);
		M0_ASSERT(user_buf[i] != NULL);
	}

	// Allocate contigious buffer for i/p checksums
	user_cksm_buf[0] = m0_alloc(AD_ADIEU_CS_SZ * ARRAY_SIZE(user_cksm_buf));
	M0_ASSERT(user_cksm_buf[0] != NULL);
	memset( user_cksm_buf[0], cs_char++, AD_ADIEU_CS_SZ);	
	for (i = 1; i < ARRAY_SIZE(user_cksm_buf); ++i) {
		user_cksm_buf[i] = user_cksm_buf[i-1] + AD_ADIEU_CS_SZ; 	
		memset( user_cksm_buf[i], cs_char++, AD_ADIEU_CS_SZ);
	}
	
	for (i = 0; i < ARRAY_SIZE(read_buf); ++i) {
		read_buf[i] = m0_alloc_aligned(buf_size, block_shift);
		M0_ASSERT(read_buf[i] != NULL);
	}

	// Allocate contigious buffer for o/p checksums 
	read_cksm_buf[0] = m0_alloc(AD_ADIEU_CS_SZ * ARRAY_SIZE(read_cksm_buf));
	M0_ASSERT(read_cksm_buf[0] != NULL);
	memset( read_cksm_buf[0], 0, AD_ADIEU_CS_SZ);	
	for (i = 1; i < ARRAY_SIZE(read_cksm_buf); ++i) {
		read_cksm_buf[i] = read_cksm_buf[i-1] + AD_ADIEU_CS_SZ; 	
		memset( read_cksm_buf[i], 0, AD_ADIEU_CS_SZ);
	}

	for (i = 0; i < NR; ++i) {
		user_bufs[i] = m0_stob_addr_pack(user_buf[i], block_shift);
		read_bufs[i] = m0_stob_addr_pack(read_buf[i], block_shift);
		user_vec[i] = buf_size >> block_shift;
		stob_vec[i] = (buf_size * (2 * i + 1)) >> block_shift;
		memset(user_buf[i], ('a' + i)|1, buf_size);
	}
	return rc;
}

static void test_adieu_fini(void)
{
	int i;
	int rc;

	rc = m0_stob_destroy(obj, NULL);
	M0_ASSERT(rc == 0);
	rc = m0_stob_domain_destroy(dom);
	M0_ASSERT(rc == 0);

	for (i = 0; i < ARRAY_SIZE(user_buf); ++i)
		m0_free(user_buf[i]);

	m0_free(user_cksm_buf[0]);

	for (i = 0; i < ARRAY_SIZE(read_buf); ++i)
		m0_free(read_buf[i]);

	m0_free(read_cksm_buf[0]);
}

static void test_write(int i)
{
	int		    rc;
	struct m0_fol_frag *fol_frag;

	M0_ALLOC_PTR(fol_frag);
	M0_UB_ASSERT(fol_frag != NULL);

	m0_stob_io_init(&io);

	io.si_opcode = SIO_WRITE;
	io.si_flags  = 0;
	io.si_fol_frag = fol_frag;
	io.si_user.ov_vec.v_nr = i;
	io.si_user.ov_vec.v_count = user_vec;
	io.si_user.ov_buf = (void **)user_bufs;

	io.si_stob.iv_vec.v_nr = i;
	io.si_stob.iv_vec.v_count = user_vec;
	io.si_stob.iv_index = stob_vec;

	io.si_unit_sz  = (buf_size >> block_shift);
	io.si_cksum_sz = AD_ADIEU_CS_SZ;
	// Checksum for i buf_size blocks
	io.si_cksum.b_addr = user_cksm_buf[0];
	io.si_cksum.b_nob  = ( i * AD_ADIEU_CS_SZ );

	m0_clink_init(&clink, NULL);
	m0_clink_add_lock(&io.si_wait, &clink);

	rc = m0_stob_io_prepare_and_launch(&io, obj, NULL, NULL);
	M0_ASSERT(rc == 0);

	m0_chan_wait(&clink);

	M0_ASSERT(io.si_rc == 0);
	M0_ASSERT(io.si_count == (buf_size * i) >> block_shift);

	m0_clink_del_lock(&clink);
	m0_clink_fini(&clink);

	m0_stob_io_fini(&io);
}

static void test_read(int i)
{
	int rc;

	m0_stob_io_init(&io);

	io.si_opcode = SIO_READ;
	io.si_flags  = 0;
	io.si_user.ov_vec.v_nr = i;
	io.si_user.ov_vec.v_count = user_vec;
	io.si_user.ov_buf = (void **)read_bufs;

	io.si_stob.iv_vec.v_nr = i;
	io.si_stob.iv_vec.v_count = user_vec;
	io.si_stob.iv_index = stob_vec;

	io.si_unit_sz  = (buf_size >> block_shift);
	io.si_cksum_sz = AD_ADIEU_CS_SZ;
	// Checksum for i buf_size blocks
	io.si_cksum.b_addr = read_cksm_buf[0];
	io.si_cksum.b_nob  = ( i * AD_ADIEU_CS_SZ );

	m0_clink_init(&clink, NULL);
	m0_clink_add_lock(&io.si_wait, &clink);

	rc = m0_stob_io_prepare_and_launch(&io, obj, NULL, NULL);
	M0_ASSERT(rc == 0);

	m0_chan_wait(&clink);

	M0_ASSERT(io.si_rc == 0);
	M0_ASSERT(io.si_count == (buf_size * i) >> block_shift);

	m0_clink_del_lock(&clink);
	m0_clink_fini(&clink);

	m0_stob_io_fini(&io);
}

/**
   Adieu unit-test.
 */
static void test_adieu(const char *path)
{
	int ch;
	int i;
	int j;

	for (i = 1; i < NR; ++i) {
		test_write(i);

		/* this works only for linuxstob */
		f = fopen(path, "r");
		for (j = 0; j < i; ++j) {
			int k;

			for (k = 0; k < buf_size; ++k) {
				ch = fgetc(f);
				M0_ASSERT(ch == '\0');
				M0_ASSERT(!feof(f));
			}
			for (k = 0; k < buf_size; ++k) {
				ch = fgetc(f);
				M0_ASSERT(ch != '\0');
				M0_ASSERT(!feof(f));
			}
		}
		ch = fgetc(f);
		M0_ASSERT(ch == EOF);
		fclose(f);
	}

	for (i = 1; i < NR; ++i) {
		test_read(i);
		M0_ASSERT(memcmp(user_buf[i - 1], read_buf[i - 1], buf_size) == 0);
		// TODO: Check how this can be enabled for linux stob
		// M0_ASSERT(memcmp(user_cksm_buf[i - 1], read_cksm_buf[i - 1], AD_ADIEU_CS_SZ) == 0);
	}
}

void m0_stob_ut_adieu_linux(void)
{
	int rc;

	rc = test_adieu_init(linux_location, NULL, NULL);
	M0_ASSERT(rc == 0);
	test_adieu(linux_path);
	test_adieu_fini();
}

void m0_stob_ut_adieu_perf(void)
{
	int rc;

	rc = test_adieu_init(perf_location, NULL, NULL);
	M0_ASSERT(rc == 0);
	test_adieu(perf_path);
	test_adieu_fini();
}

/*
   Adieu unit-benchmark
 */

static void ub_write(int i)
{
	test_write(NR - 1);
}

static void ub_read(int i)
{
	test_read(NR - 1);
}

static m0_bcount_t  user_vec1[NR_SORT];
static char        *user_bufs1[NR_SORT];
static m0_bindex_t  stob_vec1[NR_SORT];

static void ub_iovec_init()
{
	int i;

	for (i = 0; i < NR_SORT ; i++)
		stob_vec1[i] = MIN_BUF_SIZE * i;

	m0_stob_io_init(&io);

	io.si_opcode              = SIO_WRITE;
	io.si_flags               = 0;

	io.si_user.ov_vec.v_nr    = NR_SORT;
	io.si_user.ov_vec.v_count = user_vec1;
	io.si_user.ov_buf         = (void **)user_bufs1;

	io.si_stob.iv_vec.v_nr    = NR_SORT;
	io.si_stob.iv_vec.v_count = user_vec1;
	io.si_stob.iv_index       = stob_vec1;
}

static void ub_iovec_invert()
{
	int  i;
	bool swapped;

	/* Reverse sort index vecs. */
	do {
		swapped = false;
		for (i = 0; i < NR_SORT - 1; i++) {
			if (stob_vec1[i] < stob_vec1[i + 1]) {
				m0_bindex_t tmp  = stob_vec1[i];
				stob_vec1[i]     = stob_vec1[i + 1];
				stob_vec1[i + 1] = tmp;
				swapped          = true;
			}
		}
	} while(swapped);
}

static void ub_iovec_sort()
{
	m0_stob_iovec_sort(&io);
}

static void ub_iovec_sort_invert()
{
	ub_iovec_invert();
	m0_stob_iovec_sort(&io);
}

static int ub_init(const char *opts M0_UNUSED)
{
	return test_adieu_init(linux_location, NULL, NULL);
}

static void ub_fini(void)
{
	test_adieu_fini();
}

enum {
	UB_ITER = 100,
	UB_ITER_SORT = 100000
};

struct m0_ub_set m0_adieu_ub = {
	.us_name = "adieu-ub",
	.us_init = ub_init,
	.us_fini = ub_fini,
	.us_run  = {
		{ .ub_name = "write-prime",
		  .ub_iter = 1,
		  .ub_round = ub_write,
		  .ub_block_size = MIN_BUF_SIZE,
		  .ub_blocks_per_op = MIN_BUF_SIZE_IN_BLOCKS },

		{ .ub_name = "write",
		  .ub_iter = UB_ITER,
		  .ub_block_size = MIN_BUF_SIZE,
		  .ub_blocks_per_op = MIN_BUF_SIZE_IN_BLOCKS,
		  .ub_round = ub_write },

		{ .ub_name = "read",
		  .ub_iter = UB_ITER,
		  .ub_block_size = MIN_BUF_SIZE,
		  .ub_blocks_per_op = MIN_BUF_SIZE_IN_BLOCKS,
		  .ub_round = ub_read },

		{ .ub_name = "iovec-sort",
		  .ub_iter = UB_ITER_SORT,
		  .ub_init = ub_iovec_init,
		  .ub_block_size = MIN_BUF_SIZE,
		  .ub_blocks_per_op = MIN_BUF_SIZE_IN_BLOCKS,
		  .ub_round = ub_iovec_sort },

		{ .ub_name = "iovec-sort-invert",
		  .ub_iter = UB_ITER_SORT,
		  .ub_init = ub_iovec_init,
		  .ub_block_size = MIN_BUF_SIZE,
		  .ub_blocks_per_op = MIN_BUF_SIZE_IN_BLOCKS,
		  .ub_round = ub_iovec_sort_invert },

		{ .ub_name = NULL }
	}
};

/** @} end group stob */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
