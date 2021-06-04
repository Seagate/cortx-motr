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

enum {
	IR_INVALID_COL = UINT8_MAX,
};

#define ALLOC_ARR_INFO(arr, nr, msg, ret) ({					\
	M0_ALLOC_ARR(arr, nr);							\
	if (arr == NULL) 							\
		(ret) = M0_ERR_INFO(-ENOMEM,					\
				    "failed to allocate memory for " msg);	\
	else 									\
		(ret) = 0;							\
})

/* Forward declarations */
static void xor_calculate(struct m0_parity_math *math,
                          const struct m0_buf *data,
                          struct m0_buf *parity);

#if ISAL_ENCODE_ENABLED
static void isal_encode(struct m0_parity_math *math,
                        const struct m0_buf *data,
                        struct m0_buf *parity);
#else
static void reed_solomon_encode(struct m0_parity_math *math,
                                const struct m0_buf *data,
                                struct m0_buf *parity);
#endif /* ISAL_ENCODE_ENABLED */

static void xor_diff(struct m0_parity_math *math,
		     struct m0_buf         *old,
		     struct m0_buf         *new,
		     struct m0_buf         *parity,
		     uint32_t               index);

#if ISAL_ENCODE_ENABLED
static void isal_diff(struct m0_parity_math *math,
		      struct m0_buf         *old,
		      struct m0_buf         *new,
		      struct m0_buf         *parity,
		      uint32_t               index);

static bool parity_math_invariant(const struct m0_parity_math *math);
#else
static void reed_solomon_diff(struct m0_parity_math *math,
		              struct m0_buf         *old,
		              struct m0_buf         *new,
		              struct m0_buf         *parity,
		              uint32_t               index);
#endif /* ISAL_ENCODE_ENABLED */

static void xor_recover(struct m0_parity_math *math,
                        struct m0_buf *data,
                        struct m0_buf *parity,
                        struct m0_buf *fails,
			enum m0_parity_linsys_algo algo);

#if ISAL_ENCODE_ENABLED
static void isal_recover(struct m0_parity_math *math,
			 struct m0_buf *data,
			 struct m0_buf *parity,
			 struct m0_buf *fails,
			 enum m0_parity_linsys_algo algo);

/**
 * This is wrapper function for Intel ISA API ec_encode_data_update().
 * @param[out] dest_buf - Array of coded output buffers i.e. struct m0_buf
 * @param[in]  src_buf  - Pointer to single input source (struct m0_buf) used to
 *                        update output parity.
 * @param[in]  vec_idx  - The vector index corresponding to the single
 *                        input source.
 * @param[in]  g_tbls   - Pointer to array of input tables generated from
 *                        coding coefficients in ec_init_tables().
 *                        Must be of size 32*data_nr*dest_nr
 * @param[in]  data_nr  - The number of data blocks for coding.
 * @param[in]  dest_nr  - The number of output blocks to concurrently
 *                        encode/decode.
 * @retval     0        - success otherwise failure
 */
static int isal_encode_data_update(struct m0_buf *dest_buf, struct m0_buf *src_buf,
				   uint32_t vec_idx, uint8_t *g_tbls,
				   uint32_t data_nr, uint32_t dest_nr);

/**
 * Inverts the encoding matrix and generates tables of recovery coefficient
 * codes for lost data.
 * @param[in]  data_count     - count of SNS data units used in system.
 * @param[in]  parity_count   - count of SNS parity units used in system.
 * @param[in]  failed_idx_buf - array containing failed block indices, treated
 *                              as uint8_t block with b_nob elements.
 * @param[in]  alive_idx_buf  - array containing non-failed block indices,
 *                              treated as uint8_t block with b_nob elements.
 * @param[in]  encode_mat     - Pointer to sets of arrays of input coefficients used
 *                              to encode or decode data.
 * @param[out] g_tbls         - Pointer to concatenated output tables for decode
 * @retval     0              - success otherwise failure
 */
static int isal_gen_recov_coeff_tbl(uint32_t data_count, uint32_t parity_count,
				    struct m0_buf *failed_idx_buf,
				    struct m0_buf *alive_idx_buf,
				    uint8_t *encode_mat, uint8_t *g_tbls);

/**
 * Sort the data and parity buffers based on input fail buffer. If buffer is
 * marked as failed, its pointer will be added in frags_out buffer array. If
 * buffer is not marked as failed in fail buffer, its pointer will be added in
 * frags_in buffer array. Buffer array frags_in will be used as source buffers
 * for recovery. Buffer array frags_out will be used as buffers to be recovered.
 * @param[out] frags_in   - Array of buffer pointers containing pointers of
 *                          buffers which are not failed.
 * @param[out] frags_out  - Array of buffer pointers containing pointers of
 *                          failed buffers.
 * @param[in]  unit_count - Total count of buffers i.e. data_count + parity_count
 * @param[in]  data_count - count of SNS data units used in system.
 * @param[in]  fail       - block with flags, treated as uint8_t block with
 *                          b_nob elements, if element is '1' then data or parity
 *                          block with given index is treated as broken.
 * @param[in]  data       - data block, treated as uint8_t block with
 *                          b_nob elements.
 * @param[in]  parity     - parity block, treated as uint8_t block with
 *                          b_nob elements.
 * @retval     true         on success
 * @retval     false        on failure to sort buffers
 */
static bool buf_sort(uint8_t **frags_in, uint8_t **frags_out,
		    uint32_t unit_count, uint32_t data_count,
		    uint8_t *fail, struct m0_buf *data,
		    struct m0_buf *parity);

/**
 * Sort the indices for failed and non-failed data and parity blocks.
 * @param[in]  fail       - block with flags, if element is '1' then data or parity
 *                          block with given index is treated as broken.
 * @param[in]  unit_count - Total length of fail buffer
 * @param[out] failed_idx - block with failed indices, treated as uint8_t block
 *                          with b_nob elements
 * @param[out] alive_idx  - block with non-failed (alive) indices, treated as
 *                          uint8_t block with b_nob elements
 * @retval     true         on success
 * @retval     false        on failure to sort indices
 */
static bool fails_sort(uint8_t *fail, uint32_t unit_count,
		       struct m0_buf *failed_idx, struct m0_buf *alive_idx);
#endif /* ISAL_ENCODE_ENABLED */

#if !ISAL_ENCODE_ENABLED
static void reed_solomon_recover(struct m0_parity_math *math,
                                 struct m0_buf *data,
                                 struct m0_buf *parity,
                                 struct m0_buf *fails,
				 enum m0_parity_linsys_algo algo);
#endif /* !ISAL_ENCODE_ENABLED */

static void fail_idx_xor_recover(struct m0_parity_math *math,
				 struct m0_buf *data,
				 struct m0_buf *parity,
				 const uint32_t failure_index);

#if !ISAL_ENCODE_ENABLED
static void fail_idx_reed_solomon_recover(struct m0_parity_math *math,
					  struct m0_buf *data,
					  struct m0_buf *parity,
					  const uint32_t failure_index);
#endif /* !ISAL_ENCODE_ENABLED */

#if ISAL_ENCODE_ENABLED
static void fail_idx_isal_recover(struct m0_parity_math *math,
				  struct m0_buf *data,
				  struct m0_buf *parity,
				  const uint32_t failure_index);

/**
 * Generate decode matrix for incremental recovery using Intel ISA.
 * @param[in] ir  - Pointer to incremental recovery structure.
 * @retval     0  - success otherwise failure
 */
static int ir_gen_decode_matrix(struct m0_sns_ir *ir);
#endif /* ISAL_ENCODE_ENABLED */

#if !ISAL_ENCODE_ENABLED
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
#endif /* !ISAL_ENCODE_ENABLED */

/**
 * Recovery of each failed block depends upon subset of alive blocks.
 * This routine prepares a bitmap indicating this dependency. If a bit at
 *  location 'x' is set 'true' then it implies that f_block has no dependency
 *  on block with index 'x'.
 */
