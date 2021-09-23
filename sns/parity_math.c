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

/* Below are some functions which has two separate implementations for using
 * Galois and Intel ISA library. At a time any one of the function is
 * called depending on whether Intel ISA library is present or not. If
 * Intel ISA library is present, it will use implementation specific to Intel
 * ISA and use Intel ISA APIs for encoding and recovery, else it will use
 * Galois library arithmetic functions.
 */

/**
 * This function initialize fields required to use Reed Solomon algorithm.
 * It has two separate implementations for using Galois and Intel ISA library.
 */
static int reed_solomon_init(struct m0_parity_math *math);

/**
 * This function clears fields used by Reed Solomon algorithm.
 * It has two separate implementations for using Galois and Intel ISA library.
 */
static void reed_solomon_fini(struct m0_parity_math *math);

/**
 * This function calculates parity fields using Reed Solomon algorithm.
 * It has two separate implementations for using Galois and Intel ISA library.
 */
static void reed_solomon_encode(struct m0_parity_math *math,
				const struct m0_buf *data,
				struct m0_buf *parity);

/**
 * This function calculates differential parity using Reed Solomon algorithm.
 * It has two separate implementations for using Galois and Intel ISA library.
 */
static int reed_solomon_diff(struct m0_parity_math *math,
			     struct m0_buf         *old,
			     struct m0_buf         *new,
			     struct m0_buf         *parity,
			     uint32_t               index);

/**
 * This function recovers failed data and/or parity using Reed Solomon
 * algorithm. It has two separate implementations for using Galois and Intel
 * ISA library.
 */
static int reed_solomon_recover(struct m0_parity_math *math,
				struct m0_buf *data,
				struct m0_buf *parity,
				struct m0_buf *fails,
				enum m0_parity_linsys_algo algo);

/**
 * Recovers data or parity units partially or fully depending on the parity
 * calculation algorithm, given the failure index.
 * It has two separate implementations for using Galois and Intel ISA library.
 */
static void fail_idx_reed_solomon_recover(struct m0_parity_math *math,
					  struct m0_buf *data,
					  struct m0_buf *parity,
					  const uint32_t failure_index);

/**
 * Initialize fields, specific to Reed Solomon implementation, which are
 * required for incremental recovery.
 * It has two separate implementations for using Galois and Intel ISA library.
 * @param[in]  math    - Pointer to parity math structure.
 * @param[out] ir      - Pointer to incremental recovery structure.
 */
static void ir_rs_init(const struct m0_parity_math *math, struct m0_sns_ir *ir);

/**
 * Free fields initialized for incremental recovery.
 * It has two separate implementations for using Galois and Intel ISA library.
 * @param[in] ir - pointer to incremental recovery structure.
 */
static void ir_rs_fini(struct m0_sns_ir *ir);

/**
 * This function registers failed index.
 * It has two separate implementations for using Galois and Intel ISA library.
 * @param[in, out] ir           - pointer to incremental recovery structure.
 * @param[in]      failed_index - index of the failed block in a parity group.
 */
static void ir_failure_register(struct m0_sns_ir *ir,
				uint32_t failed_index);

/**
 * Computes data-recovery matrix. Populates dependency bitmaps for failed
 * blocks.
 * It has two separate implementations for using Galois and Intel ISA library.
 * @param[in, out] ir       - pointer to incremental recovery structure.
 * @retval         0          on success.
 * @retval         -ENOMEM    on failure to acquire memory.
 * @retval         -EDOM      when input matrix is singular.
 */
static int ir_mat_compute(struct m0_sns_ir *ir);

/**
 * Core routine to recover failed block using current alive block.
 * It has two separate implementations for using Galois and Intel ISA library.
 * @param[in] ir           - Pointer to incremental recovery structure.
 * @param[in] alive_block  - Pointer to the alive block.
 * @retval    0            - success otherwise failure
 */
static int ir_recover(struct m0_sns_ir *ir, struct m0_sns_ir_block *alive_block);


/**
 * Recovery of each failed block depends upon subset of alive blocks.
 * This routine prepares a bitmap indicating this dependency. If a bit at
 *  location 'x' is set 'true' then it implies that f_block has no dependency
 *  on block with index 'x'.
 * It has two separate implementations for using Galois and Intel ISA library.
 */
static void dependency_bitmap_prepare(struct m0_sns_ir_block *f_block,
				      struct m0_sns_ir *ir);

/**
 * Returns last usable block index.
 * It has two separate implementations for using Galois and Intel ISA library.
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

#if ISAL_ENCODE_ENABLED
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
 */
