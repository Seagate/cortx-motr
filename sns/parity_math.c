/*
 * Copyright (c) 2013-2021 Seagate Technology LLC and/or its Affiliates
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


#include "lib/arith.h"
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/misc.h" /* SET0() */
#include "lib/types.h"

#include "sns/parity_ops.h"
#include "sns/parity_math.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNS
#include "lib/trace.h"

#define BUF_ALLOC_ERR_INFO(ret, str, len) ({			\
	M0_ERR_INFO(ret, "Failed to allocate memory for "	\
		    str " buffer of length = %u",		\
		    (uint32_t)len);				\
})

#define VALUE_ASSERT_INFO(cond, var) ({				\
	M0_ASSERT_INFO(cond, "Invalid "#var" value. "		\
		       #var" = %u.", (uint32_t)var);		\
})

#define VALUE_MISMATCH_ASSERT_INFO(lhs, rhs) ({			\
	M0_ASSERT_INFO(lhs == rhs, #lhs" Value mismatch. "	\
		       #lhs" = %u, "#rhs"=%u",			\
		       (uint32_t)lhs, (uint32_t)rhs);		\
})

#define BLOCK_SIZE_ASSERT_INFO(blksz, index, block) ({		\
	VALUE_MISMATCH_ASSERT_INFO(blksz, block[index].b_nob);	\
})

#define MAT_INIT_ERR_INFO(ret, str, row, col) ({		\
	M0_ERR_INFO(ret, "Failed to initialize %ux%u "		\
	 	    str " matrix.", (uint32_t)row,		\
		    (uint32_t)col);				\
})

#define MATVEC_INIT_ERR_INFO(ret, str, size) ({			\
	M0_ERR_INFO(ret, "Failed to initialize " str " matrix "	\
		    "vector of size=%u", (uint32_t)size);	\
})

/* Forward declarations. */
static void xor_calculate(struct m0_parity_math *math,
			  const struct m0_buf *data,
			  struct m0_buf *parity);

static int xor_diff(struct m0_parity_math *math,
		    struct m0_buf         *old,
		    struct m0_buf         *new,
		    struct m0_buf         *parity,
		    uint32_t               index);

static int xor_recover(struct m0_parity_math *math,
		       struct m0_buf *data,
		       struct m0_buf *parity,
		       struct m0_buf *fails,
		       enum m0_parity_linsys_algo algo);

static void fail_idx_xor_recover(struct m0_parity_math *math,
				 struct m0_buf *data,
				 struct m0_buf *parity,
				 const uint32_t failure_index);

/* Below are some functions which have separate implementations for user
 * space and kernel space. Intel ISA library is used in user space only. Hence
 * implementation of these functions in kernel space are empty for now and will
 * be removed once kernel space compilation is removed.
 */

/**
 * This function initializes fields required to use Reed Solomon algorithm.
 */
static int reed_solomon_init(struct m0_parity_math *math);

/**
 * This function clears fields used by Reed Solomon algorithm.
 */
static void reed_solomon_fini(struct m0_parity_math *math);

/**
 * This function calculates parity fields using Reed Solomon algorithm.
 */
static void reed_solomon_encode(struct m0_parity_math *math,
				const struct m0_buf *data,
				struct m0_buf *parity);

/**
 * This function calculates differential parity using Reed Solomon algorithm.
 */
static int reed_solomon_diff(struct m0_parity_math *math,
			     struct m0_buf         *old,
			     struct m0_buf         *new,
			     struct m0_buf         *parity,
			     uint32_t               index);

/**
 * This function recovers failed data and/or parity using Reed Solomon
 * algorithm.
 */
static int reed_solomon_recover(struct m0_parity_math *math,
				struct m0_buf *data,
				struct m0_buf *parity,
				struct m0_buf *fails,
				enum m0_parity_linsys_algo algo);

/**
 * Recovers data or parity units partially or fully depending on the parity
 * calculation algorithm, given the failure index.
 */
static void fail_idx_reed_solomon_recover(struct m0_parity_math *math,
					  struct m0_buf *data,
					  struct m0_buf *parity,
					  const uint32_t failure_index);

/**
 * Initialize fields, specific to Reed Solomon implementation, which are
 * required for incremental recovery.
 * @param[in]  math    - Pointer to parity math structure.
 * @param[out] ir      - Pointer to incremental recovery structure.
 */
static void ir_rs_init(const struct m0_parity_math *math, struct m0_sns_ir *ir);

/**
 * This function registers failed index.
 * @param[in, out] ir           - pointer to incremental recovery structure.
 * @param[in]      failed_index - index of the failed block in a parity group.
 */
static void ir_failure_register(struct m0_sns_ir *ir,
				uint32_t failed_index);

/**
 * Computes data-recovery matrix. Populates dependency bitmaps for failed
 * blocks.
 * @param[in, out] ir       - pointer to incremental recovery structure.
 * @retval         0          on success.
 * @retval         -ENOMEM    on failure to acquire memory.
 * @retval         -EDOM      when input matrix is singular.
 */
static int ir_mat_compute(struct m0_sns_ir *ir);

/**
 * Core routine to recover failed block using current alive block.
 * @param[in] ir           - Pointer to incremental recovery structure.
 * @param[in] alive_block  - Pointer to the alive block.
 * @retval    0            - success otherwise failure
 */
static int ir_recover(struct m0_sns_ir *ir, struct m0_sns_ir_block *alive_block);

/**
 * Returns last usable block index.
 */
static uint32_t last_usable_block_id(const struct m0_sns_ir *ir,
				     uint32_t block_idx);

static bool parity_math_invariant(const struct m0_parity_math *math);

/* Counts number of failed blocks. */
static uint32_t fails_count(uint8_t *fail, uint32_t unit_count);

static inline uint32_t ir_blocks_count(const struct m0_sns_ir *ir);

/**
 * Initialize si_block structures.
 * @param  ir  - pointer to incremental recovery structure.
 */
static int ir_si_blocks_init(struct m0_sns_ir *ir);

/**
 * Constant times x plus y, over galois field. Naming convention for this
 * function is borrowed from BLAS routines.
 */
static void gfaxpy(struct m0_bufvec *y, struct m0_bufvec *x,
		   m0_parity_elem_t alpha);

static bool is_usable(const struct m0_sns_ir *ir,
		      const struct m0_bitmap *in_bmap,
		      struct m0_sns_ir_block *failed_block);

static inline bool is_valid_block_idx(const  struct m0_sns_ir *ir,
				      uint32_t block_idx);

#if !defined(__KERNEL__) && defined(HAVE_ISA_L_H)
/**
 * This is wrapper function for Intel ISA API ec_encode_data_update().
 * @param[out] dest_bufs - Array of coded output buffers i.e. struct m0_buf
 * @param[in]  src_buf   - Pointer to single input source (struct m0_buf) used
 *                         to update output parity.
 * @param[in]  vec_idx   - The vector index corresponding to the single
 *                         input source.
 * @param[in]  g_tbls    - Pointer to array of input tables generated from
 *                         coding coefficients in ec_init_tables().
 *                         Must be of size 32*data_nr*dest_nr
 * @param[in]  data_nr   - The number of data blocks for coding.
 * @param[in]  dest_nr   - The number of output blocks to concurrently
 *                         encode/decode.
 * @retval     0           on success.
 * @retval     -ENOMEM     on failure to acquire memory.
 */