static void dependency_bitmap_prepare(struct m0_sns_ir_block *f_block,
				      struct m0_sns_ir *ir);

#if ISAL_ENCODE_ENABLED
/**
 * Converts given number of m0_bufvec to m0_buf.
 * @param[out] bufs  - Array of m0_buf.
 * @param[in]  bvecs - Array of pointers to m0_bufvec.
 * @param[in]  count - Number of buffers to copy
 */
static int bufvec_to_buf_copy(struct m0_buf *bufs,
			      struct m0_bufvec **bvecs,
			      uint32_t count);

/**
 * Converts given number of m0_buf to m0_bufvec.
 * @param[out] bvecs - Array of pointers to m0_bufvec.
 * @param[in]  bufs  - Array of m0_buf.
 * @param[in]  count - Number of buffers to copy
 */
static int buf_to_bufvec_copy(struct m0_bufvec **bvecs,
			      const struct m0_buf *bufs,
			      uint32_t count);

/**
 * Core routine to recover failed_block based on available_block using
 * Intel ISA library.
 * @param[in] ir           - Pointer to incremental recovery structure.
 * @param[in] alive_block  - Pointer to the alive block.
 * @retval    0            - success otherwise failure
 */
static int ir_recover(struct m0_sns_ir *ir, struct m0_sns_ir_block *alive_block);
#endif /* ISAL_ENCODE_ENABLED */

#if !ISAL_ENCODE_ENABLED
static inline uint32_t recov_mat_col(const struct m0_sns_ir_block *alive_block,
				     const struct m0_sns_ir_block *failed_block,
				     const struct m0_sns_ir *ir);

/**
 * Updates the dependency-bitmap for a failed block once contribution
 * by available block is computed.
 */
static void dependency_bitmap_update(struct m0_sns_ir_block *f_block,
				     const struct m0_bitmap *bitmap);

/**
 * Core routine to recover failed_block based on available_block.
 */
static void incr_recover(struct m0_sns_ir_block *failed_block,
			 const struct m0_sns_ir_block *available_block,
			 struct m0_sns_ir *ir);
#endif /* !ISAL_ENCODE_ENABLED */

/**
 * Constant times x plus y, over galois field. Naming convention for this
 * function is borrowed from BLAS routines.
 */
static void gfaxpy(struct m0_bufvec *y, struct m0_bufvec *x,
		   m0_parity_elem_t alpha);

#if !ISAL_ENCODE_ENABLED
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
#endif /* !ISAL_ENCODE_ENABLED */
static inline bool is_valid_block_idx(const  struct m0_sns_ir *ir,
				      uint32_t block_idx);

#if !ISAL_ENCODE_ENABLED
static bool is_data(const struct m0_sns_ir *ir, uint32_t index);
#endif /* !ISAL_ENCODE_ENABLED */

static bool is_usable(const struct m0_sns_ir *ir,
		      const struct m0_bitmap *in_bmap,
		      struct m0_sns_ir_block *failed_block);

static uint32_t last_usable_block_id(const struct m0_sns_ir *ir,
				     uint32_t block_idx);

#if !ISAL_ENCODE_ENABLED
static inline  bool are_failures_mixed(const struct m0_sns_ir *ir);

static inline const struct m0_matrix* recovery_mat_get(const struct m0_sns_ir
						       *ir,
						       uint32_t failed_idx);
#endif /* !ISAL_ENCODE_ENABLED */

static inline uint32_t ir_blocks_count(const struct m0_sns_ir *ir);

#if ISAL_ENCODE_ENABLED
/**
 * Initialize fields required for incremental recovery using Intel ISA.
 * @param[in] ir       - Pointer to incremental recovery structure.
 * @retval    0          Success
 * @retval    -ENOMEM    Failed to allocate memory
 */
static int isal_ir_init(struct m0_sns_ir *ir);

/**
 * Free fields initialized for incremental recovery using Intel ISA.
 * @param[in] ir - pointer to incremental recovery structure.
 */
static void isal_ir_fini(struct m0_sns_ir *ir);
#endif /* ISAL_ENCODE_ENABLED */

static void (*calculate[M0_PARITY_CAL_ALGO_NR])(struct m0_parity_math *math,
						const struct m0_buf *data,
						struct m0_buf *parity) = {
	[M0_PARITY_CAL_ALGO_XOR] = xor_calculate,
#if ISAL_ENCODE_ENABLED
	[M0_PARITY_CAL_ALGO_ISA] = isal_encode,
#else
	[M0_PARITY_CAL_ALGO_REED_SOLOMON] = reed_solomon_encode,
#endif /* ISAL_ENCODE_ENABLED */
};

static void (*diff[M0_PARITY_CAL_ALGO_NR])(struct m0_parity_math *math,
					   struct m0_buf         *old,
					   struct m0_buf         *new,
					   struct m0_buf         *parity,
					   uint32_t               index) = {
	[M0_PARITY_CAL_ALGO_XOR]          = xor_diff,
#if ISAL_ENCODE_ENABLED
	[M0_PARITY_CAL_ALGO_ISA] = isal_diff,
#else
	[M0_PARITY_CAL_ALGO_REED_SOLOMON] = reed_solomon_diff,
#endif /* ISAL_ENCODE_ENABLED */
};

static void (*recover[M0_PARITY_CAL_ALGO_NR])(struct m0_parity_math *math,
					      struct m0_buf *data,
					      struct m0_buf *parity,
					      struct m0_buf *fails,
					      enum m0_parity_linsys_algo algo) = {
	[M0_PARITY_CAL_ALGO_XOR] = xor_recover,
#if ISAL_ENCODE_ENABLED
	[M0_PARITY_CAL_ALGO_ISA] = isal_recover,
#else
	[M0_PARITY_CAL_ALGO_REED_SOLOMON] = reed_solomon_recover,
#endif /* ISAL_ENCODE_ENABLED */
};

static void (*fidx_recover[M0_PARITY_CAL_ALGO_NR])(struct m0_parity_math *math,
						   struct m0_buf *data,
						   struct m0_buf *parity,
						   const uint32_t fidx) = {
	[M0_PARITY_CAL_ALGO_XOR] = fail_idx_xor_recover,
#if ISAL_ENCODE_ENABLED
	[M0_PARITY_CAL_ALGO_ISA] = fail_idx_isal_recover,
#else
	[M0_PARITY_CAL_ALGO_REED_SOLOMON] = fail_idx_reed_solomon_recover,
#endif /* ISAL_ENCODE_ENABLED */
};

enum {
	SNS_PARITY_MATH_DATA_BLOCKS_MAX = 1 << (M0_PARITY_GALOIS_W - 1),
	BAD_FAIL_INDEX = -1
};

/* m0_parity_* are to much eclectic. just more simple names. */
static int gadd(int x, int y)
{
	return m0_parity_add(x, y);
}

#if !ISAL_ENCODE_ENABLED
static int gsub(int x, int y)
{
	return m0_parity_sub(x, y);
}
#endif /* !ISAL_ENCODE_ENABLED */

static int gmul(int x, int y)
{
	return m0_parity_mul(x, y);
}

#if !ISAL_ENCODE_ENABLED
static int gdiv(int x, int y)
{
	return m0_parity_div(x, y);
}

static int gpow(int x, int p)
{
	return m0_parity_pow(x, p);
}

/* Fills vandermonde matrix with initial values. */
static int vandmat_init(struct m0_matrix *m, uint32_t data_count,
			uint32_t parity_count)
{
	int ret;
	uint32_t y;
	uint32_t x;
	uint32_t mat_height = data_count + parity_count;
	uint32_t mat_width = data_count;

	ret = m0_matrix_init(m, mat_width, mat_height);
	if (ret < 0)
		return ret;

	for (y = 0; y < mat_height; ++y)
		for (x = 0; x < mat_width; ++x)
			*m0_matrix_elem_get(m, x, y) = gpow(y, x);

	return ret;
}