static void isal_encode_data_update(struct m0_buf *dest_bufs, struct m0_buf *src_buf,
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

#else
/* Fills 'mat' with data passed to recovery algorithm. */
static void recovery_mat_fill(struct m0_parity_math *math,
			      uint8_t *fail, uint32_t unit_count, /* in. */
			      struct m0_matrix *mat) /* out. */;

/* Updates internal structures of 'math' with recovered data. */
static void parity_math_recover(struct m0_parity_math *math,
				uint8_t *fail, uint32_t unit_count,
				enum m0_parity_linsys_algo algo);

/* Fills vandermonde matrix with initial values. */
static int vandmat_init(struct m0_matrix *m, uint32_t data_count,
			uint32_t parity_count);

static void vandmat_fini(struct m0_matrix *mat);

/* Normalises vandermonde matrix, upper part of which becomes identity matrix
 * in case of success. */
static int vandmat_norm(struct m0_matrix *m);

static bool check_row_is_id(struct m0_matrix *m, uint32_t row);

/**
 * Inverts the encoding matrix and generates a recovery matrix for lost data.
 * When all failed blocks are parity blocks this function plays no role.
 */
static int data_recov_mat_construct(struct m0_sns_ir *ir);

/**
 * Constructs out_mat by selecting a subset of rows from in_mat.
 * There is one-to-one mapping between blocks and rows of the in_mat.
 * A particular row is selected if its corresponding block has
 * sib_status == status.
 */
static void submatrix_construct(struct m0_matrix *in_mat,
				struct m0_sns_ir_block *blocks,
				enum m0_sns_ir_block_status status,
				struct m0_matrix *out_mat);

static inline uint32_t recov_mat_col(const struct m0_sns_ir_block *alive_block,
				     const struct m0_sns_ir_block *failed_block,
				     const struct m0_sns_ir *ir);

/**
 * Core routine to recover failed_block based on available_block.
 */
static void incr_recover(struct m0_sns_ir_block *failed_block,
			 const struct m0_sns_ir_block *available_block,
			 struct m0_sns_ir *ir);

/**
 * Updates the dependency-bitmap for a failed block once contribution
 * by available block is computed.
 */
static void dependency_bitmap_update(struct m0_sns_ir_block *f_block,
				     const struct m0_bitmap *bitmap);

/**
 * Adds correction for a remote block that is received before all blocks
 * local to a node are transformed. This correction is needed only when
 * set of failed blocks involve both data and parity blocks.
 */
static void forward_rectification(struct m0_sns_ir *ir,
				  struct m0_bufvec *in_bufvec,
				  uint32_t failed_index);

/**
 * When a set of failed blocks contain both data and parity blocks, this
 * function transforms partially recovered data block, local to a node
 * for computing failed parity.
 */
static void failed_data_blocks_xform(struct m0_sns_ir *ir);

static inline  bool are_failures_mixed(const struct m0_sns_ir *ir);

static bool is_data(const struct m0_sns_ir *ir, uint32_t index);

static inline const struct m0_matrix* recovery_mat_get(const struct m0_sns_ir
						       *ir,
						       uint32_t failed_idx);

#endif /* ISAL_ENCODE_ENABLED */

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
	}

	if (ret != 0)
		m0_parity_math_fini(math);

	return (ret == 0) ? M0_RC(ret) : M0_ERR(ret);
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
	return (rc == 0) ? M0_RC(rc) : M0_ERR(rc);
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
	return (rc == 0) ? M0_RC(rc) : M0_ERR(rc);
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

M0_INTERNAL int m0_parity_recov_mat_gen(struct m0_parity_math *math,
					uint8_t *fail)
{
#if !ISAL_ENCODE_ENABLED
	int rc;

	recovery_mat_fill(math, fail,
			  math->pmi_data_count + math->pmi_parity_count,
			  &math->pmi_sys_mat);
	m0_matrix_init(&math->pmi_recov_mat, math->pmi_sys_mat.m_width,
		       math->pmi_sys_mat.m_height);
	rc = m0_matrix_invert(&math->pmi_sys_mat, &math->pmi_recov_mat);

	return rc == 0 ? M0_RC(0) : M0_ERR(rc);
#else
	return 0;
#endif /* !ISAL_ENCODE_ENABLED */
}

M0_INTERNAL void m0_parity_recov_mat_destroy(struct m0_parity_math *math)
{
#if !ISAL_ENCODE_ENABLED
	m0_matrix_fini(&math->pmi_recov_mat);
#endif /* !ISAL_ENCODE_ENABLED */
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

	ir_rs_fini(ir);

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
#if !ISAL_ENCODE_ENABLED
		dependency_bitmap_update(&blocks[failed_index],
					 bitmap);
		if (is_data(ir, failed_index) && are_failures_mixed(ir) &&
		    ir->si_local_nr != 0)
			forward_rectification(ir, bufvec, failed_index);
#endif /* !ISAL_ENCODE_ENABLED */
		break;
	}

	return M0_RC(0);
}

/* Parity Math Helper Functions */

