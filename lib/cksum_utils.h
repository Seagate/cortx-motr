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

#pragma once

#ifndef __MOTR_CKSUM_UTILS_H__
#define __MOTR_CKSUM_UTILS_H__


/**
 * Function to get number of units starting in a given extent range.
 */
M0_INTERNAL m0_bcount_t m0_extent_get_num_unit_start( m0_bindex_t ext_start,
                                                        m0_bindex_t ext_len, m0_bindex_t unit_sz );

/**
 * Function returns offset for a given unit size
 */
M0_INTERNAL m0_bcount_t m0_extent_get_unit_offset( m0_bindex_t off,
                                                        m0_bindex_t base_off, m0_bindex_t unit_sz);

/**
 * Calculates checksum address for a cob segment and unit size
 */
M0_INTERNAL void * m0_extent_get_checksum_addr(void *b_addr, m0_bindex_t off,
                                                        m0_bindex_t base_off, m0_bindex_t unit_sz, m0_bcount_t cs_size);

/**
 * Calculates checksum nob for a cob segment and unit size
 */
M0_INTERNAL m0_bcount_t m0_extent_get_checksum_nob(m0_bindex_t ext_start, m0_bindex_t ext_length,
                                                         m0_bindex_t unit_sz, m0_bcount_t cs_size);

M0_INTERNAL void * m0_extent_vec_get_checksum_addr(void *b_addr, m0_bindex_t off,
                                                        void *vec, m0_bindex_t unit_sz, m0_bcount_t cs_sz );

#endif /* __MOTR_CKSUM_UTILS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
