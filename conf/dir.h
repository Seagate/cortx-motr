/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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
#ifndef __MOTR_CONF_DIR_H__
#define __MOTR_CONF_DIR_H__

#include "lib/tlist.h"

/**
 * @defgroup conf_dir
 *
 * @{
 */

/* defined in conf/objs/dir.c */
M0_TL_DESCR_DECLARE(m0_conf_dir, M0_EXTERN);
M0_TL_DECLARE(m0_conf_dir, M0_INTERNAL, struct m0_conf_obj);

/**
 * Adds object to directory.
 *
 * @pre obj->co_status == M0_CS_READY
 * @pre m0_conf_obj_type(obj) == dir->cd_item_type
 */
M0_INTERNAL void m0_conf_dir_add(struct m0_conf_dir *dir,
				 struct m0_conf_obj *obj);

/**
 * Deletes object from directory.
 *
 * @pre obj->co_status == M0_CS_READY
 * @pre m0_conf_obj_type(obj) == dir->cd_item_type
 */
M0_INTERNAL void m0_conf_dir_del(struct m0_conf_dir *dir,
				 struct m0_conf_obj *obj);

/** Compares fids with directory entries. */
M0_INTERNAL bool m0_conf_dir_elems_match(const struct m0_conf_dir *dir,
					 const struct m0_fid_arr  *fids);

static inline uint32_t m0_conf_dir_len(const struct m0_conf_dir *dir)
{
	return m0_conf_dir_tlist_length(&dir->cd_items);
}

/**
 * Creates new m0_conf_dir, populates it with stub objects, and links to parent.
 *
 * @param parent         Parent of this directory.
 * @param children_type  Type of entries.
 * @param children_ids   [optional] Identifiers of the entries.
 * @param[out] out       Resulting pointer.
 *
 * m0_conf_dir_new() is transactional: if it fails, the configuration cache
 * (both DAG of objects and m0_conf_cache registry) is left unchanged.
 *
 * XXX @todo UT transactional property of m0_conf_dir_new().
 */
M0_INTERNAL int m0_conf_dir_new(struct m0_conf_obj *parent,
				const struct m0_fid *relfid,
				const struct m0_conf_obj_type *children_type,
				const struct m0_fid_arr *children_ids,
				struct m0_conf_dir **out);

/** @} conf_dir */
#endif /* __MOTR_CONF_DIR_H__ */