/* Funtions m0_parity_* are to much eclectic. Just more simple names. */
static int gadd(int x, int y)
{
	return m0_parity_add(x, y);
}

static int gmul(int x, int y)
{
	return m0_parity_mul(x, y);
}

#if !ISAL_ENCODE_ENABLED
static int gsub(int x, int y)
{
	return m0_parity_sub(x, y);
}

static int gdiv(int x, int y)
{
	return m0_parity_div(x, y);
}

static int gpow(int x, int p)
{
	return m0_parity_pow(x, p);
}
#endif

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
			for (i = 0; i <seg_size; ++i) {
				y_addr[i] = gadd(y_addr[i],
						 x_addr[i]);
			}
			break;
		default:
			for (i = 0; i < seg_size; ++i) {
				y_addr[i] = gadd(y_addr[i], gmul(x_addr[i],
							         alpha));
			}
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

#if ISAL_ENCODE_ENABLED
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
	tbl_len = math->pmi_data_count * math->pmi_parity_count * 32;

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

	/* Generate a matrix of coefficients to be used for encoding. */
	gf_gen_rs_matrix(rs->rs_encode_matrix, total_count,
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
	isal_encode_data_update(parity, &diff_data_buf, index,
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

static void ir_rs_fini(struct m0_sns_ir *ir)
{
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
	uint8_t                  curr_idx = UINT8_MAX;
	uint32_t                 i;
	uint32_t                 length;
	int                      ret = 0;

	M0_ENTRY("ir=%p, alive_block=%p", ir, alive_block);

	rs = &ir->si_rs;

	struct m0_buf            out_bufs[rs->rs_failed_nr];

	/* Check if given alive block is dependecy of any failed block. */
	for (i = 0; i < rs->rs_failed_nr; i++) {
		failed_bitmap = &ir->si_blocks[rs->rs_failed_idx[i]].sib_bitmap;
		if (m0_bitmap_get(failed_bitmap, alive_block->sib_idx) == 0)
			return M0_RC(ret);
	}

	alive_bufvec = alive_block->sib_addr;
	length = (uint32_t)m0_vec_count(&alive_bufvec->ov_vec);

	M0_SET_ARR0(out_bufs);

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
	isal_encode_data_update(out_bufs, &in_buf, curr_idx,
				rs->rs_decode_tbls, ir->si_data_nr,
				rs->rs_failed_nr);

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

	return M0_RC(ret);
}

static void isal_encode_data_update(struct m0_buf *dest_bufs, struct m0_buf *src_buf,
				    uint32_t vec_idx, uint8_t *g_tbls,
				    uint32_t data_nr, uint32_t dest_nr)
{
	uint32_t i;
	uint32_t block_size;

	M0_ENTRY("dest_bufs=%p, src_buf=%p, vec_idx=%u, "
		 "g_tbls=%p, data_nr=%u, dest_nr=%u",
		 dest_bufs, src_buf, vec_idx, g_tbls, data_nr, dest_nr);

	M0_PRE(dest_bufs != NULL);
	M0_PRE(src_buf != NULL);
	M0_PRE(g_tbls != NULL);

	uint8_t *dest_frags[dest_nr];

	block_size = (uint32_t)src_buf->b_nob;

	for (i = 0; i < dest_nr; ++i) {
		BLOCK_SIZE_ASSERT_INFO(block_size, i, dest_bufs);
		dest_frags[i] = (uint8_t *)dest_bufs[i].b_addr;
	}

	ec_encode_data_update(block_size, data_nr, dest_nr, vec_idx,
			      g_tbls, (uint8_t *)src_buf->b_addr, dest_frags);

	M0_LEAVE();
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
		}
		else
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

	M0_ENTRY("data_count=%u, parity_count=%u, rs=%p",
		 data_count, parity_count, rs);

	M0_PRE(rs != NULL);

	total_count = data_count + parity_count;
	mat_size = total_count * data_count;

	uint8_t   decode_mat[mat_size];
	uint8_t   temp_mat[mat_size];
	uint8_t   invert_mat[mat_size];

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
	if (ret != 0)
		return M0_ERR_INFO(ret, "failed to construct an %u x %u "
				   "inverse of the input matrix",
				   data_count, data_count);

	/* Create decode matrix. */
	for (r = 0; r < rs->rs_failed_nr; r++) {
		idx = rs->rs_failed_idx[r];
		/* Get decode matrix with only wanted recovery rows */
		if (idx < data_count) {    /* A src err */
			for (i = 0; i < data_count; i++)
				decode_mat[data_count * r + i] =
					invert_mat[data_count * idx + i];
		}
		/* For non-src (parity) erasures need to multiply
		 * encode matrix * invert */
		else { /* A parity err */
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

#else
static void reed_solomon_fini(struct m0_parity_math *math)
{
	M0_ENTRY();
	vandmat_fini(&math->pmi_vandmat);
	m0_matrix_fini(&math->pmi_vandmat_parity_slice);
	m0_matvec_fini(&math->pmi_data);
	m0_matvec_fini(&math->pmi_parity);

	m0_linsys_fini(&math->pmi_sys);
	m0_matrix_fini(&math->pmi_sys_mat);
	m0_matvec_fini(&math->pmi_sys_vec);
	m0_matvec_fini(&math->pmi_sys_res);
	M0_LEAVE();
}

static int reed_solomon_init(struct m0_parity_math *math)
{
	int ret;

	M0_ENTRY();

	ret = vandmat_init(&math->pmi_vandmat, math->pmi_data_count,
			   math->pmi_parity_count);
	if (ret < 0)
		return M0_ERR(ret);

	ret = vandmat_norm(&math->pmi_vandmat);
	if (ret < 0)
		return M0_ERR(ret);

	ret = m0_matrix_init(&math->pmi_vandmat_parity_slice,
			     math->pmi_data_count, math->pmi_parity_count);
	if (ret < 0)
		return MAT_INIT_ERR_INFO(ret, "vandermonde parity slice",
					 math->pmi_parity_count,
					 math->pmi_data_count);

	m0_matrix_submatrix_get(&math->pmi_vandmat,
				&math->pmi_vandmat_parity_slice, 0,
				math->pmi_data_count);

	ret = m0_matvec_init(&math->pmi_data, math->pmi_data_count);
	if (ret < 0)
		return MATVEC_INIT_ERR_INFO(ret, "data", math->pmi_data_count);

	ret = m0_matvec_init(&math->pmi_parity, math->pmi_parity_count);
	if (ret < 0)
		return MATVEC_INIT_ERR_INFO(ret, "parity",
					    math->pmi_parity_count);

	ret = m0_matvec_init(&math->pmi_sys_vec, math->pmi_data.mv_size);
	if (ret < 0)
		return MATVEC_INIT_ERR_INFO(ret, "sys_vec",
					    math->pmi_data.mv_size);

	ret = m0_matvec_init(&math->pmi_sys_res, math->pmi_data.mv_size);
	if (ret < 0)
		return MATVEC_INIT_ERR_INFO(ret, "sys_res",
					    math->pmi_data.mv_size);

	ret = m0_matrix_init(&math->pmi_sys_mat, math->pmi_data.mv_size,
			     math->pmi_data.mv_size);
	if (ret < 0)
		return MAT_INIT_ERR_INFO(ret, "sys_mat", math->pmi_data.mv_size,
					 math->pmi_data.mv_size);

	return M0_RC(ret);
}

static void reed_solomon_encode(struct m0_parity_math *math,
				const struct m0_buf *data,
				struct m0_buf *parity)
{
#define PARITY_MATH_REGION_ENABLE 0

#if !PARITY_MATH_REGION_ENABLE
	uint32_t          ei; /* block element index. */
#endif
	uint32_t          pi; /* parity unit index. */
	uint32_t          di; /* data unit index. */
	m0_parity_elem_t  mat_elem;
	uint32_t          block_size;

	M0_ENTRY();

	M0_PRE(parity_math_invariant(math));
	M0_PRE(data != NULL);
	M0_PRE(parity != NULL);

	block_size = data[0].b_nob;

	for (di = 1; di < math->pmi_data_count; ++di)
		BLOCK_SIZE_ASSERT_INFO(block_size, di, data);

	for (pi = 0; pi < math->pmi_parity_count; ++pi)
		BLOCK_SIZE_ASSERT_INFO(block_size, pi, parity);

	for (pi = 0; pi < math->pmi_parity_count; ++pi) {
		for (di = 0; di < math->pmi_data_count; ++di) {
			mat_elem =
			*m0_matrix_elem_get(&math->pmi_vandmat_parity_slice,
					    di, pi);
			if (mat_elem == 0)
				continue;
#if !PARITY_MATH_REGION_ENABLE
			for (ei = 0; ei < block_size; ++ei) {
				if (di == 0)
					((uint8_t*)parity[pi].b_addr)[ei] =
						M0_PARITY_ZERO;

				((uint8_t*)parity[pi].b_addr)[ei] =
					gadd(((uint8_t*)parity[pi].b_addr)[ei],
					     gmul(mat_elem,
						  ((uint8_t*)data[di].b_addr)
						  [ei]));
			}
#else
			if (di == 0)
				memset((uint8_t*)parity[pi].b_addr,
				       M0_PARITY_ZERO, block_size);

			m0_parity_region_mac(
				(m0_parity_elem_t*) parity[pi].b_addr,
				(const m0_parity_elem_t*) data[di].b_addr,
				block_size,
				mat_elem);
#endif
		}
	}

#undef PARITY_MATH_REGION_ENABLE
	M0_LEAVE();
}

static int reed_solomon_diff(struct m0_parity_math *math,
			     struct m0_buf         *old,
			     struct m0_buf         *new,
			     struct m0_buf         *parity,
			     uint32_t               index)
{
	struct m0_matrix *mat;
	uint32_t          ei;
	uint32_t          ui;
	uint8_t           diff_data;
	m0_parity_elem_t  mat_elem;

	M0_ENTRY();

	M0_PRE(math   != NULL);
	M0_PRE(old    != NULL);
	M0_PRE(new    != NULL);
	M0_PRE(parity != NULL);
	M0_PRE(index  <  math->pmi_data_count);
	M0_PRE(old[index].b_nob == new[index].b_nob);
	M0_PRE(m0_forall(i, math->pmi_parity_count,
		         new[index].b_nob == parity[i].b_nob));

	mat = &math->pmi_vandmat_parity_slice;
	for (ui = 0; ui < math->pmi_parity_count; ++ui) {
		for (ei = 0; ei < new[index].b_nob; ++ei) {
			mat_elem = *m0_matrix_elem_get(mat, index, ui);
			diff_data = ((uint8_t *)old[index].b_addr)[ei] ^
				    ((uint8_t *)new[index].b_addr)[ei];
			((uint8_t*)parity[ui].b_addr)[ei] ^=
				gmul(diff_data, mat_elem);
		}
	}

	return M0_RC(0);
}

static int reed_solomon_recover(struct m0_parity_math *math,
				struct m0_buf *data,
				struct m0_buf *parity,
				struct m0_buf *fails,
				enum m0_parity_linsys_algo algo)
{
	uint32_t  ei; /* block element index. */
	uint32_t  ui; /* unit index. */
	uint8_t  *fail;
	uint32_t  fail_count;
	uint32_t  unit_count = math->pmi_data_count + math->pmi_parity_count;
	uint32_t  block_size = data[0].b_nob;

	M0_ENTRY();

	fail = (uint8_t*) fails->b_addr;
	fail_count = fails_count(fail, unit_count);

	M0_ASSERT(fail_count > 0);
	M0_ASSERT(fail_count <= math->pmi_parity_count);

	for (ui = 1; ui < math->pmi_data_count; ++ui)
		M0_ASSERT(block_size == data[ui].b_nob);

	for (ui = 0; ui < math->pmi_parity_count; ++ui)
		M0_ASSERT(block_size == parity[ui].b_nob);

	for (ei = 0; ei < block_size; ++ei) {
		struct m0_matvec *recovered = &math->pmi_sys_res;

		/* load data and parity. */
		for (ui = 0; ui < math->pmi_data_count; ++ui)
			*m0_matvec_elem_get(&math->pmi_data, ui) =
				((uint8_t*)data[ui].b_addr)[ei];

		for (ui = 0; ui < math->pmi_parity_count; ++ui)
			*m0_matvec_elem_get(&math->pmi_parity, ui) =
				((uint8_t*)parity[ui].b_addr)[ei];

		/* recover data. */
		parity_math_recover(math, fail, unit_count, algo);
		/* store data. */
		for (ui = 0; ui < math->pmi_data_count; ++ui) {
			if (fail[ui] == 0)
				continue;
			((uint8_t*)data[ui].b_addr)[ei] =
				*m0_matvec_elem_get(recovered, ui);
		}
	}

	return M0_RC(0);
}

static void recovery_mat_fill(struct m0_parity_math *math,
			      uint8_t *fail, uint32_t unit_count, /* in. */
			      struct m0_matrix *mat) /* out. */
{
	uint32_t f;
	uint32_t y = 0;
	uint32_t x;

	for (f = 0; f < unit_count; ++f) {
		if (!fail[f] && y < mat->m_height) {
			for (x = 0; x < mat->m_width; ++x)
				*m0_matrix_elem_get(mat, x, y) =
					*m0_matrix_elem_get(&math->pmi_vandmat,
							    x, f);
			++y;
		}
	}
}

/* Fills 'mat' and 'vec' with data passed to recovery algorithm. */
static void recovery_vec_fill(struct m0_parity_math *math,
			       uint8_t *fail, uint32_t unit_count, /* in. */
			       struct m0_matvec *vec) /* out. */
{
	uint32_t f;
	uint32_t y = 0;

	for (f = 0; f < unit_count; ++f) {
		/*
		 * if (block is ok) and
		 * (not enough equations to solve system).
		 */
		if (!fail[f] && y < vec->mv_size) {
			/* copy vec. */
			*m0_matvec_elem_get(vec, y) = f < math->pmi_data_count
				? *m0_matvec_elem_get(&math->pmi_data, f)
				: *m0_matvec_elem_get(&math->pmi_parity,
						f - math->pmi_data_count);
			++y;
		}
	}
}

static void parity_math_recover(struct m0_parity_math *math,
				uint8_t *fail, uint32_t unit_count,
				enum m0_parity_linsys_algo algo)
{
	struct m0_matrix *mat = &math->pmi_sys_mat;
	struct m0_matvec *vec = &math->pmi_sys_vec;
	struct m0_matvec *res = &math->pmi_sys_res;
	struct m0_linsys *sys = &math->pmi_sys;
	uint32_t          i;

	recovery_vec_fill(math, fail, unit_count, vec);
	if (algo == M0_LA_GAUSSIAN) {
		recovery_mat_fill(math, fail, unit_count, mat);
		m0_linsys_init(sys, mat, vec, res);
		m0_linsys_solve(sys);
	} else {
		for (i = 0; i < math->pmi_data_count; ++i) {
			if (fail[i] == 0)
				continue;
			m0_matrix_vec_multiply(&math->pmi_recov_mat, vec, res, m0_parity_mul, m0_parity_add);
		}
	}
}

static int vandmat_init(struct m0_matrix *m, uint32_t data_count,
			uint32_t parity_count)
{
	int      ret;
	uint32_t y;
	uint32_t x;
	uint32_t mat_height = data_count + parity_count;
	uint32_t mat_width = data_count;

	ret = m0_matrix_init(m, mat_width, mat_height);
	if (ret < 0)
		return M0_ERR(ret);

	for (y = 0; y < mat_height; ++y)
		for (x = 0; x < mat_width; ++x)
			*m0_matrix_elem_get(m, x, y) = gpow(y, x);

	return ret;
}

static void vandmat_fini(struct m0_matrix *mat)
{
	m0_matrix_fini(mat);
}

static int vandmat_norm(struct m0_matrix *m)
{
	uint32_t y;

	for (y = 0; y < m->m_width; ++y) {
		uint32_t x = 0;
		m0_matrix_col_operate(m, y, *m0_matrix_elem_get(m, y, y), gdiv);

		for (x = 0; x < m->m_width; ++x)
			if (x != y)
				m0_matrix_cols_operate(m, x, y, gsub, 0, gmul,
                                            *m0_matrix_elem_get(m, x, y), gsub);

		/* Assert if units are not configured properly. */
		M0_ASSERT(check_row_is_id(m, y));
	}

	return 0;
}

/* Checks if row has only one element equals 1, and 0 others */
static bool check_row_is_id(struct m0_matrix *m, uint32_t row)
{
	bool     ret = true;
	uint32_t x;

	for (x = 0; x < m->m_width && ret; ++x)
		ret &= (row == x) == *m0_matrix_elem_get(m, x, row);

	return ret;
}

static void ir_rs_init(const struct m0_parity_math *math, struct m0_sns_ir *ir)
{
	uint32_t i;

	M0_ENTRY("math=%p, ir=%p", math, ir);

	M0_PRE(math != NULL);
	M0_PRE(ir != NULL);
	M0_PRE(ir->si_blocks != NULL);

	ir->si_vandmat             = math->pmi_vandmat;
	ir->si_parity_recovery_mat = math->pmi_vandmat_parity_slice;
	ir->si_failed_data_nr      = 0;

	for (i = 0; i < ir_blocks_count(ir); ++i)
		ir->si_blocks[i].sib_data_recov_mat_col = IR_INVALID_COL;

	M0_LEAVE();
}

static void ir_rs_fini(struct m0_sns_ir *ir)
{
	m0_matrix_fini(&ir->si_data_recovery_mat);
}

static void ir_failure_register(struct m0_sns_ir *ir,
				uint32_t failed_index)
{
	if (is_data(ir, failed_index))
		M0_CNT_INC(ir->si_failed_data_nr);
}

static int ir_mat_compute(struct m0_sns_ir *ir)
{
	int                     ret = 0;
	uint32_t                i;
	uint32_t                j;
	struct m0_sns_ir_block *blocks;

	blocks = ir->si_blocks;
	if (ir->si_failed_data_nr != 0) {
		for (j = 0, i = 0; j < ir_blocks_count(ir) && i < ir->si_data_nr;
		     ++j) {
			if (blocks[j].sib_status == M0_SI_BLOCK_ALIVE) {
				blocks[j].sib_data_recov_mat_col = i;
				++i;
			}
		}
		ret = data_recov_mat_construct(ir);
		if (ret != 0)
			return M0_ERR(ret);
	}

	for (j = 0, i = 0; j < ir_blocks_count(ir); ++j) {
		if (blocks[j].sib_status == M0_SI_BLOCK_FAILED) {
			blocks[j].sib_recov_mat_row = is_data(ir, j) ? i :
				j - ir->si_data_nr;
			dependency_bitmap_prepare(&blocks[j], ir);
			++i;
		}
	}

	return M0_RC(ret);
}

static int ir_recover(struct m0_sns_ir *ir, struct m0_sns_ir_block *alive_block)
{
	struct m0_sns_ir_block *blocks;
	uint32_t                i;

	M0_PRE(ir != NULL);
	M0_PRE(alive_block != NULL);

	blocks = ir->si_blocks;

	for (i = 0; i < ir_blocks_count(ir); ++i)
		if (blocks[i].sib_status == M0_SI_BLOCK_FAILED) {
			incr_recover(&blocks[i], alive_block, ir);
			m0_bitmap_set(&blocks[i].sib_bitmap,
					alive_block->sib_idx, false);
		}

	if (ir->si_local_nr == 0 && are_failures_mixed(ir))
		failed_data_blocks_xform(ir);

	return M0_RC(0);
}

static void dependency_bitmap_prepare(struct m0_sns_ir_block *f_block,
				      struct m0_sns_ir *ir)
{
	uint32_t i;

	M0_PRE(f_block != NULL && ir != NULL);
	M0_PRE(f_block->sib_status == M0_SI_BLOCK_FAILED);

	if (is_data(ir, f_block->sib_idx)) {
		for (i = 0; i < ir_blocks_count(ir); ++i) {
			if (ir->si_blocks[i].sib_status != M0_SI_BLOCK_ALIVE)
				continue;
			if (ir->si_blocks[i].sib_data_recov_mat_col !=
			    IR_INVALID_COL)
				m0_bitmap_set(&f_block->sib_bitmap,
					      ir->si_blocks[i].sib_idx, true);
		}
	} else {
		for (i = 0; i < ir->si_data_nr; ++i) {
			m0_bitmap_set(&f_block->sib_bitmap,
				      ir->si_blocks[i].sib_idx, true);
		}
	}
}

static int data_recov_mat_construct(struct m0_sns_ir *ir)
{
	int              ret = 0;
	struct m0_matrix encode_mat;
	struct m0_matrix encode_mat_inverse;

	M0_SET0(&encode_mat);
	M0_SET0(&encode_mat_inverse);

	M0_PRE(ir != NULL);
	M0_PRE(ir->si_blocks != NULL);

	ret = m0_matrix_init(&encode_mat, ir->si_data_nr,
			     ir->si_data_nr);
	if (ret != 0)
		goto fini;
	submatrix_construct(&ir->si_vandmat, ir->si_blocks,
			    M0_SI_BLOCK_ALIVE, &encode_mat);
	ret = m0_matrix_init(&encode_mat_inverse, encode_mat.m_width,
			     encode_mat.m_height);
	if (ret != 0)
		goto fini;
	ret = m0_matrix_invert(&encode_mat, &encode_mat_inverse);
	if (ret != 0)
		goto fini;
	ret = m0_matrix_init(&ir->si_data_recovery_mat, ir->si_data_nr,
			     ir->si_failed_data_nr);
	if (ret != 0)
		goto fini;
	submatrix_construct(&encode_mat_inverse, ir->si_blocks,
			    M0_SI_BLOCK_FAILED, &ir->si_data_recovery_mat);
fini:
	m0_matrix_fini(&encode_mat);
	m0_matrix_fini(&encode_mat_inverse);
	return (ret == 0) ? M0_RC(ret) : M0_ERR(ret);
}

static void submatrix_construct(struct m0_matrix *in_mat,
				struct m0_sns_ir_block *blocks,
				enum m0_sns_ir_block_status status,
				struct m0_matrix *out_mat)
{
	uint32_t out_row;
	uint32_t in_row;

	M0_PRE(in_mat != NULL);
	M0_PRE(out_mat != NULL);
	M0_PRE(blocks != NULL);
	M0_PRE(m0_matrix_is_init(in_mat));
	M0_PRE(m0_matrix_is_init(out_mat));
	M0_PRE(out_mat->m_width == in_mat->m_width);

	for (in_row = 0, out_row = 0; in_row < in_mat->m_height &&
	     out_row < out_mat->m_height; ++in_row) {
		if (blocks[in_row].sib_status == status) {
			m0_matrix_row_copy(out_mat, in_mat, out_row, in_row);
			++out_row;
		}
	}
}

static inline uint32_t recov_mat_col(const struct m0_sns_ir_block *alive_block,
				     const struct m0_sns_ir_block *failed_block,
				     const struct m0_sns_ir *ir)
{
	M0_PRE(alive_block != NULL);
	M0_PRE(failed_block != NULL);
	return is_data(ir, failed_block->sib_idx) ?
		alive_block->sib_data_recov_mat_col : alive_block->sib_idx;
}

static void incr_recover(struct m0_sns_ir_block *failed_block,
			 const struct m0_sns_ir_block *alive_block,
			 struct m0_sns_ir *ir)
{
	const struct m0_matrix *mat;
	uint32_t                row;
	uint32_t                col;
	uint32_t                last_usable_bid;
	m0_parity_elem_t        mat_elem;

	M0_PRE(failed_block != NULL);
	M0_PRE(alive_block != NULL);
	M0_PRE(ir != NULL);
	last_usable_bid = last_usable_block_id(ir, failed_block->sib_idx);
	if (alive_block->sib_idx <= last_usable_bid &&
	    m0_bitmap_get(&failed_block->sib_bitmap, alive_block->sib_idx)) {
		mat = recovery_mat_get(ir, failed_block->sib_idx);
		row = failed_block->sib_recov_mat_row;
		col = recov_mat_col(alive_block, failed_block, ir);
		mat_elem = *m0_matrix_elem_get(mat, col, row);
		gfaxpy(failed_block->sib_addr, alive_block->sib_addr,
		       mat_elem);
	}
}

static inline const struct m0_matrix* recovery_mat_get(const struct m0_sns_ir
						       *ir, uint32_t failed_idx)
{
	return is_data(ir, failed_idx) ? &ir->si_data_recovery_mat :
		&ir->si_parity_recovery_mat;
}

static void dependency_bitmap_update(struct m0_sns_ir_block *block,
				     const struct m0_bitmap *bitmap)
{
	size_t i;
	size_t b_set_nr;

	b_set_nr = m0_bitmap_set_nr(bitmap);
	for (i = 0; i < bitmap->b_nr && b_set_nr != 0; ++i) {
		if (m0_bitmap_get(bitmap, i)) {
			m0_bitmap_set(&block->sib_bitmap, i, false);
			--b_set_nr;
		}
	}
}

static void forward_rectification(struct m0_sns_ir *ir,
				  struct m0_bufvec *in_bufvec,
				  uint32_t failed_index)
{
	struct m0_sns_ir_block in_block;
	uint32_t               j;

	M0_PRE(ir->si_local_nr != 0);
	M0_PRE(is_data(ir, failed_index));
	M0_PRE(ir->si_blocks[failed_index].sib_status == M0_SI_BLOCK_FAILED);

	memcpy(&in_block, &ir->si_blocks[failed_index], sizeof in_block);
	in_block.sib_addr = in_bufvec;
	for (j = 0; j < ir_blocks_count(ir); ++j)
		if (!is_data(ir, ir->si_blocks[j].sib_idx) &&
		    ir->si_blocks[j].sib_status == M0_SI_BLOCK_FAILED)
			incr_recover(&ir->si_blocks[j],
				     &in_block,
				     ir);
}

static void failed_data_blocks_xform(struct m0_sns_ir *ir)
{
	struct m0_sns_ir_block *res_block;
	struct m0_sns_ir_block *par_block;
	uint32_t                i;
	uint32_t                j;

	M0_PRE(ir->si_local_nr == 0);
	res_block = ir->si_blocks;
	par_block = ir->si_blocks;

	for (i = 0; i < ir_blocks_count(ir); ++i) {
		if (is_data(ir, ir->si_blocks[i].sib_idx) &&
			    ir->si_blocks[i].sib_status == M0_SI_BLOCK_FAILED)
			for (j = 0; j < ir_blocks_count(ir); ++j) {
				if (!is_data(ir, ir->si_blocks[j].sib_idx) &&
					     ir->si_blocks[j].sib_status ==
					     M0_SI_BLOCK_FAILED) {
					incr_recover(&par_block[j],
						     &res_block[i], ir);
					m0_bitmap_set(&par_block[j].sib_bitmap,
						      res_block[i].sib_idx,
						      false);
				}
			}
	}
}

static inline  bool are_failures_mixed(const struct m0_sns_ir *ir)
{
	return !!ir->si_failed_data_nr &&
		ir_blocks_count(ir) != ir->si_failed_data_nr + ir->si_alive_nr;
}

static bool is_data(const struct m0_sns_ir *ir, uint32_t index)
{
	M0_PRE(is_valid_block_idx(ir, index));
	return index < ir->si_data_nr;
}

static uint32_t last_usable_block_id(const struct m0_sns_ir *ir,
				     uint32_t block_idx)
{
	uint32_t i;
	uint32_t last_usable_bid = ir_blocks_count(ir);

	if (is_data(ir, block_idx)) {
		for (i = 0; i  < ir_blocks_count(ir); ++i) {
			if (ir->si_blocks[i].sib_status == M0_SI_BLOCK_ALIVE) {
				if (ir->si_blocks[i].sib_data_recov_mat_col ==
				    IR_INVALID_COL)
					return last_usable_bid;
				last_usable_bid = i;
			}
		}
	} else
		return ir->si_data_nr - 1;
	return last_usable_bid;
}

#endif /* ISAL_ENCODE_ENABLED */

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
