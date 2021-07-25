/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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


#include "lib/types.h"
#include "lib/assert.h"
#include "lib/arith.h"  /* M0_3WAY */
#include "lib/arith.h"
#include "lib/string.h" /* sscanf */
#include "lib/errno.h"  /* EINVAL */
#include "lib/buf.h"    /* m0_buf */
#include "lib/vec.h"    /* m0_indexvec */
#include "lib/ext.h"    /* m0_ext */
#include "lib/trace.h"  /* M0_RC */

void __dummy_function(void)
{
}

/* No padding. */
M0_BASSERT(sizeof(struct m0_uint128) == 16);

M0_INTERNAL bool m0_uint128_eq(const struct m0_uint128 *u0,
			       const struct m0_uint128 *u1)
{
	return m0_uint128_cmp(u0, u1) == 0;
}

M0_INTERNAL int m0_uint128_cmp(const struct m0_uint128 *u0,
			       const struct m0_uint128 *u1)
{
	return M0_3WAY(u0->u_hi, u1->u_hi) ?: M0_3WAY(u0->u_lo, u1->u_lo);
}

M0_INTERNAL int m0_uint128_sscanf(const char *s, struct m0_uint128 *u128)
{
	int rc = sscanf(s, U128I_F, U128_S(u128));
	return rc == 2 ? 0 : -EINVAL;
}

M0_INTERNAL void m0_uint128_add(struct m0_uint128 *res,
				const struct m0_uint128 *a,
				const struct m0_uint128 *b)
{
	/*
	 * If res == a, we need to store the original value of a->u_lo
	 * in order for the function to work correctly.
	 */
	uint64_t a_lo = a->u_lo;

	res->u_lo = a->u_lo + b->u_lo;
	res->u_hi = a->u_hi + b->u_hi + (res->u_lo < a_lo);
}

M0_INTERNAL void m0_uint128_mul64(struct m0_uint128 *res, uint64_t a,
				  uint64_t b)
{
	uint64_t a_lo = a & UINT32_MAX;
	uint64_t a_hi = a >> 32;
	uint64_t b_lo = b & UINT32_MAX;
	uint64_t b_hi = b >> 32;
	uint64_t c;

	M0_CASSERT((sizeof a) * CHAR_BIT == 64);

	/*
	 * a * b = a_hi * b_hi * (1 << 64) +
	 *	   a_lo * b_lo +
	 *	   a_lo * b_hi * (1 << 32) +
	 *	   a_hi * b_lo * (1 << 32)
	 */
	*res = M0_UINT128(a_hi * b_hi, a_lo * b_lo);
	c = a_lo * b_hi;
	m0_uint128_add(res, res, &M0_UINT128(c >> 32, (c & UINT32_MAX) << 32));
	c = a_hi * b_lo;
	m0_uint128_add(res, res, &M0_UINT128(c >> 32, (c & UINT32_MAX) << 32));
}

uint64_t m0_rnd64(uint64_t *prev)
{
	/*
	 * Linear congruential generator with constants from TAOCP MMIX.
	 * http://en.wikipedia.org/wiki/Linear_congruential_generator
	 */
	/* double result; */
	return *prev = *prev * 6364136223846793005ULL + 1442695040888963407ULL;
	/*
	 * Use higher bits of *prev to generate return value, because they are
	 * more random.
	 */
	/* return result * max / (1.0 + ~0ULL); */
}

uint64_t m0_rnd(uint64_t max, uint64_t *prev)
{
	uint64_t result;
	/* Uses the same algorithm as GNU libc */
	result = *prev = *prev * 0x5DEECE66DULL + 0xB;

	/* PRNG generates 48-bit values only */
	M0_ASSERT((max >> 48) == 0);
	/*Take value from higher 48 bits */
	return (result >> 16) * max / ((~0UL) >> 16);
}
M0_EXPORTED(m0_rnd);