static int isal_encode_data_update(struct m0_buf *dest_bufs, struct m0_buf *src_buf,
				   uint32_t vec_idx, uint8_t *g_tbls,
				   uint32_t data_nr, uint32_t dest_nr);

/**
 * Sorts the indices for failed and non-failed data and parity blocks.
 * @param[in,out]  rs            - Pointer to Reed Solomon structure.
 * @param[in]      fail          - block with flags, if element is '1' then data
 *                                 or parity block with given index is treated
 *                                 as broken.
 * @param[in]      total_count   - Total count of buffers i.e. data_count + parity_count
 * @param[in]      parity_count  - Count of SNS parity units used in system.
 */
static void fails_sort(struct m0_reed_solomon *rs, uint8_t *fail,
		       uint32_t total_count, uint32_t parity_count);

/**
 * Sorts the data and parity buffers based on input fail buffer. If buffer is
 * marked as failed, its pointer will be added in rs_bufs_out buffer array. If
 * buffer is not marked as failed in fail buffer, its pointer will be added in
 * rs_bufs_in buffer array. Buffer array rs_bufs_in will be used as source
 * buffers for recovery. Buffer array rs_bufs_out will be used as buffers to
 * be recovered.
 * @param[in,out]  rs          - Pointer to Reed Solomon structure.
 * @param[in]      total_count - Total count of buffers i.e. data_count + parity_count
 * @param[in]      data_count  - count of SNS data units used in system.
 * @param[in]      fail        - block with flags, treated as uint8_t block with
 *                               b_nob elements, if element is '1' then data or parity
 *                               block with given index is treated as broken.
 * @param[in]      data        - data block, treated as uint8_t block with
 *                               b_nob elements.
 * @param[in]      parity      - parity block, treated as uint8_t block with
 *                               b_nob elements.
 */
static void buf_sort(struct m0_reed_solomon *rs, uint32_t total_count,
		     uint32_t data_count, uint8_t *fail,
		     struct m0_buf *data, struct m0_buf *parity);

/**
 * Inverts the encoding matrix and generates tables of recovery coefficient
 * codes for lost data.
 * @param[in]      data_count     - count of SNS data units used in system.
 * @param[in]      parity_count   - count of SNS parity units used in system.
 * @param[in,out]  rs             - Pointer to reed solomon structure.
 * @retval         0              - success otherwise failure
 */
static int isal_gen_recov_coeff_tbl(uint32_t data_count, uint32_t parity_count,
				    struct m0_reed_solomon *rs);

/**
 * Recovery of each failed block depends upon subset of alive blocks.
 * This routine prepares a bitmap indicating this dependency. If a bit at
 * location 'x' is set 'true' then it implies that f_block has no dependency
 * on block with index 'x'.
 */
static void dependency_bitmap_prepare(struct m0_sns_ir_block *f_block,
				      struct m0_sns_ir *ir);
#endif /* !defined(__KERNEL__) && defined(HAVE_ISA_L_H) */

static void (*calculate[M0_PARITY_CAL_ALGO_NR])(struct m0_parity_math *math,
						const struct m0_buf *data,
						struct m0_buf *parity) = {
	[M0_PARITY_CAL_ALGO_XOR] = xor_calculate,
	[M0_PARITY_CAL_ALGO_REED_SOLOMON] = reed_solomon_encode,
};

static int (*diff[M0_PARITY_CAL_ALGO_NR])(struct m0_parity_math *math,
					  struct m0_buf         *old,
					  struct m0_buf         *new,
					  struct m0_buf         *parity,
					   uint32_t               index) = {
	[M0_PARITY_CAL_ALGO_XOR]          = xor_diff,
	[M0_PARITY_CAL_ALGO_REED_SOLOMON] = reed_solomon_diff,
};

static int (*recover[M0_PARITY_CAL_ALGO_NR])(struct m0_parity_math *math,
					     struct m0_buf *data,
					     struct m0_buf *parity,
					     struct m0_buf *fails,
					     enum m0_parity_linsys_algo algo) = {
	[M0_PARITY_CAL_ALGO_XOR] = xor_recover,
	[M0_PARITY_CAL_ALGO_REED_SOLOMON] = reed_solomon_recover,
};

static void (*fidx_recover[M0_PARITY_CAL_ALGO_NR])(struct m0_parity_math *math,
						   struct m0_buf *data,
						   struct m0_buf *parity,
						   const uint32_t fidx) = {
	[M0_PARITY_CAL_ALGO_XOR] = fail_idx_xor_recover,
	[M0_PARITY_CAL_ALGO_REED_SOLOMON] = fail_idx_reed_solomon_recover,
};

enum {
	SNS_PARITY_MATH_DATA_BLOCKS_MAX = 1 << (M0_PARITY_GALOIS_W - 1),
	BAD_FAIL_INDEX = -1,
	IR_INVALID_COL = UINT8_MAX,
	MIN_TABLE_LEN = 32,
};

/* Parity Math Functions. */

M0_INTERNAL void m0_parity_math_fini(struct m0_parity_math *math)
{
	M0_ENTRY();
	if (math->pmi_parity_algo == M0_PARITY_CAL_ALGO_REED_SOLOMON)
		reed_solomon_fini(math);
	M0_LEAVE();
}

M0_INTERNAL int m0_parity_math_init(struct m0_parity_math *math,
				    uint32_t data_count, uint32_t parity_count)
{
	int ret = 0;

	M0_ENTRY("data_count=%u parity_count=%u", data_count, parity_count);

	M0_PRE(math != NULL);

	M0_SET0(math);
	math->pmi_data_count   = data_count;
	math->pmi_parity_count = parity_count;

	M0_PRE(parity_math_invariant(math));

	if (parity_count == 1)
		math->pmi_parity_algo = M0_PARITY_CAL_ALGO_XOR;
	else {
		math->pmi_parity_algo = M0_PARITY_CAL_ALGO_REED_SOLOMON;
		ret = reed_solomon_init(math);
		if (ret != 0) {
			m0_parity_math_fini(math);
			return M0_ERR(ret);
		}
	}

	return M0_RC(ret);
}

M0_INTERNAL void m0_parity_math_calculate(struct m0_parity_math *math,
					  struct m0_buf *data,
					  struct m0_buf *parity)
{
	M0_ENTRY();
	(*calculate[math->pmi_parity_algo])(math, data, parity);
	M0_LEAVE();
}

M0_INTERNAL int  m0_parity_math_diff(struct m0_parity_math *math,
				     struct m0_buf *old,
				     struct m0_buf *new,
				     struct m0_buf *parity, uint32_t index)
{
	int rc;

	M0_ENTRY();
	rc = (*diff[math->pmi_parity_algo])(math, old, new, parity, index);
	return rc == 0 ? M0_RC(rc) : M0_ERR(rc);
}