static void vandmat_fini(struct m0_matrix *mat)
{
	m0_matrix_fini(mat);
}

/* Checks if row has only one element equals 1, and 0 others */
static bool check_row_is_id(struct m0_matrix *m, uint32_t row)
{
	bool ret = true;
	uint32_t x;

	for (x = 0; x < m->m_width && ret; ++x)
		ret &= (row == x) == *m0_matrix_elem_get(m, x, row);

	return ret;
}

/* Normalises vandermonde matrix, upper part of which becomes identity matrix
 * in case of success. */
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
#endif /* !ISAL_ENCODE_ENABLED */

#if ISAL_ENCODE_ENABLED
static bool parity_math_invariant(const struct m0_parity_math *math)
{
	return  _0C(math != NULL) && _0C(math->pmi_data_count >= 1) &&
		_0C(math->pmi_parity_count >= 1) &&
		_0C(math->pmi_data_count >= math->pmi_parity_count) &&
		_0C(math->pmi_data_count <= SNS_PARITY_MATH_DATA_BLOCKS_MAX);
}

static int isal_encode_data_update(struct m0_buf *dest_buf, struct m0_buf *src_buf,
				   uint32_t vec_idx, uint8_t *g_tbls,
				   uint32_t data_nr, uint32_t dest_nr)
{
	uint32_t i;
	uint32_t block_size;
	int	 ret = 0;

	M0_ENTRY("dest_buf=%p, src_buf=%p, vec_idx=%u, "
		 "g_tbls=%p, data_nr=%u, dest_nr=%u",
		 dest_buf, src_buf, vec_idx, g_tbls, data_nr, dest_nr);

	M0_PRE(dest_buf != NULL);
	M0_PRE(src_buf != NULL);
	M0_PRE(g_tbls != NULL);

	uint8_t	 *dest_frags[dest_nr];

	block_size = (uint32_t)src_buf->b_nob;

	for (i = 0; i < dest_nr; ++i) {
		if (block_size != dest_buf[i].b_nob) {
			ret = M0_ERR_INFO(-EINVAL, "dest block size mismatch. "
					  "block_size = %u, "
					  "dest_buf[%u].b_nob=%u",
					  block_size, i,
					  (uint32_t)dest_buf[i].b_nob);
			return M0_RC(ret);
		}
		dest_frags[i] = (uint8_t *)dest_buf[i].b_addr;
	}

	ec_encode_data_update(block_size, data_nr, dest_nr, vec_idx,
			      g_tbls, (uint8_t *)src_buf->b_addr, dest_frags);

	return M0_RC(ret);
}
#endif /* ISAL_ENCODE_ENABLED */

M0_INTERNAL void m0_parity_math_fini(struct m0_parity_math *math)
{
	M0_ENTRY();
#if ISAL_ENCODE_ENABLED
	if (math->pmi_parity_algo == M0_PARITY_CAL_ALGO_ISA) {
		m0_free(math->pmi_encode_matrix);
		m0_free(math->pmi_encode_tbls);
		m0_free(math->pmi_decode_tbls);
	}
#else
	if (math->pmi_parity_algo == M0_PARITY_CAL_ALGO_REED_SOLOMON) {
		vandmat_fini(&math->pmi_vandmat);
		m0_matrix_fini(&math->pmi_vandmat_parity_slice);
		m0_matvec_fini(&math->pmi_data);
		m0_matvec_fini(&math->pmi_parity);

		m0_linsys_fini(&math->pmi_sys);
		m0_matrix_fini(&math->pmi_sys_mat);
		m0_matvec_fini(&math->pmi_sys_vec);
		m0_matvec_fini(&math->pmi_sys_res);
	}
#endif /* ISAL_ENCODE_ENABLED */
	M0_LEAVE();
}

M0_INTERNAL int m0_parity_math_init(struct m0_parity_math *math,
				    uint32_t data_count, uint32_t parity_count)
{
	int ret = 0;

	M0_PRE(data_count >= 1);
	M0_PRE(parity_count >= 1);
	M0_PRE(data_count >= parity_count);
	M0_PRE(data_count <= SNS_PARITY_MATH_DATA_BLOCKS_MAX);

	M0_SET0(math);

	M0_ENTRY("data_count=%u parity_count=%u", data_count, parity_count);

	math->pmi_data_count	= data_count;
	math->pmi_parity_count	= parity_count;

	if (parity_count == 1) {
		math->pmi_parity_algo = M0_PARITY_CAL_ALGO_XOR;
		return M0_RC(ret);
	}
#if ISAL_ENCODE_ENABLED
	uint32_t total_count = data_count + parity_count;
	uint32_t tbl_len = data_count * parity_count * 32;

	math->pmi_parity_algo = M0_PARITY_CAL_ALGO_ISA;

	ALLOC_ARR_INFO(math->pmi_encode_matrix, (total_count * data_count),
		       "encode matrix", ret);
	if (math->pmi_encode_matrix == NULL)
		goto handle_error;

	ALLOC_ARR_INFO(math->pmi_encode_tbls, tbl_len,
		       "encode coefficient tables", ret);
	if (math->pmi_encode_tbls == NULL)
		goto handle_error;

	ALLOC_ARR_INFO(math->pmi_decode_tbls, tbl_len,
		       "decode coefficient tables", ret);
	if (math->pmi_decode_tbls == NULL)
		goto handle_error;

	/* Generate a matrix of coefficients to be used for encoding. */
	gf_gen_rs_matrix(math->pmi_encode_matrix, total_count, data_count);

	/* Initialize tables for fast Erasure Code encode. */
	ec_init_tables(data_count, parity_count,
		       &math->pmi_encode_matrix[data_count * data_count],
		       math->pmi_encode_tbls);

#else
	math->pmi_parity_algo = M0_PARITY_CAL_ALGO_REED_SOLOMON;

	ret = vandmat_init(&math->pmi_vandmat, data_count, parity_count);
	if (ret < 0)
		goto handle_error;

	ret = vandmat_norm(&math->pmi_vandmat);
	if (ret < 0)
		goto handle_error;

	ret = m0_matrix_init(&math->pmi_vandmat_parity_slice,
			     data_count, parity_count);
	if (ret < 0)
		goto handle_error;

	m0_matrix_submatrix_get(&math->pmi_vandmat,
				&math->pmi_vandmat_parity_slice, 0,
				data_count);

	ret = m0_matvec_init(&math->pmi_data, data_count);
	if (ret < 0)
		goto handle_error;

	ret = m0_matvec_init(&math->pmi_parity, parity_count);
	if (ret < 0)
		goto handle_error;

	ret = m0_matvec_init(&math->pmi_sys_vec, math->pmi_data.mv_size);
	if (ret < 0)
		goto handle_error;

	ret = m0_matvec_init(&math->pmi_sys_res, math->pmi_data.mv_size);
	if (ret < 0)
		goto handle_error;

	ret = m0_matrix_init(&math->pmi_sys_mat, math->pmi_data.mv_size,
			     math->pmi_data.mv_size);
	if (ret < 0)
		goto handle_error;
#endif /* ISAL_ENCODE_ENABLED */

	return M0_RC(ret);
 handle_error:
	m0_parity_math_fini(math);

	return M0_RC(ret);
}

static void xor_calculate(struct m0_parity_math *math,
			  const struct m0_buf *data,
			  struct m0_buf *parity)
{
        uint32_t          ei; /* block element index. */
        uint32_t          ui; /* unit index. */
        uint32_t          block_size = data[0].b_nob;
	m0_parity_elem_t  pe;

	M0_PRE(block_size == parity[0].b_nob);
	for (ui = 1; ui < math->pmi_data_count; ++ui)
		M0_PRE(block_size == data[ui].b_nob);

	for (ei = 0; ei < block_size; ++ei) {
		pe = 0;
		for (ui = 0; ui < math->pmi_data_count; ++ui)
			pe ^= (m0_parity_elem_t)((uint8_t*)data[ui].b_addr)[ei];

		((uint8_t*)parity[0].b_addr)[ei] = pe;
	}

}

