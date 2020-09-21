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


#pragma once

#ifndef __MOTR_MW_CONSENSUS_H__
#define __MOTR_MW_CONSENSUS_H__


struct m0_m_container;
struct m0_id_factory;
struct m0_tx;

/**
   @defgroup consensus Consensus
   @{
*/

struct m0_consensus_domain;
struct m0_consensus_proposer;
struct m0_consensus_acceptor;
struct m0_consensus;

struct m0_consensus_acceptor_ops {
	int  (*cacc_is_value_ok)(struct m0_consensus_acceptor *acc,
				 const struct m0_consensus *cons);
	void (*cacc_reached)(struct m0_consensus_acceptor *acc,
			     struct m0_tx *tx, const struct m0_consensus *cons);
};

M0_INTERNAL int m0_consensus_domain_init(struct m0_consensus_domain **domain);
M0_INTERNAL void m0_consensus_domain_fini(struct m0_consensus_domain *domain);
M0_INTERNAL int m0_consensus_domain_add(struct m0_consensus_domain *domain,
					struct m0_server *server);

M0_INTERNAL int m0_consensus_proposer_init(struct m0_consensus_proposer
					   **proposer,
					   struct m0_id_factory *idgen);
M0_INTERNAL void m0_consensus_proposer_fini(struct m0_consensus_proposer
					    *proposer);

M0_INTERNAL int m0_consensus_acceptor_init(struct m0_consensus_acceptor
					   **acceptor,
					   struct m0_m_container *store,
					   const struct
					   m0_consensus_acceptor_ops *ops);
M0_INTERNAL void m0_consensus_acceptor_fini(struct m0_consensus_acceptor
					    *acceptor);

M0_INTERNAL int m0_consensus_init(struct m0_consensus **cons,
				  struct m0_consensus_proposer *prop,
				  const struct m0_buf *val);
M0_INTERNAL void m0_consensus_fini(struct m0_consensus *cons);

M0_INTERNAL int m0_consensus_establish(struct m0_consensus_proposer *proposer,
				       struct m0_consensus *cons);

M0_INTERNAL struct m0_buf *m0_consensus_value(const struct m0_consensus *cons);

/** @} end of consensus group */

/* __MOTR_MW_CONSENSUS_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