M0_INTERNAL int m0_parity_math_recover(struct m0_parity_math *math,
				       struct m0_buf *data,
				       struct m0_buf *parity,
				       struct m0_buf *fails,
				       enum m0_parity_linsys_algo algo)
{
	int rc;

	M0_ENTRY();
	rc = (*recover[math->pmi_parity_algo])(math, data, parity, fails, algo);
	return rc == 0 ? M0_RC(rc) : M0_ERR(rc);
}

M0_INTERNAL void m0_parity_math_fail_index_recover(struct m0_parity_math *math,
						   struct m0_buf *data,
						   struct m0_buf *parity,
						   const uint32_t fidx)
{
	M0_ENTRY();
	(*fidx_recover[math->pmi_parity_algo])(math, data, parity, fidx);
	M0_LEAVE();
}

M0_INTERNAL void m0_parity_math_refine(struct m0_parity_math *math,
				       struct m0_buf *data,
				       struct m0_buf *parity,
				       uint32_t data_ind_changed)
{
	/* for simplicity: */
	m0_parity_math_calculate(math, data, parity);
}

M0_INTERNAL void m0_parity_math_buffer_xor(struct m0_buf *dest,
					   const struct m0_buf *src)
{
        uint32_t ei; /* block element index. */

        for (ei = 0; ei < src[0].b_nob; ++ei)
		((uint8_t*)dest[0].b_addr)[ei] ^=
			(m0_parity_elem_t)((uint8_t*)src[0].b_addr)[ei];
}

M0_INTERNAL int m0_sns_ir_init(const struct m0_parity_math *math,
			       uint32_t local_nr, struct m0_sns_ir *ir)
{
	int ret;

	M0_ENTRY("math=%p, local_nr=%u, ir=%p", math, local_nr, ir);

	M0_PRE(math != NULL);
	M0_PRE(ir != NULL);

	M0_SET0(ir);

	ir->si_data_nr   = math->pmi_data_count;
	ir->si_parity_nr = math->pmi_parity_count;
	ir->si_local_nr  = local_nr;
	ir->si_alive_nr  = ir_blocks_count(ir);

	ret = ir_si_blocks_init(ir);
	if (ret != 0) {
		m0_sns_ir_fini(ir);
		return M0_ERR_INFO(ret, "Failed to initialize si_blocks");
	}

	ir_rs_init(math, ir);

	return M0_RC(ret);
}

M0_INTERNAL int m0_sns_ir_failure_register(struct m0_bufvec *recov_addr,
					   uint32_t failed_index,
					   struct m0_sns_ir *ir)
{
	struct m0_sns_ir_block *block;

	M0_ENTRY("recov_addr=%p, failed_index=%u, ir=%p",
		 recov_addr, failed_index, ir);

	M0_PRE(ir != NULL);
	M0_PRE(ir->si_blocks != NULL);
	M0_PRE(recov_addr != NULL);
	M0_PRE(is_valid_block_idx(ir, failed_index));

	block = &ir->si_blocks[failed_index];
	M0_PRE(block->sib_status != M0_SI_BLOCK_FAILED);

	block->sib_addr   = recov_addr;
	block->sib_status = M0_SI_BLOCK_FAILED;

	M0_CNT_DEC(ir->si_alive_nr);
	if (ir->si_alive_nr < ir->si_data_nr)
		return M0_ERR(-ERANGE);

	ir_failure_register(ir, failed_index);

	return M0_RC(0);
}

M0_INTERNAL int m0_sns_ir_mat_compute(struct m0_sns_ir *ir)
{
	M0_PRE(ir != NULL);
	return M0_RC(ir_mat_compute(ir));
}

M0_INTERNAL void m0_sns_ir_fini(struct m0_sns_ir *ir)
{
	uint32_t i;

	M0_ENTRY("ir=%p", ir);
	M0_PRE(ir != NULL);

	for (i = 0; i < ir_blocks_count(ir); ++i)
		if (ir->si_blocks[i].sib_bitmap.b_words != NULL)
			m0_bitmap_fini(&ir->si_blocks[i].sib_bitmap);

	m0_free(ir->si_blocks);
	M0_LEAVE();
}

M0_INTERNAL int m0_sns_ir_recover(struct m0_sns_ir *ir,
				  struct m0_bufvec *bufvec,
				  const struct m0_bitmap *bitmap,
				  uint32_t failed_index,
				  enum m0_sns_ir_block_type block_type)
{
	int                     block_idx = 0;
	size_t                  b_set_nr;
	struct m0_sns_ir_block *blocks;
	struct m0_sns_ir_block *alive_block;
	int                     ret;

	M0_ENTRY("ir=%p, bufvec=%p, bitmap=%p, failed_index=%u, block_type=%u",
		 ir, bufvec, bitmap, failed_index, block_type);

	M0_PRE(ir != NULL && bufvec != NULL && bitmap != NULL);

	b_set_nr = m0_bitmap_set_nr(bitmap);
	M0_PRE(b_set_nr > 0);
	M0_PRE(ergo(b_set_nr > 1, block_type == M0_SI_BLOCK_REMOTE));
	M0_PRE(ergo(block_type == M0_SI_BLOCK_REMOTE,
		    is_valid_block_idx(ir, failed_index)));
	M0_PRE(ergo(block_type == M0_SI_BLOCK_REMOTE,
	            ir->si_blocks[failed_index].sib_status ==
		    M0_SI_BLOCK_FAILED));
	M0_PRE(ergo(block_type == M0_SI_BLOCK_LOCAL, ir->si_local_nr != 0));

	if (b_set_nr == 1)
		block_idx = m0_bitmap_ffs(bitmap);

	VALUE_ASSERT_INFO(is_valid_block_idx(ir, block_idx), block_idx);

	blocks = ir->si_blocks;

	switch (block_type) {
	/* Input block is assumed to be an untransformed block, and is used for
	 * recovering all failed blocks */
	case M0_SI_BLOCK_LOCAL:

		M0_CNT_DEC(ir->si_local_nr);
		alive_block = &blocks[block_idx];
		alive_block->sib_addr = bufvec;

		ret = ir_recover(ir, alive_block);
		if (ret != 0)
			return 	M0_ERR_INFO(ret, "Incremental recovery failed.");
		break;

	/* Input block is assumed to be a transformed block, and is
	 * cumulatively added to a block with index failed_index. */
	case M0_SI_BLOCK_REMOTE:
		if (!is_usable(ir, (struct m0_bitmap*) bitmap,
			       &blocks[failed_index]))
			break;
		gfaxpy(blocks[failed_index].sib_addr, bufvec,
		       1);
		break;
	}

	return M0_RC(0);
}

/* Parity Math Helper Functions */

/* Functions m0_parity_* are too much eclectic. Just more simple names. */
static int gadd(int x, int y)
{
	return m0_parity_add(x, y);
}

static int gmul(int x, int y)
{
	return m0_parity_mul(x, y);
}

