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


#include "be/tx_credit.h"

#include "lib/assert.h"         /* M0_PRE */
#include "lib/arith.h"          /* max_check */
#include "lib/misc.h"           /* m0_forall */

/**
 * @addtogroup be
 *
 * @{
 */

/**
 * Invalid credit structure used to forcibly fail a transaction.
 *
 * This is declared here rather than in credit.c so that this symbol exists in
 * the kernel build.
 */
const struct m0_be_tx_credit m0_be_tx_credit_invalid =
	M0_BE_TX_CREDIT(M0_BCOUNT_MAX, M0_BCOUNT_MAX);

M0_INTERNAL void m0_be_tx_credit_add(struct m0_be_tx_credit *c0,
				     const struct m0_be_tx_credit *c1)
{
	c0->tc_reg_nr   += c1->tc_reg_nr;
	c0->tc_reg_size += c1->tc_reg_size;
	c0->tc_cb_nr	+= c1->tc_cb_nr;
	if (M0_DEBUG_BE_CREDITS) {
		m0_forall(i, M0_BE_CU_NR,
			  c0->tc_balance[i] += c1->tc_balance[i], true);
	}
}

M0_INTERNAL void m0_be_tx_credit_sub(struct m0_be_tx_credit *c0,
				     const struct m0_be_tx_credit *c1)
{
	int i;

	M0_PRE(c0->tc_reg_nr   >= c1->tc_reg_nr);
	M0_PRE(c0->tc_reg_size >= c1->tc_reg_size);

	c0->tc_reg_nr   -= c1->tc_reg_nr;
	c0->tc_reg_size -= c1->tc_reg_size;
	if (M0_DEBUG_BE_CREDITS) {
		for (i = 0; i < M0_BE_CU_NR; ++i) {
			M0_PRE(c0->tc_balance[i] >= c1->tc_balance[i]);
			c0->tc_balance[i] -= c1->tc_balance[i];
		}
	}
}

M0_INTERNAL void m0_be_tx_credit_mul(struct m0_be_tx_credit *c, m0_bcount_t k)
{
	c->tc_reg_nr   *= k;
	c->tc_reg_size *= k;

	if (M0_DEBUG_BE_CREDITS)
		m0_forall(i, M0_BE_CU_NR, c->tc_balance[i] *= k, true);
}

M0_INTERNAL void m0_be_tx_credit_mul_bp(struct m0_be_tx_credit *c, unsigned bp)
{
	c->tc_reg_nr   = c->tc_reg_nr   * bp / 10000;
	c->tc_reg_size = c->tc_reg_size * bp / 10000;
}

M0_INTERNAL void m0_be_tx_credit_mac(struct m0_be_tx_credit *c,
				     const struct m0_be_tx_credit *c1,
				     m0_bcount_t k)
{
	struct m0_be_tx_credit c1_k = *c1;

	m0_be_tx_credit_mul(&c1_k, k);
	m0_be_tx_credit_add(c, &c1_k);
}

M0_INTERNAL bool m0_be_tx_credit_le(const struct m0_be_tx_credit *c0,
				    const struct m0_be_tx_credit *c1)
{
	return c0->tc_reg_nr   <= c1->tc_reg_nr &&
	       c0->tc_reg_size <= c1->tc_reg_size;
}

M0_INTERNAL bool m0_be_tx_credit_eq(const struct m0_be_tx_credit *c0,
				    const struct m0_be_tx_credit *c1)
{
	return c0->tc_reg_nr   == c1->tc_reg_nr &&
	       c0->tc_reg_size == c1->tc_reg_size;
}

M0_INTERNAL void m0_be_tx_credit_max(struct m0_be_tx_credit       *c,
				     const struct m0_be_tx_credit *c0,
				     const struct m0_be_tx_credit *c1)
{
	*c = M0_BE_TX_CREDIT(max_check(c0->tc_reg_nr,   c1->tc_reg_nr),
			     max_check(c0->tc_reg_size, c1->tc_reg_size));
}

M0_INTERNAL void m0_be_tx_credit_add_max(struct m0_be_tx_credit       *c,
					 const struct m0_be_tx_credit *c0,
					 const struct m0_be_tx_credit *c1)
{
	struct m0_be_tx_credit cred;

	m0_be_tx_credit_max(&cred, c0, c1);
	m0_be_tx_credit_add(c, &cred);
}

/** @} end of be group */

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
