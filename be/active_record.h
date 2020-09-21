/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_BE_ACT_RECORD_H__
#define __MOTR_BE_ACT_RECORD_H__

/**
 * @defgroup BE
 *
 * @{
 */

#include "be/list.h"
#include "lib/buf.h"
#include "lib/chan.h"
#include "lib/mutex.h"

enum m0_be_active_record_type {
	ART_OPEN,
	ART_CLOSE,
	ART_NORM
};

struct m0_be_active_record {
	struct m0_be_list_link			ar_link;
	uint64_t				ar_tx_id;
	enum m0_be_active_record_type		ar_rec_type;
	struct m0_be_active_record_domain      *ar_dom;
	/**
	 * Copy of tx payload inside log today used as a workaround to
	 * access this payload during recovery and without need to
	 * scan log fully
	 */
	struct m0_buf				ar_payload;
	uint64_t				ar_magic;
};

struct m0_be_active_record_domain_subsystem {
	char                   rds_name[32];
	struct m0_be_list      rds_list;
	/* link into m0_be_active_record_domain::ard_list */
	struct m0_be_list_link rds_link;
	uint64_t               rds_magic;

	/* volatile fields */
	struct m0_mutex        rds_lock;
	struct m0_chan         rds_chan;
};

struct m0_be_active_record_domain {
	struct m0_be_list  ard_list;
	struct m0_be_seg  *ard_seg;
};

enum m0_be_active_record_domain_op {
	RDO_CREATE,
	RDO_DESTROY
};

enum m0_be_active_record_op {
	ARO_CREATE,
	ARO_DESTROY,
	ARO_DEL,
	ARO_ADD
};

/* ----------------------------------------------------------------------
 * struct m0_be_active_record_domain
 * ---------------------------------------------------------------------- */

M0_INTERNAL void
m0_be_active_record_domain_init(struct m0_be_active_record_domain *dom,
				struct m0_be_seg *seg);
M0_INTERNAL void
m0_be_active_record_domain_fini(struct m0_be_active_record_domain *dom);
M0_INTERNAL bool
m0_be_active_record_domain__invariant(struct m0_be_active_record_domain *dom);

#define m0_be_active_record_domain_create(dom, tx, seg, ...)		\
	m0_be_active_record_domain__create((dom), (tx), (seg),		\
					   (const struct m0_buf []){	\
					   __VA_ARGS__, M0_BUF_INIT0 })

/* @pre m0_be_active_record_domain_init() is called */
M0_INTERNAL int
m0_be_active_record_domain__create(struct m0_be_active_record_domain **dom,
				   struct m0_be_tx                    *tx,
				   struct m0_be_seg                   *seg,
				   const struct m0_buf                *path);
M0_INTERNAL int
m0_be_active_record_domain_destroy(struct m0_be_active_record_domain *dom,
				   struct m0_be_tx *tx);

M0_INTERNAL void
m0_be_active_record_domain_credit(struct m0_be_active_record_domain *dom,
				  enum m0_be_active_record_domain_op op,
				  uint8_t                            subsys_nr,
				  struct m0_be_tx_credit            *accum);

/* ----------------------------------------------------------------------
 * struct m0_be_active_record
 * ---------------------------------------------------------------------- */

M0_INTERNAL void
m0_be_active_record_init(struct m0_be_active_record        *rec,
			 struct m0_be_active_record_domain *ar_dom);
M0_INTERNAL void
m0_be_active_record_fini(struct m0_be_active_record *rec);
M0_INTERNAL bool
m0_be_active_record__invariant(struct m0_be_active_record *rec);

M0_INTERNAL int
m0_be_active_record_create(struct m0_be_active_record	    **rec,
			   struct m0_be_tx	             *tx,
			   struct m0_be_active_record_domain *ar_dom);
M0_INTERNAL int
m0_be_active_record_destroy(struct m0_be_active_record *rec,
			    struct m0_be_tx            *tx);

M0_INTERNAL void
m0_be_active_record_credit(struct m0_be_active_record  *rec,
			   enum m0_be_active_record_op  op,
			   struct m0_be_tx_credit      *accum);

M0_INTERNAL int
m0_be_active_record_add(const char		   *subsys,
			struct m0_be_active_record *rec,
			struct m0_be_tx            *tx);

M0_INTERNAL int
m0_be_active_record_del(const char		   *subsys,
			struct m0_be_active_record *rec,
			struct m0_be_tx            *tx);


/** @} end of BE group */
#endif /* __MOTR_BE_ACT_RECORD_H__ */

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