M0_INTERNAL uint64_t m0_gcd64(uint64_t p, uint64_t q)
{
	uint64_t t;

	while (q != 0) {
		t = p % q;
		p = q;
		q = t;
	}
	return p;
}

static uint64_t m0u64(const unsigned char *s)
{
	uint64_t v;
	int      i;

	for (v = 0, i = 0; i < 8; ++i)
		v |= ((uint64_t)s[i]) << (64 - 8 - i * 8);
	return v;
}

M0_INTERNAL void m0_uint128_init(struct m0_uint128 *u128, const char *magic)
{
	M0_ASSERT(strlen(magic) == sizeof *u128);
	u128->u_hi = m0u64((const unsigned char *)magic);
	u128->u_lo = m0u64((const unsigned char *)magic + 8);
}

enum {
	M0_MOD_SAFE_LIMIT = UINT64_MAX/32
};

static int64_t getdelta(uint64_t x0, uint64_t x1)
{
	int64_t delta;

	delta = (int64_t)x0 - (int64_t)x1;
	M0_ASSERT( delta < (int64_t)M0_MOD_SAFE_LIMIT &&
		  -delta < (int64_t)M0_MOD_SAFE_LIMIT);
	return delta;
}

M0_INTERNAL bool m0_mod_gt(uint64_t x0, uint64_t x1)
{
	return getdelta(x0, x1) > 0;
}

M0_INTERNAL bool m0_mod_ge(uint64_t x0, uint64_t x1)
{
	return getdelta(x0, x1) >= 0;
}

M0_INTERNAL uint64_t m0_round_up(uint64_t val, uint64_t size)
{
	M0_PRE(m0_is_po2(size));
	return (val + size - 1) & ~(size - 1) ;
}

M0_INTERNAL uint64_t m0_round_down(uint64_t val, uint64_t size)
{
	M0_PRE(m0_is_po2(size));
	return val & ~(size - 1);
}

/*
 * Check that ergo() and equi() macros are really what they pretend to be.
 */

M0_BASSERT(ergo(false, false) == true);
M0_BASSERT(ergo(false, true)  == true);
M0_BASSERT(ergo(true,  false) == false);
M0_BASSERT(ergo(true,  true)  == true);

M0_BASSERT(equi(false, false) == true);
M0_BASSERT(equi(false, true)  == false);
M0_BASSERT(equi(true,  false) == false);
M0_BASSERT(equi(true,  true)  == true);

M0_INTERNAL const char *m0_bool_to_str(bool b)
{
	return b ? "true" : "false";
}

M0_INTERNAL const char *m0_short_file_name(const char *fname)
{
	static const char  top_src_dir[] = "motr/";
	const char        *p;

	p = strstr(fname, top_src_dir);
	if (p != NULL)
		return p + strlen(top_src_dir);

	return fname;
}

M0_INTERNAL const char *m0_failed_condition;
M0_EXPORTED(m0_failed_condition);

M0_INTERNAL uint32_t m0_no_of_bits_set(uint64_t val)
{
	uint32_t	     count = 0;
	static const uint8_t bits_set[] = {
		[ 0] = 0, /* 0000 */
		[ 1] = 1, /* 0001 */
		[ 2] = 1, /* 0010 */
		[ 3] = 2, /* 0011 */
		[ 4] = 1, /* 0100 */
		[ 5] = 2, /* 0101 */
		[ 6] = 2, /* 0110 */
		[ 7] = 3, /* 0111 */
		[ 8] = 1, /* 1000 */
		[ 9] = 2, /* 1001 */
		[10] = 2, /* 1010 */
		[11] = 3, /* 1011 */
		[12] = 2, /* 1100 */
		[13] = 3, /* 1101 */
		[14] = 3, /* 1110 */
		[15] = 4  /* 1111 */
	};

	while (val != 0) {
		count += bits_set[val & 0xF];
		val >>= 4;
	}
	return count;
}

