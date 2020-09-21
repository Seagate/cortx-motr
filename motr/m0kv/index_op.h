/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_M0INDEX_OP_H__
#define __MOTR_M0INDEX_OP_H__

/**
 * @defgroup client
 *
 * @{
 */
struct m0_realm;
struct m0_fid_arr;
struct m0_fid;
struct m0_bufvec;

int index_create(struct m0_realm *parent,
		 struct m0_fid_arr *fids);
int index_drop(struct m0_realm *parent, struct m0_fid_arr *fids);
int index_list(struct m0_realm  *parent,
	       struct m0_fid    *fid,
	       int               cnt,
	       struct m0_bufvec *keys);
int index_lookup(struct m0_realm   *parent,
		 struct m0_fid_arr *fids,
		 struct m0_bufvec  *rets);
int index_put(struct m0_realm   *parent,
	      struct m0_fid_arr *fids,
	      struct m0_bufvec  *keys,
	      struct m0_bufvec  *vals);
int index_del(struct m0_realm   *parent,
	      struct m0_fid_arr *fids,
	      struct m0_bufvec  *keys);
int index_get(struct m0_realm  *parent,
	      struct m0_fid    *fid,
	      struct m0_bufvec *keys,
	      struct m0_bufvec *vals);
int index_next(struct m0_realm  *parent,
	       struct m0_fid    *fid,
	       struct m0_bufvec *keys, int cnt,
	       struct m0_bufvec *vals);

/** @} end of client group */
#endif /* __MOTR_M0INDEX_OP_H__ */

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