#if ISAL_ENCODE_ENABLED
static void isal_diff(struct m0_parity_math *math,
		      struct m0_buf         *old,
		      struct m0_buf         *new,
		      struct m0_buf         *parity,
		      uint32_t               index)
{
	struct m0_buf	diff_data_buf;
	uint8_t	       *diff_data = NULL;
	uint32_t	block_size;
	uint32_t	alignment = sizeof(uint_fast32_t);
	uint32_t	i;
	int		ret = 0;

	M0_ENTRY("math=%p, old=%p, new=%p, parity=%p, index=%u",
		 math, old, new, parity, index);

	M0_PRE(parity_math_invariant(math));
	M0_PRE(old    != NULL);
	M0_PRE(new    != NULL);
	M0_PRE(parity != NULL);
	M0_PRE(index  <  math->pmi_data_count);
	M0_PRE(old[index].b_nob == new[index].b_nob);

	block_size = new[index].b_nob;

	/* It is assumed that the buffer size will always be a multiple of
	 * 8-bytes (especially since block size currently is 4K) and this assert
	 * is added with the hope that any deviation from this assumption is
	 * caught during development instead of on the field.
	 */
	M0_ASSERT_INFO(m0_is_aligned(block_size, alignment),
		       "block_size=%u is not %u-bytes aligned",
		       block_size, alignment);

	ALLOC_ARR_INFO(diff_data, block_size, "differential data block", ret);
	if (diff_data == NULL)
		goto fini;

	m0_buf_init(&diff_data_buf, diff_data, block_size);

	/* Calculate differential data */
	for (i = 0; i < (block_size / alignment); i++)
		((uint_fast32_t *)diff_data)[i] =
			((uint_fast32_t *)old[index].b_addr)[i] ^
			((uint_fast32_t *)new[index].b_addr)[i];

	/* Update differential parity */
	isal_encode_data_update(parity, &diff_data_buf, index,
				math->pmi_encode_tbls, math->pmi_data_count,
				math->pmi_parity_count);

fini:
	m0_free(diff_data);

	/* TODO: Return error code instead of assert */
	M0_ASSERT(ret == 0);

	M0_LEAVE();
}
#endif /* ISAL_ENCODE_ENABLED */

#if !ISAL_ENCODE_ENABLED
static void reed_solomon_diff(struct m0_parity_math *math,
			      struct m0_buf         *old,
			      struct m0_buf         *new,
			      struct m0_buf         *parity,
			      uint32_t               index)
{
	struct m0_matrix *mat;
	uint32_t          ei;
	uint32_t          ui;
	uint8_t		  diff_data;
	m0_parity_elem_t  mat_elem;

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
}
#endif /* !ISAL_ENCODE_ENABLED */

static void xor_diff(struct m0_parity_math *math,
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
}

#if !ISAL_ENCODE_ENABLED
static void reed_solomon_encode(struct m0_parity_math *math,
				const struct m0_buf *data,
				struct m0_buf *parity)
{
#define PARITY_MATH_REGION_ENABLE 0

#if !PARITY_MATH_REGION_ENABLE
	uint32_t	  ei; /* block element index. */
#endif
	uint32_t	  pi; /* parity unit index. */
	uint32_t	  di; /* data unit index. */
	m0_parity_elem_t  mat_elem;
	uint32_t	  block_size = data[0].b_nob;


	for (di = 1; di < math->pmi_data_count; ++di)
		M0_ASSERT(block_size == data[di].b_nob);

	for (pi = 0; pi < math->pmi_parity_count; ++pi)
		M0_ASSERT(block_size == parity[pi].b_nob);

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
}
#endif /* !ISAL_ENCODE_ENABLED */

#if ISAL_ENCODE_ENABLED
static void isal_encode(struct m0_parity_math *math,
			const struct m0_buf *data,
			struct m0_buf *parity)
{
	uint32_t  i;
	uint32_t  block_size;
	int	  ret = 0;

	M0_ENTRY();

	M0_PRE(parity_math_invariant(math));
	M0_PRE(data != NULL);
	M0_PRE(parity != NULL);

	uint8_t  *frags_in[math->pmi_data_count];
	uint8_t  *frags_out[math->pmi_parity_count];

	block_size = data[0].b_nob;

	frags_in[0] = (uint8_t *)data[0].b_addr;
	for (i = 1; i < math->pmi_data_count; ++i) {
		if (block_size != data[i].b_nob) {
			ret = M0_ERR_INFO(-EINVAL, "data block size mismatch. "
					  "block_size = %u, data[%u].b_nob=%u",
					  block_size, i, (uint32_t)data[i].b_nob);
			goto fini;
		}
		frags_in[i] = (uint8_t *)data[i].b_addr;
	}

	for (i = 0; i < math->pmi_parity_count; ++i) {
		if (block_size != parity[i].b_nob) {
			ret = M0_ERR_INFO(-EINVAL, "parity block size mismatch. "
					  "block_size = %u, parity[%u].b_nob=%u",
					  block_size, i, (uint32_t)parity[i].b_nob);
			goto fini;
		}
		frags_out[i] = (uint8_t *)parity[i].b_addr;
	}

	/* Generate erasure codes on given blocks of data. */
	ec_encode_data(block_size, math->pmi_data_count, math->pmi_parity_count,
		       math->pmi_encode_tbls, frags_in, frags_out);

fini:
	/* TODO: Return error code instead of assert */
	M0_ASSERT(ret == 0);

	M0_LEAVE();
}
#endif /* ISAL_ENCODE_ENABLED */

M0_INTERNAL void m0_parity_math_calculate(struct m0_parity_math *math,
					  struct m0_buf *data,
					  struct m0_buf *parity)
{
	M0_ENTRY();
	(*calculate[math->pmi_parity_algo])(math, data, parity);
	M0_LEAVE();
}

M0_INTERNAL void m0_parity_math_diff(struct m0_parity_math *math,
				     struct m0_buf *old,
				     struct m0_buf *new,
				     struct m0_buf *parity, uint32_t index)
{
	M0_ENTRY();
	(*diff[math->pmi_parity_algo])(math, old, new, parity, index);
	M0_LEAVE();
}

M0_INTERNAL void m0_parity_math_refine(struct m0_parity_math *math,
				       struct m0_buf *data,
				       struct m0_buf *parity,
				       uint32_t data_ind_changed)
{
	M0_ENTRY();
	/* for simplicity: */
	m0_parity_math_calculate(math, data, parity);
	M0_LEAVE();
}

/* Counts number of failed blocks. */
static uint32_t fails_count(uint8_t *fail, uint32_t unit_count)
{
	uint32_t x;
	uint32_t count = 0;

	for (x = 0; x < unit_count; ++x)
		count += !!fail[x];

	return count;
}

#if !ISAL_ENCODE_ENABLED
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
#endif /* !ISAL_ENCODE_ENABLED */

#if !ISAL_ENCODE_ENABLED
/* Fills 'mat' with data passed to recovery algorithm. */
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
#endif /* !ISAL_ENCODE_ENABLED */

#if !ISAL_ENCODE_ENABLED
/* Updates internal structures of 'math' with recovered data. */
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
#endif /* !ISAL_ENCODE_ENABLED */

#if !ISAL_ENCODE_ENABLED
M0_INTERNAL int m0_parity_recov_mat_gen(struct m0_parity_math *math,
					uint8_t *fail)
{
	int rc;

	recovery_mat_fill(math, fail, math->pmi_data_count + math->pmi_parity_count,
			  &math->pmi_sys_mat);
	m0_matrix_init(&math->pmi_recov_mat, math->pmi_sys_mat.m_width,
		       math->pmi_sys_mat.m_height);
	rc = m0_matrix_invert(&math->pmi_sys_mat, &math->pmi_recov_mat);

	return rc == 0 ? M0_RC(0) : M0_ERR(rc);
}
#endif /* !ISAL_ENCODE_ENABLED */