static uint32_t fails_count(uint8_t *fail, uint32_t unit_count)
{
	uint32_t x;
	uint32_t count = 0;

	for (x = 0; x < unit_count; ++x)
		count += !!fail[x];

	return count;
}

static bool parity_math_invariant(const struct m0_parity_math *math)
{
	return  _0C(math != NULL) && _0C(math->pmi_data_count >= 1) &&
		_0C(math->pmi_parity_count >= 1) &&
		_0C(math->pmi_data_count >= math->pmi_parity_count) &&
		_0C(math->pmi_data_count <= SNS_PARITY_MATH_DATA_BLOCKS_MAX);
}

static void xor_calculate(struct m0_parity_math *math,
			  const struct m0_buf *data,
			  struct m0_buf *parity)
{
	uint32_t          ei; /* block element index. */
	uint32_t          ui; /* unit index. */
	uint32_t          block_size = data[0].b_nob;
	m0_parity_elem_t  pe;

	M0_ENTRY();
	M0_PRE(block_size == parity[0].b_nob);
	for (ui = 1; ui < math->pmi_data_count; ++ui)
		M0_PRE(block_size == data[ui].b_nob);

	for (ei = 0; ei < block_size; ++ei) {
		pe = 0;
		for (ui = 0; ui < math->pmi_data_count; ++ui)
			pe ^= (m0_parity_elem_t)((uint8_t*)data[ui].b_addr)[ei];

		((uint8_t*)parity[0].b_addr)[ei] = pe;
	}
	M0_LEAVE();
}

static int xor_diff(struct m0_parity_math *math,
		    struct m0_buf         *old,
		    struct m0_buf         *new,
		    struct m0_buf         *parity,
		    uint32_t               index)
{
	uint32_t ei;

	M0_PRE(math   != NULL);
	M0_PRE(old    != NULL);
	M0_PRE(new    != NULL);
	M0_PRE(parity != NULL);
	M0_PRE(index  <  math->pmi_data_count);
	M0_PRE(old[index].b_nob == new[index].b_nob);
	M0_PRE(new[index].b_nob == parity[0].b_nob);

	for (ei = 0; ei < new[index].b_nob; ++ei) {
		((uint8_t*)parity[0].b_addr)[ei] ^=
			((uint8_t *)old[index].b_addr)[ei] ^
			((uint8_t *)new[index].b_addr)[ei];
	}

	return M0_RC(0);
}

static int xor_recover(struct m0_parity_math *math,
		       struct m0_buf *data,
		       struct m0_buf *parity,
		       struct m0_buf *fails,
		       enum m0_parity_linsys_algo algo)
{
	uint32_t          ei; /* block element index. */
	uint32_t          ui; /* unit index. */
	uint8_t          *fail;
	uint32_t          fail_count;
	uint32_t          unit_count;
	uint32_t          block_size = data[0].b_nob;
	m0_parity_elem_t  pe;
	int               fail_index = BAD_FAIL_INDEX;

	unit_count = math->pmi_data_count + math->pmi_parity_count;
	fail = (uint8_t*) fails->b_addr;
	fail_count = fails_count(fail, unit_count);

	M0_PRE(fail_count == 1);
	M0_PRE(fail_count <= math->pmi_parity_count);
	M0_PRE(block_size == parity[0].b_nob);

	for (ui = 1; ui < math->pmi_data_count; ++ui)
		M0_PRE(block_size == data[ui].b_nob);

	for (ei = 0; ei < block_size; ++ei) {
		pe = 0;
		for (ui = 0; ui < math->pmi_data_count; ++ui) {
			if (fail[ui] != 1)
				pe ^= (m0_parity_elem_t)((uint8_t*)
				       data[ui].b_addr)[ei];
			else
				fail_index = ui;
		}
		/* Now ui points to parity block, test if it was failed. */
		if (fail[ui] != 1) {
			M0_ASSERT(fail_index != BAD_FAIL_INDEX);
			((uint8_t*)data[fail_index].b_addr)[ei] = pe ^
				((uint8_t*)parity[0].b_addr)[ei];
		} else /* Parity was lost, so recover it. */
			((uint8_t*)parity[0].b_addr)[ei] = pe;
	}
	return M0_RC(0);
}

static void fail_idx_xor_recover(struct m0_parity_math *math,
				 struct m0_buf *data,
				 struct m0_buf *parity,
				 const uint32_t failure_index)
{
	uint32_t          ei; /* block element index. */
	uint32_t          ui; /* unit index. */
	uint32_t          unit_count;
	uint32_t          block_size = data[0].b_nob;
	m0_parity_elem_t  pe;

	M0_PRE(block_size == parity[0].b_nob);

	unit_count = math->pmi_data_count + math->pmi_parity_count;
	M0_ASSERT(failure_index < unit_count);

	for (ui = 1; ui < math->pmi_data_count; ++ui)
		M0_ASSERT(block_size == data[ui].b_nob);

	for (ei = 0; ei < block_size; ++ei) {
		pe = 0;
		for (ui = 0; ui < math->pmi_data_count; ++ui)
			if (ui != failure_index)
				pe ^= (m0_parity_elem_t)((uint8_t*)
				       data[ui].b_addr)[ei];

		if (ui != failure_index)
			((uint8_t*)data[failure_index].b_addr)[ei] = pe ^
				((uint8_t*)parity[0].b_addr)[ei];
		else /* Parity was lost, so recover it. */
			((uint8_t*)parity[0].b_addr)[ei] = pe;
	}
}

/** @todo Iterative reed-solomon decode to be implemented. */
static void fail_idx_reed_solomon_recover(struct m0_parity_math *math,
					  struct m0_buf *data,
					  struct m0_buf *parity,
					  const uint32_t failure_index)
{
}

static int ir_si_blocks_init(struct m0_sns_ir *ir)
{
	uint32_t i;
	uint32_t blocks_count;
	int      ret = 0;

	M0_PRE(ir != NULL);

	blocks_count = ir_blocks_count(ir);
	M0_ALLOC_ARR(ir->si_blocks, blocks_count);
	if (ir->si_blocks == NULL)
		return BUF_ALLOC_ERR_INFO(-ENOMEM, "ir blocks", blocks_count);

	for (i = 0; i < blocks_count; ++i) {
		ir->si_blocks[i].sib_idx = i;
		ir->si_blocks[i].sib_status = M0_SI_BLOCK_ALIVE;
		ret = m0_bitmap_init(&ir->si_blocks[i].sib_bitmap,
				     blocks_count);
		if (ret != 0)
			return M0_ERR_INFO(ret, "Failed to initialize bitmap "
					  "for si_blocks with index = %u", i);
	}

	return ret;
}

static inline uint32_t ir_blocks_count(const struct m0_sns_ir *ir)
{
	return ir->si_data_nr + ir->si_parity_nr;
}