M0_INTERNAL bool
m0_elems_are_unique(const void *array, unsigned nr_elems, size_t elem_size)
{
	return m0_forall(i, nr_elems,
			 m0_forall(j, i, memcmp(array + i * elem_size,
						array + j * elem_size,
						elem_size) != 0));
}

M0_INTERNAL unsigned int
m0_full_name_hash(const unsigned char *name, unsigned int len)
{
	unsigned long hash = 0;
	unsigned long c;
	while (len--) {
		c = *name++;
		hash = (hash + (c << 4) + (c >> 4)) * 11;
	}
	return ((unsigned int)hash);
}

M0_INTERNAL uint64_t m0_ptr_wrap(const void *p)
{
	return p != NULL ? p - (void *)&m0_ptr_wrap : 0;
}

M0_INTERNAL const void *m0_ptr_unwrap(uint64_t val)
{
	return val != 0 ? val + (void *)&m0_ptr_wrap : NULL;
}


M0_INTERNAL void m0_permute(uint64_t n, uint64_t *k, uint64_t *s, uint64_t *r)
{
	uint64_t i;
	uint64_t j;
	uint64_t t;
	uint64_t x;

	/*
	 * k[0] is an index of one of the n elements that permutation moves to
	 * the 0-th position in s[];
	 *
	 * k[1] is an index of one of the (n - 1) remaining elements that
	 * permutation moves to the 1-st position in s[], etc.
	 *
	 * To produce i-th element of s[], pick one of remaining elements, say
	 * s[t], as specified by k[i], shift elements s[i] ... s[t] to the right
	 * by one and place s[t] in s[i]. This guarantees that at beginning of
	 * the loop elements s[0] ... s[i - 1] are already selected and elements
	 * s[i] ... s[n - 1] are "remaining".
	 */

	for (i = 0; i < n - 1; ++i) {
		t = k[i] + i;
		M0_ASSERT(t < n);
		x = s[t];
		for (j = t; j > i; --j)
			s[j] = s[j - 1];
		s[i] = x;
		r[x] = i;
	}
	/*
	 * The loop above iterates n-1 times, because the last element finds its
	 * place automatically. Complete inverse permutation.
	 */
	r[s[n - 1]] = n - 1;
}

M0_INTERNAL void m0_array_sort(uint64_t *arr, uint64_t arr_len)
{
	uint64_t i;
	uint64_t j;

	M0_PRE(arr_len > 0);
	for (i = 0; i < arr_len - 1; ++i) {
		for (j = i + 1; j < arr_len - 1; ++j) {
			if (arr[i] > arr[j])
				M0_SWAP(arr[i], arr[j]);
		}
	}

}

M0_INTERNAL bool m0_bit_get(void *buffer, m0_bcount_t i)
{
	m0_bcount_t byte_num  = M0_BYTES(i + 1) - 1;
	char        byte_val  = *((char*)buffer + byte_num);
	m0_bcount_t bit_shift = i % 8;

	return 0x1 & (byte_val >> bit_shift);
}

M0_INTERNAL void m0_bit_set(void *buffer, m0_bcount_t i, bool val)
{
	m0_bcount_t  byte_num  = M0_BYTES(i + 1) - 1;
	m0_bcount_t  bit_shift = i % 8;
	char        *byte_val  = (char*)buffer + byte_num;

	*byte_val |= (0x1 & val) << bit_shift;
}

M0_INTERNAL const struct m0_key_val M0_KEY_VAL_NULL = {
	.kv_key = M0_BUF_INIT0,
	.kv_val = M0_BUF_INIT0,
};

M0_INTERNAL bool m0_key_val_is_null(struct m0_key_val *kv)
{
	return m0_buf_eq(&M0_KEY_VAL_NULL.kv_key, &kv->kv_key) &&
		m0_buf_eq(&M0_KEY_VAL_NULL.kv_val, &kv->kv_val);
}