#if !ISAL_ENCODE_ENABLED
M0_INTERNAL void m0_parity_recov_mat_destroy(struct m0_parity_math *math)
{
	m0_matrix_fini(&math->pmi_recov_mat);
}
#endif /* !ISAL_ENCODE_ENABLED */

static void xor_recover(struct m0_parity_math *math,
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
	int		  fail_index = BAD_FAIL_INDEX;

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
}

#if ISAL_ENCODE_ENABLED
static int isal_gen_recov_coeff_tbl(uint32_t data_count, uint32_t parity_count,
				    struct m0_buf *failed_idx_buf,
				    struct m0_buf *alive_idx_buf,
				    uint8_t *encode_mat, uint8_t *g_tbls)
{
	uint8_t  *decode_mat = NULL;
	uint8_t  *temp_mat = NULL;
	uint8_t  *invert_mat = NULL;
	uint32_t  unit_count;
	uint32_t  i;
	uint32_t  j;
	uint32_t  r;
	uint8_t	  s;
	uint8_t	  idx;
	int	  ret = 0;

	M0_ENTRY("data_count=%u, parity_count=%u, failed_idx_buf=%p, "
		 "alive_idx_buf=%p, encode_mat=%p, g_tbls=%p",
		 data_count, parity_count, failed_idx_buf, alive_idx_buf,
		 encode_mat, g_tbls);

	unit_count = data_count + parity_count;

	ALLOC_ARR_INFO(decode_mat, (unit_count * data_count),
		       "decode matrix", ret);
	if (decode_mat == NULL)
		goto fini;

	ALLOC_ARR_INFO(temp_mat, (unit_count * data_count),
		       "temp matrix", ret);
	if (temp_mat == NULL)
		goto fini;

	ALLOC_ARR_INFO(invert_mat, (unit_count * data_count),
		       "invert matrix", ret);
	if (invert_mat == NULL)
		goto fini;

	/* Construct temp_mat (matrix that encoded remaining frags)
	 * by removing erased rows
	 */
	for (i = 0; i < data_count; i++) {
		idx = ((uint8_t *)alive_idx_buf->b_addr)[i];
		for (j = 0; j < data_count; j++)
			temp_mat[data_count * i + j] =
				encode_mat[data_count * idx + j];
	}

	/* Invert matrix to get recovery matrix */
	ret = gf_invert_matrix(temp_mat, invert_mat, data_count);
	if (ret != 0) {
		ret = M0_ERR_INFO(ret, "failed to construct an %u x %u "
				  "inverse of the input matrix",
				  data_count, data_count);
		goto fini;
	}

	/* Create decode matrix */
	for (r = 0; r < failed_idx_buf->b_nob; r++) {
		idx = ((uint8_t *)failed_idx_buf->b_addr)[r];
		/* Get decode matrix with only wanted recovery rows */
		if (idx < data_count) {    /* A src err */
			for (i = 0; i < data_count; i++)
				decode_mat[data_count * r + i] =
					invert_mat[data_count * idx + i];
		}
		/* For non-src (parity) erasures need to multiply
		 * encode matrix * invert
		 */
		else { /* A parity err */
			for (i = 0; i < data_count; i++) {
				s = 0;
				for (j = 0; j < data_count; j++)
					s ^= gf_mul(invert_mat[j * data_count + i],
						    encode_mat[data_count * idx + j]);
				decode_mat[data_count * r + i] = s;
			}
		}
	}

	ec_init_tables(data_count, (uint32_t)failed_idx_buf->b_nob, decode_mat, g_tbls);

fini:
	m0_free(temp_mat);
	m0_free(invert_mat);
	m0_free(decode_mat);

	return M0_RC(ret);
}

static bool buf_sort(uint8_t **frags_in, uint8_t **frags_out,
		     uint32_t unit_count, uint32_t data_count,
		     uint8_t *fail, struct m0_buf *data,
		     struct m0_buf *parity)
{
	uint32_t  i;
	uint32_t  j;
	uint32_t  k;
	uint8_t  *addr;

	M0_ENTRY();

	if ((fail == NULL) || (frags_in == NULL) || (frags_out == NULL) ||
	    (data == NULL) || (parity == NULL))
		return M0_RC(false);

	for (i = 0, j = 0, k = 0; i < unit_count; i++) {
		if (i < data_count)
			addr = (uint8_t *)data[i].b_addr;
		else
			addr = (uint8_t *)parity[i - data_count].b_addr;

		if (fail[i] != 0)
			frags_out[j++] = addr;
		else if (k < data_count)
			frags_in[k++] = addr;
		else
			continue;
	}

	return M0_RC(true);
}

static bool fails_sort(uint8_t *fail, uint32_t unit_count,
		       struct m0_buf *failed_idx, struct m0_buf *alive_idx)
{
	uint32_t  i;
	uint8_t	 *failed_ids;
	uint8_t	 *alive_ids;

	M0_ENTRY();

	if ((fail == NULL) || (failed_idx == NULL) || (alive_idx == NULL) ||
	    (failed_idx->b_addr == NULL) || (alive_idx->b_addr == NULL))
		return M0_RC(false);

	failed_ids = (uint8_t *)failed_idx->b_addr;
	alive_ids = (uint8_t *)alive_idx->b_addr;

	failed_idx->b_nob = 0;
	alive_idx->b_nob = 0;
	for (i = 0; i < unit_count; i++) {
		if (fail[i] != 0)
			failed_ids[failed_idx->b_nob++] = i;
		else
			alive_ids[alive_idx->b_nob++] = i;
	}

	return M0_RC(true);
}

static void isal_recover(struct m0_parity_math *math,
			 struct m0_buf *data,
			 struct m0_buf *parity,
			 struct m0_buf *fails,
			 enum m0_parity_linsys_algo algo)
{
	struct m0_buf failed_idx_buf = M0_BUF_INIT0;
	struct m0_buf alive_idx_buf = M0_BUF_INIT0;
	uint32_t      fail_count;
	uint32_t      unit_count;
	uint32_t      block_size;
	uint32_t      i;
	uint8_t	     *fail = NULL;
	int	      ret = 0;

	M0_ENTRY();

	M0_PRE(parity_math_invariant(math));
	M0_PRE(data != NULL);
	M0_PRE(parity != NULL);
	M0_PRE(fails != NULL);

	uint8_t	     *frags_in[math->pmi_data_count];
	uint8_t	     *frags_out[math->pmi_parity_count];

	unit_count = math->pmi_data_count + math->pmi_parity_count;
	block_size = data[0].b_nob;
	fail = (uint8_t*) fails->b_addr;

	fail_count = fails_count(fail, unit_count);

	if ((fail_count == 0) || (fail_count > math->pmi_parity_count)) {
		ret = M0_ERR_INFO(-EINVAL, "Invalid fail count value. "
				  "fail_count = %u. Expected value "
				  "0 < fail_count <= %u", fail_count,
				  math->pmi_parity_count);
		goto fini;
	}

	/* Validate block size for data buffers */
	for (i = 1; i < math->pmi_data_count; ++i) {
		if (block_size != data[i].b_nob) {
			ret = M0_ERR_INFO(-EINVAL, "data block size mismatch. "
					  "block_size = %u, data[%u].b_nob=%u",
					  block_size, i, (uint32_t)data[i].b_nob);
			goto fini;
		}
	}

	/* Validate block size for parity buffers */
	for (i = 0; i < math->pmi_parity_count; ++i) {
		if (block_size != parity[i].b_nob) {
			ret = M0_ERR_INFO(-EINVAL, "parity block size mismatch. "
					  "block_size = %u, parity[%u].b_nob=%u",
					  block_size, i, (uint32_t)parity[i].b_nob);
			goto fini;
		}
	}