static void gfaxpy(struct m0_bufvec *y, struct m0_bufvec *x,
		   m0_parity_elem_t alpha)
{
	uint32_t                i;
	uint32_t                seg_size;
	uint8_t                *y_addr;
	uint8_t                *x_addr;
	m0_bcount_t             step;
	struct m0_bufvec_cursor x_cursor;
	struct m0_bufvec_cursor y_cursor;

	M0_ENTRY("y=%p, x=%p, alpha=%u", y, x, (uint32_t)alpha);

	M0_PRE(y != NULL && x != NULL);
	M0_PRE(y->ov_vec.v_nr != 0);
	M0_PRE(y->ov_vec.v_nr == x->ov_vec.v_nr);
	M0_PRE(y->ov_vec.v_count[0] != 0);
	M0_PRE(y->ov_vec.v_count[0] == x->ov_vec.v_count[0]);
	seg_size = y->ov_vec.v_count[0];
	M0_PRE(m0_forall(i, y->ov_vec.v_nr, y->ov_vec.v_count[i] == seg_size));

	m0_bufvec_cursor_init(&y_cursor, y);
	m0_bufvec_cursor_init(&x_cursor, x);
	do {
		x_addr  = m0_bufvec_cursor_addr(&x_cursor);
		y_addr  = m0_bufvec_cursor_addr(&y_cursor);

		switch (alpha) {
		/* This is a special case of the 'default' case that follows.
		 * Here we avoid unnecessary call to gmul.*/
		case 1:
			for (i = 0; i <seg_size; ++i)
				y_addr[i] = gadd(y_addr[i], x_addr[i]);
			break;
		default:
			for (i = 0; i < seg_size; ++i)
				y_addr[i] = gadd(y_addr[i], gmul(x_addr[i],
							         alpha));
			break;
		}
		step = m0_bufvec_cursor_step(&y_cursor);
	} while (!m0_bufvec_cursor_move(&x_cursor, step) &&
		 !m0_bufvec_cursor_move(&y_cursor, step));

	M0_LEAVE();
}

static bool is_usable(const struct m0_sns_ir *ir,
		      const struct m0_bitmap *in_bmap,
		      struct m0_sns_ir_block *failed_block)
{
	size_t   i;
	uint32_t last_usable_bid;

	M0_PRE(in_bmap != NULL && failed_block != NULL && ir != NULL);
	M0_PRE(in_bmap->b_nr == failed_block->sib_bitmap.b_nr);

	last_usable_bid = last_usable_block_id(ir, failed_block->sib_idx);
	if (last_usable_bid == ir_blocks_count(ir))
		return false;
	for (i = 0; i <= last_usable_bid; ++i) {
		if (m0_bitmap_get(in_bmap, i) &&
		    !m0_bitmap_get(&failed_block->sib_bitmap, i))
			return false;
	}
	return true;
}

static inline bool is_valid_block_idx(const struct m0_sns_ir *ir,
				      uint32_t block_idx)
{
	return block_idx < ir_blocks_count(ir);
}

#if !defined(__KERNEL__) && defined(HAVE_ISA_L_H)
static void reed_solomon_fini(struct m0_parity_math *math)
{
	M0_ENTRY();
	m0_free(math->pmi_rs.rs_encode_matrix);
	m0_free(math->pmi_rs.rs_encode_tbls);
	m0_free(math->pmi_rs.rs_decode_tbls);
	m0_free(math->pmi_rs.rs_alive_idx);
	m0_free(math->pmi_rs.rs_failed_idx);
	m0_free(math->pmi_rs.rs_bufs_in);
	m0_free(math->pmi_rs.rs_bufs_out);
	M0_LEAVE();
}

static int reed_solomon_init(struct m0_parity_math *math)
{
	struct m0_reed_solomon *rs;
	uint32_t                total_count;
	uint32_t                tbl_len;
	int                     ret = 0;

	M0_ENTRY("math=%p", math);

	M0_PRE(parity_math_invariant(math));

	rs = &math->pmi_rs;
	total_count = math->pmi_data_count + math->pmi_parity_count;
	/* The encode and decode tables are the expanded tables needed for
	 * fast encode or decode for erasure codes on blocks of data.  32bytes
	 * are generated for each input coefficient. Hence total table length
	 * required is 32 * data_count * parity_count.
	 */
	tbl_len = math->pmi_data_count * math->pmi_parity_count * MIN_TABLE_LEN;

	M0_ALLOC_ARR(rs->rs_encode_matrix, (total_count * math->pmi_data_count));
	if (rs->rs_encode_matrix == NULL)
		return BUF_ALLOC_ERR_INFO(-ENOMEM, "encode matrix",
					  (total_count * math->pmi_data_count));

	M0_ALLOC_ARR(rs->rs_encode_tbls, tbl_len);
	if (rs->rs_encode_tbls == NULL)
		return BUF_ALLOC_ERR_INFO(-ENOMEM, "encode coefficient tables",
					  tbl_len);

	M0_ALLOC_ARR(rs->rs_decode_tbls, tbl_len);
	if (rs->rs_decode_tbls == NULL)
		return BUF_ALLOC_ERR_INFO(-ENOMEM, "decode coefficient tables",
					  tbl_len);

	/* Generate a matrix of coefficients to be used for encoding.
	 * The matrix generated by gf_gen_cauchy1_matrix is always invertable.
	 */
	gf_gen_cauchy1_matrix(rs->rs_encode_matrix, total_count,
			      math->pmi_data_count);

	/* Initialize tables for fast Erasure Code encode. */
	ec_init_tables(math->pmi_data_count, math->pmi_parity_count,
		       &rs->rs_encode_matrix[math->pmi_data_count *
					     math->pmi_data_count],
		       rs->rs_encode_tbls);

	M0_ALLOC_ARR(rs->rs_failed_idx, math->pmi_parity_count);
	if (rs->rs_failed_idx == NULL)
		return BUF_ALLOC_ERR_INFO(-ENOMEM, "failed index",
					  math->pmi_parity_count);

	M0_ALLOC_ARR(rs->rs_alive_idx, total_count);
	if (rs->rs_alive_idx == NULL)
		return BUF_ALLOC_ERR_INFO(-ENOMEM, "alive index",
					  total_count);

	M0_ALLOC_ARR(rs->rs_bufs_in, math->pmi_data_count);
	if (rs->rs_bufs_in == NULL)
		return BUF_ALLOC_ERR_INFO(-ENOMEM, "input buffers array",
					  math->pmi_data_count);

	M0_ALLOC_ARR(rs->rs_bufs_out, math->pmi_parity_count);
	if (rs->rs_bufs_out == NULL)
		return BUF_ALLOC_ERR_INFO(-ENOMEM, "output buffers array",
					  math->pmi_parity_count);

	return M0_RC(ret);
}

static void reed_solomon_encode(struct m0_parity_math *math,
				const struct m0_buf *data,
				struct m0_buf *parity)
{
	uint32_t                i;
	uint32_t                block_size;
	struct m0_reed_solomon *rs;

	M0_ENTRY("math=%p, data=%p, parity=%p", math, data, parity);

	M0_PRE(parity_math_invariant(math));
	M0_PRE(data != NULL);
	M0_PRE(parity != NULL);

	rs = &math->pmi_rs;
	block_size = data[0].b_nob;

