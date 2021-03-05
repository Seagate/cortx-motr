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

#ifndef __MOTR_BE_BTREE_INTERNAL_H__
#define __MOTR_BE_BTREE_INTERNAL_H__

/**
 * @defgroup be Meta-data back-end
 *
 * @{
 */

#include "format/format.h" /* m0_format_header */
#include "be/btree.h"      /* BTREE_FAN_OUT */
#include "be/btree_xc.h"   /* m0_be_btree_backlink_xc */

/* btree constants */
enum {
	KV_NR = 2 * BTREE_FAN_OUT - 1,
};
struct be_btree_inlkey{
	char inlkey[64];
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct be_btree_key_val  {
	void *btree_key;
	void *btree_val;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/* WARNING!: fields position is paramount, see node_update() */
struct m0_be_bnode {
	struct m0_format_header      bt_header;  /* Header of node */
	struct m0_be_btree_backlink  bt_backlink;
	struct m0_be_bnode          *bt_next;    /* Pointer to next node */
	unsigned int                 bt_num_active_key;/* Count of active keys */
	unsigned int                 bt_level;   /* Level of node in B-Tree */
	bool                         bt_isleaf;  /* Is this Leaf node? */
	char                         bt_pad[7];  /* Used to padd */
	struct be_btree_key_val      bt_kv_arr[KV_NR]; /* Array of key-vals */	
	struct m0_be_bnode          *bt_child_arr[KV_NR + 1]; /* childnode array */
	char 				 allocated[KV_NR];
	struct be_btree_inlkey		 bt_ik[KV_NR]; /* Array of inline-keys */
	struct m0_format_footer      bt_footer;  /* Footer of node */

} M0_XCA_RECORD M0_XCA_DOMAIN(be);
M0_BASSERT(sizeof(bool) == 1);

enum m0_be_bnode_format_version {
	M0_BE_BNODE_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BE_BNODE_FORMAT_VERSION */
	/*M0_BE_BNODE_FORMAT_VERSION_2,*/
	/*M0_BE_BNODE_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BE_BNODE_FORMAT_VERSION = M0_BE_BNODE_FORMAT_VERSION_1
};

/** @} end of be group */
#endif /* __MOTR_BE_BTREE_INTERNAL_H__ */

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
