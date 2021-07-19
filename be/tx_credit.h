/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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
#ifndef __MOTR_BE_TX_CREDIT_H__
#define __MOTR_BE_TX_CREDIT_H__

#include "lib/types.h"  /* m0_bcount_t */

/**
 * @defgroup be Meta-data back-end
 *
 * @{
 */

/**
 * Credit represents resources that a transaction could consume:
 *
 *     - for each region captured by an active transaction, contents of captured
 *       region must be stored somewhere (to be written to the log later). That
 *       requires memory, which must be pre-allocated before transaction
 *       captures anything to avoid dead-locks;
 *
 *     - similarly, for each captured region, a fixed size region descriptor
 *       (m0_be_reg_d) should be stored. The memory for the descriptor must be
 *       pre-allocated;
 *
 *     - finally, before transaction captures anything, transaction engine must
 *       assure that there is enough free space in the log to write
 *       transaction's updates. The space required is proportional to total
 *       number of regions captured by the transaction and to total size of
 *       these regions.
 *
 * Hence, the user should inform the engine about amount and size of regions
 * that the transaction would modify. This is achieved by calling
 * m0_be_tx_prep() (possibly multiple times), while the transaction is in
 * PREPARE state. The calls to m0_be_tx_prep() must be conservative: it is fine
 * to prepare for more updates than the transaction will actually make (the
 * latter quantity is usually impossible to know beforehand anyway), but the
 * transaction must never capture more than it prepared.
 *
 * @see M0_BE_TX_CREDIT(), M0_BE_TX_CREDIT_TYPE().
 */

#ifdef __KERNEL__
#define M0_DEBUG_BE_CREDITS (0)
#else
#define M0_DEBUG_BE_CREDITS (1)
#endif

enum m0_be_credit_users {
	M0_BE_CU_BTREE_INSERT,
	M0_BE_CU_BTREE_DELETE,
	M0_BE_CU_BTREE_UPDATE,
	M0_BE_CU_EMAP_SPLIT,
	M0_BE_CU_EMAP_PASTE,
	M0_BE_CU_NR
};

struct m0_be_tx_credit {
	/**
	 * The number of regions needed for operation representation in the
	 * transaction.
	 */
	m0_bcount_t tc_reg_nr;
	/** Total size of memory needed for the same. */
	m0_bcount_t tc_reg_size;
	/** Number of callbacks associated with the transaction */
	m0_bcount_t tc_cb_nr;
	/** Used to track who uses the credit and how much. */
	unsigned    tc_balance[M0_BE_CU_NR];
};

/* invalid m0_be_tx_credit value */
extern const struct m0_be_tx_credit m0_be_tx_credit_invalid;

#define M0_BE_TX_CB_CREDIT(nr, size, cb_count)				    \
	(struct m0_be_tx_credit){ .tc_reg_nr = (nr), .tc_reg_size = (size), \
	.tc_cb_nr = (cb_count)}
#define M0_BE_TX_CREDIT(nr, size) M0_BE_TX_CB_CREDIT(nr, size, 0)

#define M0_BE_TX_CREDIT_TYPE(type) M0_BE_TX_CREDIT(1, sizeof(type))
#define M0_BE_TX_CREDIT_PTR(ptr)   M0_BE_TX_CREDIT(1, sizeof *(ptr))
#define M0_BE_TX_CREDIT_BUF(buf)   M0_BE_TX_CREDIT(1, (buf)->b_nob)

/** Format for the printf() family functions. @see BETXCR_P */
#define BETXCR_F "(%lu,%lu)"

/**
 * Example:
 *
 * @code
 * struct m0_be_tx_credit cred;
 * ...
 * printf("cred = " BETXCR_F "\n", BETXCR_P(&cred));
 * @endcode
 */
#define BETXCR_P(c) (unsigned long)(c)->tc_reg_nr, \
		    (unsigned long)(c)->tc_reg_size


/** c0 += c1 */
M0_INTERNAL void m0_be_tx_credit_add(struct m0_be_tx_credit *c0,
				     const struct m0_be_tx_credit *c1);

/** c0 -= c1 */
M0_INTERNAL void m0_be_tx_credit_sub(struct m0_be_tx_credit *c0,
				     const struct m0_be_tx_credit *c1);

/** c *= k */
M0_INTERNAL void m0_be_tx_credit_mul(struct m0_be_tx_credit *c, m0_bcount_t k);

/**
 * c *= bp / 10000.0
 * @note bp is basis point.
 */
M0_INTERNAL void m0_be_tx_credit_mul_bp(struct m0_be_tx_credit *c, unsigned bp);

/**
 * c += c1 * k
 * Multiply-accumulate operation.
 */
M0_INTERNAL void m0_be_tx_credit_mac(struct m0_be_tx_credit *c,
				     const struct m0_be_tx_credit *c1,
				     m0_bcount_t k);

/**
 * c = smallest credit that meets the following requirement:
 * m0_be_tx_credit_le(c0, c) && m0_be_tx_credit_le(c1, c)
 */
M0_INTERNAL void m0_be_tx_credit_max(struct m0_be_tx_credit       *c,
				     const struct m0_be_tx_credit *c0,
				     const struct m0_be_tx_credit *c1);

/* c += m0_be_tx_credit_max(c, c0, c1) */
M0_INTERNAL void m0_be_tx_credit_add_max(struct m0_be_tx_credit       *c,
					 const struct m0_be_tx_credit *c0,
					 const struct m0_be_tx_credit *c1);

/* c0 <= c1 */
M0_INTERNAL bool m0_be_tx_credit_le(const struct m0_be_tx_credit *c0,
				    const struct m0_be_tx_credit *c1);

/* c0 == c1 */
M0_INTERNAL bool m0_be_tx_credit_eq(const struct m0_be_tx_credit *c0,
				    const struct m0_be_tx_credit *c1);

/** @} end of be group */
#endif /* __MOTR_BE_TX_CREDIT_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