	rs->rs_bufs_in[0] = (uint8_t *)data[0].b_addr;
	for (i = 1; i < math->pmi_data_count; ++i) {
		BLOCK_SIZE_ASSERT_INFO(block_size, i, data);
		rs->rs_bufs_in[i] = (uint8_t *)data[i].b_addr;
	}

	for (i = 0; i < math->pmi_parity_count; ++i) {
		BLOCK_SIZE_ASSERT_INFO(block_size, i, parity);
		rs->rs_bufs_out[i] = (uint8_t *)parity[i].b_addr;
	}

	/* Generate erasure codes on given blocks of data. */
	ec_encode_data(block_size, math->pmi_data_count,
		       math->pmi_parity_count, rs->rs_encode_tbls,
		       rs->rs_bufs_in, rs->rs_bufs_out);

	M0_LEAVE();
}

static int reed_solomon_diff(struct m0_parity_math *math,
			     struct m0_buf         *old,
			     struct m0_buf         *new,
			     struct m0_buf         *parity,
			     uint32_t               index)
{
	struct m0_buf  diff_data_buf;
	uint8_t       *diff_data_arr = NULL;
	uint32_t       block_size;
	uint32_t       alignment = sizeof(uint_fast32_t);
	uint32_t       i;
	int            ret = 0;

	M0_ENTRY("math=%p, old=%p, new=%p, parity=%p, index=%u",
		 math, old, new, parity, index);

	M0_PRE(parity_math_invariant(math));
	M0_PRE(old    != NULL);
	M0_PRE(new    != NULL);
	M0_PRE(parity != NULL);
	M0_PRE(index  <  math->pmi_data_count);

	if (old[index].b_nob != new[index].b_nob)
		return M0_ERR_INFO(-EINVAL, "Data block size mismatch. "
				   "Index=%u, Old data block size=%u, "
				   "New data block size=%u", index,
				   (uint32_t)old[index].b_nob,
				   (uint32_t)new[index].b_nob);

	block_size = new[index].b_nob;

	/* It is assumed that the buffer size will always be a multiple of
	 * 8-bytes (especially since block size currently is 4K) and this assert
	 * is added with the hope that any deviation from this assumption is
	 * caught during development instead of on the field. */
	M0_ASSERT_INFO(m0_is_aligned(block_size, alignment),
		       "block_size=%u is not %u-bytes aligned",
		       block_size, alignment);

	M0_ALLOC_ARR(diff_data_arr, block_size);
	if (diff_data_arr == NULL)
		return BUF_ALLOC_ERR_INFO(-ENOMEM, "differential data block",
					  block_size);

	m0_buf_init(&diff_data_buf, diff_data_arr, block_size);

	/* Calculate the differential data using old and new data blocks. */
	for (i = 0; i < (block_size / alignment); i++)
		((uint_fast32_t *)diff_data_arr)[i] =
			((uint_fast32_t *)old[index].b_addr)[i] ^
			((uint_fast32_t *)new[index].b_addr)[i];

	/* Update differential parity using differential data. */
	ret = isal_encode_data_update(parity, &diff_data_buf, index,
				      math->pmi_rs.rs_encode_tbls,
				      math->pmi_data_count,
				      math->pmi_parity_count);

	m0_free(diff_data_arr);
	return M0_RC(ret);
}

static int reed_solomon_recover(struct m0_parity_math *math,
				struct m0_buf *data,
				struct m0_buf *parity,
				struct m0_buf *fails,
				enum m0_parity_linsys_algo algo)
{
	uint32_t                fail_count;
	uint32_t                total_count;
	uint32_t                block_size;
	uint32_t                i;
	uint8_t                *fail = NULL;
	int                     ret;
	struct m0_reed_solomon *rs;

	M0_ENTRY("math=%p, data=%p, parity=%p, fails=%p",
		 math, data, parity, fails);

	M0_PRE(parity_math_invariant(math));
	M0_PRE(data != NULL);
	M0_PRE(parity != NULL);
	M0_PRE(fails != NULL);

	total_count = math->pmi_data_count + math->pmi_parity_count;
	block_size = data[0].b_nob;
	rs = &math->pmi_rs;

	fail = (uint8_t*) fails->b_addr;
	fail_count = fails_count(fail, total_count);

	M0_ASSERT(fail_count > 0);
	M0_ASSERT(fail_count <= math->pmi_parity_count);

	/* Validate block size for data buffers. */
	for (i = 1; i < math->pmi_data_count; ++i)
		BLOCK_SIZE_ASSERT_INFO(block_size, i, data);

	/* Validate block size for parity buffers. */
	for (i = 0; i < math->pmi_parity_count; ++i)
		BLOCK_SIZE_ASSERT_INFO(block_size, i, parity);

	/* Sort buffers which are to be recovered. */
	buf_sort(rs, total_count, math->pmi_data_count,
		 fail, data, parity);

	/* Sort failed buffer indices. */
	fails_sort(rs, fail, total_count, math->pmi_parity_count);

	VALUE_MISMATCH_ASSERT_INFO(fail_count, rs->rs_failed_nr);

	/* Get encoding coefficient tables. */
	ret = isal_gen_recov_coeff_tbl(math->pmi_data_count,
				       math->pmi_parity_count, rs);
	if (ret != 0)
		return M0_ERR_INFO(ret, "failed to generate recovery "
				   "coefficient tables");

	/* Recover failed data. */
	ec_encode_data(block_size, math->pmi_data_count,
		       fail_count, rs->rs_decode_tbls,
		       rs->rs_bufs_in, rs->rs_bufs_out);

	return M0_RC(ret);
}

static void ir_rs_init(const struct m0_parity_math *math, struct m0_sns_ir *ir)
{
	M0_ENTRY("math=%p, ir=%p", math, ir);

	M0_PRE(math != NULL);
	M0_PRE(ir != NULL);

	ir->si_rs.rs_encode_matrix = math->pmi_rs.rs_encode_matrix;
	ir->si_rs.rs_decode_tbls = math->pmi_rs.rs_decode_tbls;
	ir->si_rs.rs_failed_idx = math->pmi_rs.rs_failed_idx;
	ir->si_rs.rs_alive_idx = math->pmi_rs.rs_alive_idx;

	M0_LEAVE();
}

static void ir_failure_register(struct m0_sns_ir *ir, uint32_t failed_index)
{
	M0_ENTRY("ir=%p, failed_index=%u", ir, failed_index);

	M0_PRE(ir != NULL);
	M0_PRE(ir->si_rs.rs_failed_idx != NULL);
	M0_PRE(ir->si_rs.rs_failed_nr < ir->si_parity_nr);

	ir->si_rs.rs_failed_idx[ir->si_rs.rs_failed_nr++] = failed_index;

	M0_LEAVE();
}