	ret = m0_buf_alloc(&failed_idx_buf, math->pmi_parity_count);
	if (ret != 0) {
		ret = M0_ERR_INFO(ret, "failed to allocate memory for "
				  "array of failed ids");
		goto fini;
	}

	ret = m0_buf_alloc(&alive_idx_buf, unit_count);
	if (ret != 0) {
		ret = M0_ERR_INFO(ret, "failed to allocate memory for "
				  "array of alive ids");
		goto fini;
	}

	/* Sort failed buffer indices */
	if (fails_sort(fail, unit_count, &failed_idx_buf,
		       &alive_idx_buf) == false) {
		ret = M0_ERR_INFO(-EINVAL, "failed to sort failed ids");
		goto fini;
	}

	if (fail_count != failed_idx_buf.b_nob) {
		ret = M0_ERR_INFO(-EINVAL, "failed count mismatch. "
				  "fail_count=%d, failed_idx_buf.b_nob=%d",
				  fail_count, (uint32_t)failed_idx_buf.b_nob);
		goto fini;
	}

	/* Sort buffers which are to be recovered */
	if (buf_sort(frags_in, frags_out, unit_count, math->pmi_data_count,
		     fail, data, parity) == false) {
		ret = M0_ERR_INFO(-EINVAL, "failed to sort buffers to be "
				  "recovered");
		goto fini;
	}

	/* Get encoding coefficient tables */
	ret = isal_gen_recov_coeff_tbl(math->pmi_data_count,
				       math->pmi_parity_count,
				       &failed_idx_buf,
				       &alive_idx_buf,
				       math->pmi_encode_matrix,
				       math->pmi_decode_tbls);
	if (ret != 0) {
		ret = M0_ERR_INFO(ret, "failed to generate recovery "
				  "coefficient tables");
		goto fini;
	}

	/* Recover data */
	ec_encode_data(block_size, math->pmi_data_count, fail_count,
		       math->pmi_decode_tbls, frags_in, frags_out);

fini:
	m0_buf_free(&failed_idx_buf);
	m0_buf_free(&alive_idx_buf);

	/* TODO: Return error code instead of assert */
	M0_ASSERT(ret == 0);

	M0_LEAVE();
}
#endif /* ISAL_ENCODE_ENABLED */

#if !ISAL_ENCODE_ENABLED
static void reed_solomon_recover(struct m0_parity_math *math,
				 struct m0_buf *data,
				 struct m0_buf *parity,
				 struct m0_buf *fails,
				 enum m0_parity_linsys_algo algo)
{
	uint32_t ei; /* block element index. */
	uint32_t ui; /* unit index. */
	uint8_t *fail;
	uint32_t fail_count;
	uint32_t unit_count = math->pmi_data_count + math->pmi_parity_count;
	uint32_t block_size = data[0].b_nob;

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
}
#endif /* !ISAL_ENCODE_ENABLED */