M0_INTERNAL void m0_key_val_init(struct m0_key_val *kv, const struct m0_buf *key,
				 const struct m0_buf *val)
{
	M0_PRE(kv != NULL);

	kv->kv_key = *key;
	kv->kv_val = *val;
}

M0_INTERNAL void m0_key_val_null_set(struct m0_key_val *kv)
{
	m0_key_val_init(kv, &M0_KEY_VAL_NULL.kv_key, &M0_KEY_VAL_NULL.kv_val);
}

M0_INTERNAL m0_bcount_t m0_extent_get_num_unit_start( m0_bindex_t ext_start,
						m0_bindex_t ext_len, m0_bindex_t unit_sz )
{

	/* Compute how many unit starts in a given extent spans: 
	 * Illustration below shows how extents can be received w.r.t unit size (4)
	 *    | Unit 0 || Unit 1 || Unit 2 || Unit 3 || Unit 4 ||
	 * 1. | e1 | 			  	=> 1 (0,2)(ext_start,ext_len)  
	 * 2. |   e2   |		  	=> 1 (0,4) ending on unit 0
	 * 3. |   e2    |		  	=> 1 (0,5) ending on unit 1 start
	 * 4.        |  e3     |	=> 1 (2,5)
	 * 5.  | e4 | 			  	=> 0 (1,3) within unit 0	
	 * 6.          |         |  => 1 (3,5) ending on unit 1 end
	 * 7.          |          | => 2 (3,6) ending on unit 2 start 
	 * To compute how many DU start we need to find the DU Index of
	 * start and end. 
	 */
	m0_bcount_t cs_nob;
	M0_ASSERT(unit_sz);
	cs_nob = ( (ext_start + ext_len - 1)/unit_sz - ext_start/unit_sz );
	cs_nob = ( (ext_start + ext_len - 1)/unit_sz - ext_start/unit_sz );

	// Add handling for case 1 and 5	
	if( (ext_start % unit_sz) == 0 ) 
		cs_nob++;

	return cs_nob;
}

M0_INTERNAL m0_bcount_t m0_extent_get_unit_offset( m0_bindex_t off, 
							m0_bindex_t base_off, m0_bindex_t unit_sz)
{
	M0_ASSERT(unit_sz);
	/* Unit size we get from layout id using m0_obj_layout_id_to_unit_size(lid) */
	return (off - base_off)/unit_sz;
}

M0_INTERNAL void * m0_extent_get_checksum_addr(void *b_addr, m0_bindex_t off, 
							m0_bindex_t base_off, m0_bindex_t unit_sz, m0_bcount_t cs_size )
{	
	M0_ASSERT(unit_sz && cs_size);
	return b_addr + m0_extent_get_unit_offset(off, base_off, unit_sz) *
					cs_size;	
}

M0_INTERNAL m0_bcount_t m0_extent_get_checksum_nob( m0_bindex_t ext_start, 
							m0_bindex_t ext_length, m0_bindex_t unit_sz, m0_bcount_t cs_size )
{	
	M0_ASSERT(unit_sz && cs_size);
	return m0_extent_get_num_unit_start(ext_start, ext_length, unit_sz) * cs_size;
}

/* This function will get checksum address for application provided checksum buffer
 * Checksum is corresponding to on offset (e.g gob offset) & its extent and this 
 * function helps to locate exact address for the above.
 * Checksum is stored in contigious buffer: cksum_buf_vec, while COB extents may
 * not be contigious e.g.
 * Assuming each extent has two DU, so two checksum. 
 *     | CS0 | CS1 | CS2 | CS3 | CS4 | CS5 | CS6 |
 *     | iv_index[0] |   | iv_index[1] | iv_index[2] |   | iv_index[3] |     
 * Now if we have an offset for CS3 then after first travesal b_addr will point to
 * start of CS2 and then it will land in m0_ext_is_in and will compute correct 
 * addr for CS3.
 */