static int ir_mat_compute(struct m0_sns_ir *ir)
{
	struct m0_sns_ir_block *blocks;
	struct m0_reed_solomon *rs;
	uint32_t                i;
	uint32_t                alive_nr;
	uint32_t                total_blocks_nr;
	int                     ret;

	M0_ENTRY("ir=%p", ir);
	M0_PRE(ir != NULL);

	blocks = ir->si_blocks;
	rs = &ir->si_rs;
	total_blocks_nr = ir_blocks_count(ir);

	for (i = 0, alive_nr = 0; i < total_blocks_nr; i++)
		if (blocks[i].sib_status == M0_SI_BLOCK_ALIVE)
			rs->rs_alive_idx[alive_nr++] = blocks[i].sib_idx;

	VALUE_MISMATCH_ASSERT_INFO(alive_nr, ir->si_alive_nr);

	ret = isal_gen_recov_coeff_tbl(ir->si_data_nr, ir->si_parity_nr, rs);
	if (ret != 0)
		return M0_ERR_INFO(ret, "failed to generate decode matrix");

	for (i = 0; i < rs->rs_failed_nr; i++)
		dependency_bitmap_prepare(&blocks[rs->rs_failed_idx[i]], ir);

	return M0_RC(ret);
}

static int ir_recover(struct m0_sns_ir *ir, struct m0_sns_ir_block *alive_block)
{
	struct m0_reed_solomon  *rs;
	struct m0_bitmap        *failed_bitmap;
	struct m0_bufvec        *alive_bufvec;
	struct m0_bufvec        *failed_bufvec;
	struct m0_buf            in_buf = M0_BUF_INIT0;
	struct m0_buf           *out_bufs;
	uint8_t                  curr_idx = UINT8_MAX;
	uint32_t                 i;
	uint32_t                 length;
	int                      ret = 0;

	M0_ENTRY("ir=%p, alive_block=%p", ir, alive_block);

	rs = &ir->si_rs;

	/* Check if given alive block is dependecy of any failed block. */
	for (i = 0; i < rs->rs_failed_nr; i++) {
		failed_bitmap = &ir->si_blocks[rs->rs_failed_idx[i]].sib_bitmap;
		if (m0_bitmap_get(failed_bitmap, alive_block->sib_idx) == 0)
			return M0_RC(ret);
	}

	alive_bufvec = alive_block->sib_addr;
	length = (uint32_t)m0_vec_count(&alive_bufvec->ov_vec);

	M0_ALLOC_ARR(out_bufs, rs->rs_failed_nr);
	if (out_bufs == NULL)
		return BUF_ALLOC_ERR_INFO(-ENOMEM, "output bufs",
					  rs->rs_failed_nr);

	for (i = 0; i < rs->rs_failed_nr; i++) {
		ret = m0_buf_alloc(&out_bufs[i], length);
		if (ret != 0) {
			ret = BUF_ALLOC_ERR_INFO(ret, "OUT", length);
			goto exit;
		}

		/* Get data from failed vectors in target blocks. */
		failed_bufvec = ir->si_blocks[rs->rs_failed_idx[i]].sib_addr;
		ret = m0_bufvec_to_buf_copy(&out_bufs[i], failed_bufvec);
		if (ret != 0) {
			ret = M0_ERR_INFO(ret, "Failed to copy data from "
					  "si_blocks[%u].sib_addr to "
					  "out_bufs[%u].",
					  rs->rs_failed_idx[i], i);
			goto exit;
		}
	}

	for (i = 0; i < ir->si_alive_nr; i++) {
		if(rs->rs_alive_idx[i] == alive_block->sib_idx) {
			curr_idx = i;
			break;
		}
	}

	if (curr_idx == UINT8_MAX) {
		ret = M0_ERR_INFO(-EINVAL, "Failed to find alive block "
				  "index %d in alive index array",
				   alive_block->sib_idx);
		goto exit;
	}

	/* Allocate buffer for input block. */
	ret = m0_buf_alloc(&in_buf, length);
	if (ret != 0) {
		ret = BUF_ALLOC_ERR_INFO(ret, "IN", length);
		goto exit;
	}

	/* Get data from current alive vector in input buffer. */
	ret = m0_bufvec_to_buf_copy(&in_buf, alive_bufvec);
	if (ret != 0) {
		ret = M0_ERR_INFO(ret, "Failed to copy data from "
				  "alive_bufvec to source buffer.");
		goto exit;
	}

	/* Recover the data using input buffer and its index. */
	ret = isal_encode_data_update(out_bufs, &in_buf, curr_idx,
				      rs->rs_decode_tbls, ir->si_data_nr,
				      rs->rs_failed_nr);
	if (ret != 0) {
		ret = M0_ERR_INFO(ret, "Failed to recover the data.");
		goto exit;
	}

	/* Copy recovered data back to failed vectors. */
	for (i = 0; i < rs->rs_failed_nr; i++) {
		failed_bufvec = ir->si_blocks[rs->rs_failed_idx[i]].sib_addr;
		ret = m0_buf_to_bufvec_copy(failed_bufvec, &out_bufs[i]);
		if (ret != 0){
			ret = M0_ERR_INFO(ret, "Failed to copy data from "
					  "out_bufs[%u] to "
					  "si_blocks[%u].sib_addr.", i,
					  rs->rs_failed_idx[i]);
			goto exit;
		}
	}

exit:
	m0_buf_free(&in_buf);
	for (i = 0; i < rs->rs_failed_nr; i++)
		m0_buf_free(&out_bufs[i]);
	m0_free(out_bufs);

	return M0_RC(ret);
}

static int isal_encode_data_update(struct m0_buf *dest_bufs, struct m0_buf *src_buf,
				   uint32_t vec_idx, uint8_t *g_tbls,
				   uint32_t data_nr, uint32_t dest_nr)
{
	uint32_t   i;
	uint32_t   block_size;
	uint8_t  **dest_frags;

	M0_ENTRY("dest_bufs=%p, src_buf=%p, vec_idx=%u, "
		 "g_tbls=%p, data_nr=%u, dest_nr=%u",
		 dest_bufs, src_buf, vec_idx, g_tbls, data_nr, dest_nr);

	M0_PRE(dest_bufs != NULL);
	M0_PRE(src_buf != NULL);
	M0_PRE(g_tbls != NULL);

	M0_ALLOC_ARR(dest_frags, dest_nr);
	if (dest_frags == NULL)
		return BUF_ALLOC_ERR_INFO(-ENOMEM, "destination fragments",
					  dest_nr);

	block_size = (uint32_t)src_buf->b_nob;

	for (i = 0; i < dest_nr; ++i) {
		BLOCK_SIZE_ASSERT_INFO(block_size, i, dest_bufs);
		dest_frags[i] = (uint8_t *)dest_bufs[i].b_addr;
	}

	ec_encode_data_update(block_size, data_nr, dest_nr, vec_idx,
			      g_tbls, (uint8_t *)src_buf->b_addr, dest_frags);

	m0_free(dest_frags);
	return M0_RC(0);
}