M0_INTERNAL void m0_parity_math_recover(struct m0_parity_math *math,
					struct m0_buf *data,
					struct m0_buf *parity,
					struct m0_buf *fails,
					enum m0_parity_linsys_algo algo)
{
	M0_ENTRY();
	(*recover[math->pmi_parity_algo])(math, data, parity, fails, algo);
	M0_LEAVE();
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

#if !ISAL_ENCODE_ENABLED
/** @todo Iterative reed-solomon decode to be implemented. */
static void fail_idx_reed_solomon_recover(struct m0_parity_math *math,
					  struct m0_buf *data,
					  struct m0_buf *parity,
					  const uint32_t failure_index)
{
}
#endif /* !ISAL_ENCODE_ENABLED */

#if ISAL_ENCODE_ENABLED
static void fail_idx_isal_recover(struct m0_parity_math *math,
				  struct m0_buf *data,
				  struct m0_buf *parity,
				  const uint32_t failure_index)
{
	M0_ERR_INFO(-ENOSYS, "Recover using failed index is not implemented "
		    "for Intel ISA");
	M0_ASSERT(0);
}
#endif /* ISAL_ENCODE_ENABLED */

M0_INTERNAL void m0_parity_math_fail_index_recover(struct m0_parity_math *math,
						   struct m0_buf *data,
						   struct m0_buf *parity,
						   const uint32_t fidx)
{
	M0_ENTRY();
	(*fidx_recover[math->pmi_parity_algo])(math, data, parity, fidx);
	M0_LEAVE();
}

M0_INTERNAL void m0_parity_math_buffer_xor(struct m0_buf *dest,
					   const struct m0_buf *src)
{
        uint32_t ei; /* block element index. */

        for (ei = 0; ei < src[0].b_nob; ++ei)
		((uint8_t*)dest[0].b_addr)[ei] ^=
			(m0_parity_elem_t)((uint8_t*)src[0].b_addr)[ei];
}

#if ISAL_ENCODE_ENABLED
static int isal_ir_init(struct m0_sns_ir *ir)
{
	int ret;

	M0_ENTRY("ir=%p", ir);
	M0_PRE(ir != NULL);

	/* Allocate memory to hold indices of alive blocks */
	ALLOC_ARR_INFO(ir->si_alive_idx, ir->si_alive_nr,
		       "alive index buffer", ret);
	if (ir->si_alive_idx == NULL)
		return M0_RC(ret);

	/* Allocate memory to hold indices of failed blocks */
	ALLOC_ARR_INFO(ir->si_failed_idx, ir->si_parity_nr,
		       "failed index buffer", ret);
	if (ir->si_failed_idx == NULL)
		return M0_RC(ret);

	ir->si_failed_nr = 0;

	return M0_RC(ret);
}

static void isal_ir_fini(struct m0_sns_ir *ir)
{
	m0_free(ir->si_alive_idx);
	m0_free(ir->si_failed_idx);
}
#endif /* ISAL_ENCODE_ENABLED */

M0_INTERNAL int m0_sns_ir_init(const struct m0_parity_math *math,
			       uint32_t local_nr, struct m0_sns_ir *ir)
{
	uint32_t i = 0;
	int	 ret = 0;

	M0_ENTRY("math=%p, local_nr=%u, ir=%p", math, local_nr, ir);

	M0_PRE(math != NULL);
	M0_PRE(ir != NULL);
#if ISAL_ENCODE_ENABLED
	M0_PRE(math->pmi_encode_matrix != NULL);
	M0_PRE(math->pmi_decode_tbls != NULL);
#endif /* ISAL_ENCODE_ENABLED */

	M0_SET0(ir);
	ir->si_data_nr		   = math->pmi_data_count;
	ir->si_parity_nr	   = math->pmi_parity_count;
	ir->si_local_nr		   = local_nr;
#if !ISAL_ENCODE_ENABLED
	ir->si_vandmat		   = math->pmi_vandmat;
	ir->si_parity_recovery_mat = math->pmi_vandmat_parity_slice;
	ir->si_failed_data_nr	   = 0;
#endif /* !ISAL_ENCODE_ENABLED */
	ir->si_alive_nr		   = ir_blocks_count(ir);

#if ISAL_ENCODE_ENABLED
	/* Get encode matrix */
	ir->si_encode_matrix = math->pmi_encode_matrix;

	/* Get decode table */
	ir->si_decode_tbls = math->pmi_decode_tbls;

	/* Initialize required buffers */
	ret = isal_ir_init(ir);
	if (ret != 0)
		goto fini;

#endif /* ISAL_ENCODE_ENABLED */

	ALLOC_ARR_INFO(ir->si_blocks, ir->si_alive_nr,
		       "blocks", ret);
	if (ir->si_blocks == NULL)
		goto fini;

	for (i = 0; i < ir_blocks_count(ir); ++i) {
		ir->si_blocks[i].sib_idx = i;
		ir->si_blocks[i].sib_status = M0_SI_BLOCK_ALIVE;
#if !ISAL_ENCODE_ENABLED
		ir->si_blocks[i].sib_data_recov_mat_col = IR_INVALID_COL;
#endif /* !ISAL_ENCODE_ENABLED */
		ret = m0_bitmap_init(&ir->si_blocks[i].sib_bitmap,
				     ir_blocks_count(ir));
		if (ret != 0){
			ret = M0_ERR_INFO(ret, "failed to initialize bitmap for"
			                  "ir->si_blocks[%u].sib_bitmap", i);
			goto fini;
		}
	}
	goto end;

fini:
	m0_sns_ir_fini(ir);

end:
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
#if ISAL_ENCODE_ENABLED
	/* Store failed index in buffer and increase the failure number */
	ir->si_failed_idx[ir->si_failed_nr++] = failed_index;
#else
	if (is_data(ir, failed_index))
		M0_CNT_INC(ir->si_failed_data_nr);
#endif /* ISAL_ENCODE_ENABLED */

	return (ir->si_alive_nr < ir->si_data_nr) ? M0_ERR(-EDQUOT) : M0_RC(0);
}

M0_INTERNAL int m0_sns_ir_mat_compute(struct m0_sns_ir *ir)
{
	int			ret = 0;
	uint32_t		i;
	uint32_t		j;
	uint32_t		total_blocks_nr;
	struct m0_sns_ir_block *blocks;

	M0_ENTRY("ir=%p", ir);
	M0_PRE(ir != NULL);

	blocks = ir->si_blocks;
	total_blocks_nr = ir_blocks_count(ir);

#if !ISAL_ENCODE_ENABLED
	if (ir->si_failed_data_nr != 0) {
		for (j = 0, i = 0; j < total_blocks_nr && i < ir->si_data_nr;
		     ++j) {
			if (blocks[j].sib_status == M0_SI_BLOCK_ALIVE) {
				blocks[j].sib_data_recov_mat_col = i;
				++i;
			}
		}
		ret = data_recov_mat_construct(ir);
		if (ret != 0)
			return ret;
	}

	for (j = 0, i = 0; j < total_blocks_nr; ++j) {
		if (blocks[j].sib_status == M0_SI_BLOCK_FAILED) {
			blocks[j].sib_recov_mat_row = is_data(ir, j) ? i :
				j - ir->si_data_nr;
			dependency_bitmap_prepare(&blocks[j], ir);
			++i;
		}
	}
#else
	for (i = 0, j = 0; i < total_blocks_nr; i++)
		if (blocks[i].sib_status == M0_SI_BLOCK_ALIVE)
			ir->si_alive_idx[j++] = blocks[i].sib_idx;

	ret = ir_gen_decode_matrix(ir);
	if (ret != 0)
		return M0_RC(ret);

	for (i = 0; i < ir->si_failed_nr; i++) {
		dependency_bitmap_prepare(&blocks[ir->si_failed_idx[i]], ir);
	}
#endif /* !ISAL_ENCODE_ENABLED */
	return M0_RC(ret);
}

#if ISAL_ENCODE_ENABLED
static int ir_gen_decode_matrix(struct m0_sns_ir *ir)
{
	struct m0_buf failed_idx_buf = M0_BUF_INIT0;
	struct m0_buf alive_idx_buf = M0_BUF_INIT0;
	int	      ret;

	M0_ENTRY("ir=%p", ir);
	M0_PRE(ir != NULL);

	m0_buf_init(&failed_idx_buf, ir->si_failed_idx, ir->si_failed_nr);
	m0_buf_init(&alive_idx_buf, ir->si_alive_idx, ir->si_alive_nr);

	/* Get encoding coefficient tables */
	ret = isal_gen_recov_coeff_tbl(ir->si_data_nr,
				       ir->si_parity_nr,
				       &failed_idx_buf,
				       &alive_idx_buf,
				       ir->si_encode_matrix,
				       ir->si_decode_tbls);
	if (ret != 0) {
		ret = M0_ERR_INFO(ret, "failed to generate recovery "
				  "coefficient tables");
	}

	return M0_RC(ret);
}
#endif /* ISAL_ENCODE_ENABLED */

#if !ISAL_ENCODE_ENABLED
static int data_recov_mat_construct(struct m0_sns_ir *ir)
{
	int		 ret = 0;
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
	return ret;
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
#endif /* !ISAL_ENCODE_ENABLED */

static void dependency_bitmap_prepare(struct m0_sns_ir_block *f_block,
				      struct m0_sns_ir *ir)
{
	uint32_t i;

#if !ISAL_ENCODE_ENABLED
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
#else /* ISAL_ENCODE_ENABLED */
	for (i = 0; i < ir->si_data_nr; ++i)
		m0_bitmap_set(&f_block->sib_bitmap, ir->si_alive_idx[i], true);
#endif /* ISAL_ENCODE_ENABLED */
}

#if !ISAL_ENCODE_ENABLED
static inline uint32_t recov_mat_col(const struct m0_sns_ir_block *alive_block,
				     const struct m0_sns_ir_block *failed_block,
				     const struct m0_sns_ir *ir)
{
	M0_PRE(alive_block != NULL);
	M0_PRE(failed_block != NULL);
	return is_data(ir, failed_block->sib_idx) ?
		alive_block->sib_data_recov_mat_col : alive_block->sib_idx;
}
#endif /* !ISAL_ENCODE_ENABLED */

M0_INTERNAL void m0_sns_ir_fini(struct m0_sns_ir *ir)
{
	uint32_t j;

	M0_ENTRY("ir=%p", ir);
	M0_PRE(ir != NULL);

	for (j = 0; j < ir_blocks_count(ir); ++j) {
		if (ir->si_blocks[j].sib_bitmap.b_words != NULL)
			m0_bitmap_fini(&ir->si_blocks[j].sib_bitmap);
	}

#if !ISAL_ENCODE_ENABLED
	m0_matrix_fini(&ir->si_data_recovery_mat);
#else /* ISAL_ENCODE_ENABLED */
	isal_ir_fini(ir);
#endif /* ISAL_ENCODE_ENABLED */
	m0_free(ir->si_blocks);
	M0_LEAVE();
}

#if ISAL_ENCODE_ENABLED
static int bufvec_to_buf_copy(struct m0_buf *bufs,
			      struct m0_bufvec **bvecs,
			      uint32_t count)
{
	struct m0_bufvec_cursor	cursor;
	uint32_t		i;
	int			ret = 0;

	M0_ENTRY("copy count=%u", count);

	for (i = 0; i < count; ++i) {
		m0_bufvec_cursor_init(&cursor, bvecs[i]);
		ret = m0_bufvec_to_data_copy(&cursor, bufs[i].b_addr,
					     (size_t)bufs[i].b_nob);
		if (ret != 0)
			return M0_ERR_INFO(ret, "Failed to copy data from "
					   "bufvec=%p to buf=%p. index=%u",
					   bvecs[i], &bufs[i], i);
	}

	return M0_RC(ret);
}

static int buf_to_bufvec_copy(struct m0_bufvec **bvecs,
			      const struct m0_buf *bufs,
			      uint32_t count)
{
	struct m0_bufvec_cursor	cursor;
	uint32_t		i;
	int			ret = 0;

	M0_ENTRY("copy count=%u", count);

	for (i = 0; i < count; ++i) {
		m0_bufvec_cursor_init(&cursor, bvecs[i]);
		ret = m0_data_to_bufvec_copy(&cursor, bufs[i].b_addr,
					     (size_t)bufs[i].b_nob);
		if (ret != 0)
			return M0_ERR_INFO(ret, "Failed to copy data from "
					   "buf=%p to bufvec=%p. index=%u",
					   bvecs[i], &bufs[i], i);
	}

	return M0_RC(ret);
}

static int ir_recover(struct m0_sns_ir *ir, struct m0_sns_ir_block *alive_block)
{
	struct m0_bitmap	*failed_bitmap;
	struct m0_bufvec       	*alive_bufvec;
	struct m0_bufvec	*failed_bufvecs[ir->si_failed_nr];
	struct m0_buf 		 alive_buf = M0_BUF_INIT0;
	struct m0_buf		 f_bufs[ir->si_failed_nr];
	uint8_t			 curr_idx = UINT8_MAX;
	uint32_t 		 i;
	uint32_t		 length;
	int			 ret = 0;

	M0_ENTRY("ir=%p, alive_block=%p", ir, alive_block);

	/* Check if given alive block is dependecy of any failed block */
	for (i = 0; i < ir->si_failed_nr; i++) {
		failed_bitmap = &ir->si_blocks[ir->si_failed_idx[i]].sib_bitmap;
		if (m0_bitmap_get(failed_bitmap, alive_block->sib_idx) == 0)
			return M0_RC(ret);
	}

	alive_bufvec = alive_block->sib_addr;
	length = alive_bufvec->ov_vec.v_count[0] * alive_bufvec->ov_vec.v_nr;

	memset(f_bufs, 0x00, ir->si_failed_nr * sizeof(struct m0_buf));

	for (i = 0; i < ir->si_failed_nr; i++) {
		ret = m0_buf_alloc(&f_bufs[i], length);
		if (ret != 0){
			ret = M0_ERR_INFO(ret, "Failed to allocate buffer %d "
					"of length = %u", i, length);
			goto exit;
		}
		/* Save address of m0_bufvec for failed block */
		failed_bufvecs[i] = ir->si_blocks[ir->si_failed_idx[i]].sib_addr;
	}

	/* Get data from failed vectors in buffers */
	ret = bufvec_to_buf_copy(f_bufs, failed_bufvecs, ir->si_failed_nr);
	if (ret != 0){
		ret = M0_ERR_INFO(ret, "Failed to get data from failed "
				  "vectors in buffers.");
		goto exit;
	}

	for (i = 0; i < ir->si_alive_nr; i++) {
		if(ir->si_alive_idx[i] == alive_block->sib_idx) {
			curr_idx = i;
			break;
		}
	}

	if (curr_idx == UINT8_MAX){
		ret = M0_ERR_INFO(-EINVAL, "Failed to find alive block "
				  "index %d in alive index array",
				   alive_block->sib_idx);
		goto exit;
	}

	/* Allocate buffer for alive block */
	ret = m0_buf_alloc(&alive_buf, length);
	if (ret != 0){
		ret = M0_ERR_INFO(ret, "Failed to allocate buffer of "
				  "length = %u", length);
		goto exit;
	}

	/* Get data from current vector in buffer */
	ret = bufvec_to_buf_copy(&alive_buf, &alive_bufvec, 1);
	if (ret != 0){
		ret = M0_ERR_INFO(ret, "Failed to get data from alive "
				  "vector in buffer.");
		goto exit;
	}

	isal_encode_data_update(f_bufs, &alive_buf, curr_idx,
				ir->si_decode_tbls, ir->si_data_nr,
				ir->si_failed_nr);


	/* Copy recovered data back to buffer vector. */
	ret = buf_to_bufvec_copy(failed_bufvecs, f_bufs, ir->si_failed_nr);
	if (ret != 0){
		ret = M0_ERR_INFO(ret, "Failed to copy recovered data back "
				  "to buffer vector.");
	}

exit:
	m0_buf_free(&alive_buf);
	for (i = 0; i < ir->si_failed_nr; i++)
		m0_buf_free(&f_bufs[i]);

	return M0_RC(ret);
}
#endif /* ISAL_ENCODE_ENABLED */

M0_INTERNAL void m0_sns_ir_recover(struct m0_sns_ir *ir,
				   struct m0_bufvec *bufvec,
				   const struct m0_bitmap *bitmap,
				   uint32_t failed_index,
				   enum m0_sns_ir_block_type block_type)
{
#if !ISAL_ENCODE_ENABLED
	uint32_t		j;
#endif /* !ISAL_ENCODE_ENABLED */
	size_t			block_idx = 0;
	size_t		        b_set_nr;
	struct m0_sns_ir_block *blocks;
	struct m0_sns_ir_block *alive_block;

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
	if (b_set_nr == 1) {
		for (block_idx = 0; block_idx < bitmap->b_nr; ++block_idx) {
			if (m0_bitmap_get(bitmap, block_idx))
				break;
		}
	}
	M0_ASSERT(is_valid_block_idx(ir, block_idx));

	blocks = ir->si_blocks;

	switch (block_type) {
	/* Input block is assumed to be an untransformed block, and is used for
	 * recovering all failed blocks */
	case M0_SI_BLOCK_LOCAL:

		M0_CNT_DEC(ir->si_local_nr);
		alive_block = &blocks[block_idx];
		alive_block->sib_addr = bufvec;
#if ISAL_ENCODE_ENABLED
		M0_ASSERT(ir_recover(ir, alive_block) == 0);
#else
		for (j = 0; j < ir_blocks_count(ir); ++j)
			if (ir->si_blocks[j].sib_status == M0_SI_BLOCK_FAILED) {
				incr_recover(&blocks[j], alive_block, ir);
				m0_bitmap_set(&blocks[j].sib_bitmap,
					      alive_block->sib_idx, false);
			}
		if (ir->si_local_nr == 0 && are_failures_mixed(ir)) {
			failed_data_blocks_xform(ir);
		}
#endif /* ISAL_ENCODE_ENABLED */
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

	M0_LEAVE();
}

#if !ISAL_ENCODE_ENABLED
static void incr_recover(struct m0_sns_ir_block *failed_block,
			 const struct m0_sns_ir_block *alive_block,
			 struct m0_sns_ir *ir)
{
	const struct m0_matrix *mat;
	uint32_t		row;
	uint32_t		col;
	uint32_t		last_usable_bid;
	m0_parity_elem_t	mat_elem;

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
#endif /* !ISAL_ENCODE_ENABLED */

static void gfaxpy(struct m0_bufvec *y, struct m0_bufvec *x,
		   m0_parity_elem_t alpha)
{
	uint32_t		i;
	uint32_t		seg_size;
	uint8_t		       *y_addr;
	uint8_t		       *x_addr;
	m0_bcount_t		step;
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

#if !ISAL_ENCODE_ENABLED
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
	uint32_t	       j;

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
	uint32_t		i;
	uint32_t		j;

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
#endif /* !ISAL_ENCODE_ENABLED */

static inline bool is_valid_block_idx(const struct m0_sns_ir *ir,
				      uint32_t block_idx)
{
	return block_idx < ir_blocks_count(ir);
}

#if !ISAL_ENCODE_ENABLED
static bool is_data(const struct m0_sns_ir *ir, uint32_t index)
{
	M0_PRE(is_valid_block_idx(ir, index));
	return index < ir->si_data_nr;
}
#endif /* !ISAL_ENCODE_ENABLED */

static bool is_usable(const struct m0_sns_ir *ir,
		      const struct m0_bitmap *in_bmap,
		      struct m0_sns_ir_block *failed_block)
{
	size_t		  i;
	uint32_t	  last_usable_bid;

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

static uint32_t last_usable_block_id(const struct m0_sns_ir *ir,
				     uint32_t block_idx)
{
#if !ISAL_ENCODE_ENABLED
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
#else
	return ir->si_alive_idx[ir->si_data_nr - 1];
#endif /* !ISAL_ENCODE_ENABLED */
}

#if !ISAL_ENCODE_ENABLED
static inline const struct m0_matrix* recovery_mat_get(const struct m0_sns_ir
						       *ir, uint32_t failed_idx)
{
	return is_data(ir, failed_idx) ? &ir->si_data_recovery_mat :
		&ir->si_parity_recovery_mat;
}

static inline  bool are_failures_mixed(const struct m0_sns_ir *ir)
{
	return !!ir->si_failed_data_nr &&
		ir_blocks_count(ir) != ir->si_failed_data_nr + ir->si_alive_nr;
}
#endif /* !ISAL_ENCODE_ENABLED */

static inline uint32_t ir_blocks_count(const struct m0_sns_ir *ir)
{
	return ir->si_data_nr + ir->si_parity_nr;
}

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