M0_INTERNAL void * m0_extent_vec_get_checksum_addr(void *cksum_buf_vec, m0_bindex_t off, 
							void *ivec, m0_bindex_t unit_sz, m0_bcount_t cs_sz )
{
	void *cksum_addr = NULL;
	struct m0_ext ext;
	struct m0_indexvec *vec = (struct m0_indexvec *)ivec;
	struct m0_bufvec *cksum_vec = (struct m0_bufvec *)cksum_buf_vec;
	struct m0_bufvec_cursor   cksum_cursor;
	int attr_nob = 0;
	int i;
	
	/* Get the checksum nobs consumed till reaching the off in given io */
	for (i = 0; i < vec->iv_vec.v_nr; i++)
	{
		ext.e_start = vec->iv_index[i];
		ext.e_end = vec->iv_index[i] + vec->iv_vec.v_count[i];

		/* We construct current extent e.g for iv_index[0] and check if offset is 
		 * within the span of current extent
  		 *	| iv_index[0] || iv_index[1] | iv_index[2] || iv_index[3] | 
  		 */
		if(m0_ext_is_in(&ext, off))
		{
			attr_nob += ( m0_extent_get_unit_offset(off, ext.e_start, unit_sz) * cs_sz);
			break;
		}
		else
		{
			/* off is not in the current extent, so account increment the b_addr */
			attr_nob +=  m0_extent_get_checksum_nob(ext.e_start, 
			                		vec->iv_vec.v_count[i], unit_sz, cs_sz);
		}
	}

	/* Assert to make sure the the offset is lying within the extent */
	M0_ASSERT(i < vec->iv_vec.v_nr );

	// get the checksum_addr
	m0_bufvec_cursor_init(&cksum_cursor, cksum_vec);

	if( attr_nob )
	{
		M0_ASSERT(m0_vec_cursor_end(&cksum_cursor) >= attr_nob);
		m0_bufvec_cursor_move(&cksum_cursor, attr_nob);	
	}
	cksum_addr = m0_bufvec_cursor_addr(&cksum_cursor);
	
	return cksum_addr;
}

M0_INTERNAL void *m0_vote_majority_get(struct m0_key_val *arr, uint32_t len,
				       bool (*cmp)(const struct m0_buf *,
					           const struct m0_buf *),
				       uint32_t *vote_nr)
{
	struct m0_key_val  null_kv = M0_KEY_VAL_NULL;
	struct m0_key_val *mjr     = &null_kv;
	uint32_t           i;
	uint32_t           count;

	for (i = 0, count = 0; i < len; ++i) {
		if (count == 0)
			mjr = &arr[i];
		/* A M0_KEY_VAL_NULL requires a special handling. */
		if (!m0_key_val_is_null(mjr) &&
		    !m0_key_val_is_null(&arr[i]))
			/*
			 * A case when neither is M0_KEY_VAL_NULL, invoke
			 * a user provided comparison method
			 */
			count += cmp(&mjr->kv_val, &arr[i].kv_val) ? 1: -1;
		else if (m0_key_val_is_null(mjr) &&
		         m0_key_val_is_null(&arr[i]))
			/* A case when both are M0_KEY_VAL_NULL. */
			++count;
		else
			/* Exactly one of the two is M0_KEY_VAL_NULL */
			--count;
	}
	if (m0_key_val_is_null(mjr))
		return NULL;
	/* Check if the element found is present in majority. */
	for (i = 0, count = 0; i < len; ++i) {
		if (m0_key_val_is_null(&arr[i]))
			continue;
		if (cmp(&mjr->kv_val, &arr[i].kv_val))
			++count;
	}
	*vote_nr = count;
	return count > len/2  ? mjr : NULL;
}

M0_INTERNAL uint64_t m0_dummy_id_generate(void)
{
	static struct m0_atomic64 counter = {};
	return (uint64_t)m0_atomic64_add_return(&counter, 1);
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
