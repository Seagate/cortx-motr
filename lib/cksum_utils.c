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
 *
 */

#include "lib/buf.h"    /* m0_buf */
#include "lib/vec.h"    /* m0_indexvec */
#include "lib/ext.h"    /* m0_ext */
#include "lib/trace.h"  /* M0_RC */
#include "lib/cksum_utils.h"


M0_INTERNAL m0_bcount_t m0_extent_get_num_unit_start( m0_bindex_t ext_start,
						m0_bindex_t ext_len, m0_bindex_t unit_sz )
{

	/* Compute how many unit starts in a given extent spans:
	 * Illustration below shows how extents can be received w.r.t unit size (4)
	 *    | Unit 0 || Unit 1 || Unit 2 || Unit 3 || Unit 4 ||
	 * 1. | e1 |                            => 1 (0,2)(ext_start,ext_len)
	 * 2. |   e2   |                        => 1 (0,4) ending on unit 0
	 * 3. |   e2    |                       => 1 (0,5) ending on unit 1 start
	 * 4.        |  e3     |        => 1 (2,5)
	 * 5.  | e4 |                           => 0 (1,3) within unit 0
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
		 *      | iv_index[0] || iv_index[1] | iv_index[2] || iv_index[3] |
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
		m0_bufvec_cursor_move(&cksum_cursor, attr_nob);
	}
	cksum_addr = m0_bufvec_cursor_addr(&cksum_cursor);

	return cksum_addr;
}