static void fails_sort(struct m0_reed_solomon *rs, uint8_t *fail,
		       uint32_t total_count, uint32_t parity_count)
{
	uint32_t  i;
	uint32_t  alive_nr;

	M0_ENTRY();

	M0_PRE(fail != NULL);
	M0_PRE(rs != NULL);
	M0_PRE(rs->rs_failed_idx != NULL);
	M0_PRE(rs->rs_alive_idx != NULL);

	rs->rs_failed_nr = 0;
	for (i = 0, alive_nr = 0; i < total_count; i++) {
		if (fail[i] != 0) {
			VALUE_ASSERT_INFO(rs->rs_failed_nr < parity_count,
					  rs->rs_failed_nr);
			rs->rs_failed_idx[rs->rs_failed_nr++] = i;
		} else
			rs->rs_alive_idx[alive_nr++] = i;
	}
	M0_LEAVE();
}

static void buf_sort(struct m0_reed_solomon *rs, uint32_t total_count,
		     uint32_t data_count, uint8_t *fail,
		     struct m0_buf *data, struct m0_buf *parity)
{
	uint32_t  i;
	uint32_t  j;
	uint32_t  k;
	uint8_t  *addr;

	M0_ENTRY();

	M0_PRE(rs != NULL);
	M0_PRE(fail != NULL);
	M0_PRE(data != NULL);
	M0_PRE(parity != NULL);
	M0_PRE(rs->rs_bufs_in != NULL);
	M0_PRE(rs->rs_bufs_out != NULL);

	for (i = 0, j = 0, k = 0; i < total_count; i++) {
		if (i < data_count)
			addr = (uint8_t *)data[i].b_addr;
		else
			addr = (uint8_t *)parity[i - data_count].b_addr;

		if (fail[i] != 0)
			rs->rs_bufs_out[j++] = addr;
		else if (k < data_count)
			rs->rs_bufs_in[k++] = addr;
		else
			continue;
	}
	M0_LEAVE();
}

static int isal_gen_recov_coeff_tbl(uint32_t data_count, uint32_t parity_count,
				    struct m0_reed_solomon *rs)
{
	uint32_t  total_count;
	uint32_t  mat_size;
	uint32_t  i;
	uint32_t  j;
	uint32_t  r;
	uint8_t   s;
	uint8_t   idx;
	int       ret;
	uint8_t  *decode_mat = NULL;
	uint8_t  *temp_mat = NULL;
	uint8_t  *invert_mat = NULL;

	M0_ENTRY("data_count=%u, parity_count=%u, rs=%p",
		 data_count, parity_count, rs);

	M0_PRE(rs != NULL);

	total_count = data_count + parity_count;
	mat_size = total_count * data_count;

	M0_ALLOC_ARR(decode_mat, mat_size);
	if (decode_mat == NULL) {
		ret = BUF_ALLOC_ERR_INFO(-ENOMEM, "decode matrix",
					 mat_size);
		goto exit;
	}

	M0_ALLOC_ARR(temp_mat, mat_size);
	if (temp_mat == NULL) {
		ret = BUF_ALLOC_ERR_INFO(-ENOMEM, "temp matrix",
					 mat_size);
		goto exit;
	}

	M0_ALLOC_ARR(invert_mat, mat_size);
	if (invert_mat == NULL) {
		ret = BUF_ALLOC_ERR_INFO(-ENOMEM, "invert matrix",
					 mat_size);
		goto exit;
	}

	/* Construct temp_mat (matrix that encoded remaining frags)
	 * by removing erased rows. */
	for (i = 0; i < data_count; i++) {
		idx = rs->rs_alive_idx[i];
		for (j = 0; j < data_count; j++)
			temp_mat[data_count * i + j] =
				rs->rs_encode_matrix[data_count * idx + j];
	}

	/* Invert matrix to get recovery matrix. */
	ret = gf_invert_matrix(temp_mat, invert_mat, data_count);
	if (ret != 0) {
		ret = M0_ERR_INFO(-EDOM, "failed to construct an %u x %u "
				  "inverse of the input matrix",
				  data_count, data_count);
		goto exit;
	}

	/* Create decode matrix. */
	for (r = 0; r < rs->rs_failed_nr; r++) {
		idx = rs->rs_failed_idx[r];
		/* Get decode matrix with only wanted recovery rows */
		if (idx < data_count) {    /* A src err */
			for (i = 0; i < data_count; i++)
				decode_mat[data_count * r + i] =
					invert_mat[data_count * idx + i];
		} else { /* A parity err */
			/* For non-src (parity) erasures need to multiply
			 * encode matrix * invert */
			for (i = 0; i < data_count; i++) {
				s = 0;
				for (j = 0; j < data_count; j++)
					s ^= gf_mul(invert_mat[j * data_count + i],
						    rs->rs_encode_matrix[j +
								data_count * idx]);
				decode_mat[data_count * r + i] = s;
			}
		}
	}

	ec_init_tables(data_count, rs->rs_failed_nr,
		       decode_mat, rs->rs_decode_tbls);

exit:
	m0_free(decode_mat);
	m0_free(temp_mat);
	m0_free(invert_mat);

	return M0_RC(ret);
}

static void dependency_bitmap_prepare(struct m0_sns_ir_block *f_block,
				      struct m0_sns_ir *ir)
{
	uint32_t i;

	M0_PRE(f_block != NULL && ir != NULL);
	M0_PRE(f_block->sib_status == M0_SI_BLOCK_FAILED);

	for (i = 0; i < ir->si_data_nr; ++i)
		m0_bitmap_set(&f_block->sib_bitmap, ir->si_rs.rs_alive_idx[i], true);
}

static uint32_t last_usable_block_id(const struct m0_sns_ir *ir,
				     uint32_t block_idx)
{
	return ir->si_rs.rs_alive_idx[ir->si_data_nr - 1];
}
#else /* !defined(__KERNEL__) && defined(HAVE_ISA_L_H) */
static void reed_solomon_fini(struct m0_parity_math *math)
{
}

static int reed_solomon_init(struct m0_parity_math *math)
{
	return 0;
}

static void reed_solomon_encode(struct m0_parity_math *math,
				const struct m0_buf *data,
				struct m0_buf *parity)
{
}

static int reed_solomon_diff(struct m0_parity_math *math,
			     struct m0_buf         *old,
			     struct m0_buf         *new,
			     struct m0_buf         *parity,
			     uint32_t               index)
{
	return 0;
}

static int reed_solomon_recover(struct m0_parity_math *math,
				struct m0_buf *data,
				struct m0_buf *parity,
				struct m0_buf *fails,
				enum m0_parity_linsys_algo algo)
{
	return 0;
}

static void ir_rs_init(const struct m0_parity_math *math, struct m0_sns_ir *ir)
{
}

static void ir_failure_register(struct m0_sns_ir *ir, uint32_t failed_index)
{
}

static int ir_mat_compute(struct m0_sns_ir *ir)
{
	return 0;
}

static int ir_recover(struct m0_sns_ir *ir, struct m0_sns_ir_block *alive_block)
{
	return 0;
}

static uint32_t last_usable_block_id(const struct m0_sns_ir *ir,
				     uint32_t block_idx)
{
	return 0;
}
#endif /* !defined(__KERNEL__) && defined(HAVE_ISA_L_H) */

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
