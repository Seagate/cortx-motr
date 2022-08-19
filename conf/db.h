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
#ifndef __MOTR_CONF_DB_H__
#define __MOTR_CONF_DB_H__

#include "be/tx_credit.h"
#include "be/seg.h"


struct m0_fid;
struct m0_confx;
struct m0_confx_obj;

/**
 * Calculates BE credits required by configuration database tables and @conf.
 */
M0_INTERNAL int m0_confdb_create_credit(struct m0_be_seg *seg,
					const struct m0_confx *conf,
					struct m0_be_tx_credit *accum);

/**
 * Creates configuration database, populating it with provided
 * configuration data.
 *
 * @pre  conf->cx_nr > 0
 */
M0_INTERNAL int m0_confdb_create(struct m0_be_seg      *seg,
				 struct m0_be_tx       *tx,
				 const struct m0_confx *conf,
				 const struct m0_fid   *btree_fid);

/**
 * Finalises in-memory configuration database.
 */
M0_INTERNAL void m0_confdb_fini(struct m0_be_seg *seg);
/**
 * Calculates BE credits in-order to destroy configuration database from
 * persistent store.
 */
M0_INTERNAL void m0_confdb_destroy_credit(struct m0_be_seg *seg,
					  struct m0_be_tx_credit *accum);
M0_INTERNAL int m0_confdb_destroy(struct m0_be_seg *seg, struct m0_be_tx *tx);

/**
 * Calculates BE credits in-order to truncate configuration database from
 * persistent store. Currently it is only used by conf-ut.
 */
M0_INTERNAL int m0_confdb_truncate_credit(struct m0_be_seg       *seg,
					   struct m0_be_tx        *tx,
					   struct m0_be_tx_credit *accum,
					   m0_bcount_t            *limit);
/**
 * Truncates configuration data base. Currently only used by conf-ut.
 */
M0_INTERNAL int m0_confdb_truncate(struct m0_be_seg *seg,
				   struct m0_be_tx  *tx,
				   m0_bcount_t       limit);
/**
 * Creates m0_confx and populates it with data read from a
 * configuration database.
 *
 * @note If the call succeeds, the user is responsible for freeing
 *       allocated memory with m0_confx_free(*out).
 */
M0_INTERNAL int m0_confdb_read(struct m0_be_seg *seg, struct m0_confx **out);

#endif /* __MOTR_CONF_DB_H__ */
