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

#ifndef __MOTR_FID_FID_H__
#define __MOTR_FID_FID_H__

/**
   @defgroup fid File identifier

   @{
 */

/* import */
#include "lib/types.h"
#include "xcode/xcode_attr.h"

struct m0_fid {
	uint64_t f_container;
	uint64_t f_key;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_fid_arr {
	uint32_t       af_count;
	struct m0_fid *af_elems;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(conf|rpc);

M0_INTERNAL bool m0_fid_is_set(const struct m0_fid *fid);
M0_INTERNAL bool m0_fid_is_valid(const struct m0_fid *fid);
M0_INTERNAL bool m0_fid_eq(const struct m0_fid *fid0,
			   const struct m0_fid *fid1);
M0_INTERNAL bool m0_proc_fid_eq(const struct m0_fid *fid0,
		                const struct m0_fid *fid1);
M0_INTERNAL int m0_fid_cmp(const struct m0_fid *fid0,
			   const struct m0_fid *fid1);
M0_INTERNAL void m0_fid_set(struct m0_fid *fid,
			    uint64_t container, uint64_t key);
M0_INTERNAL void m0_fid_tset(struct m0_fid *fid,
			     uint8_t tid, uint64_t container, uint64_t key);
/* Get fid type id. */
M0_INTERNAL uint8_t m0_fid_tget(const struct m0_fid *fid);
/* Change fid type id. */
M0_INTERNAL void m0_fid_tchange(struct m0_fid *fid, uint8_t tid);

M0_INTERNAL int m0_fid_sscanf(const char *s, struct m0_fid *fid);
M0_INTERNAL int m0_fid_print(char *s, size_t s_len, const struct m0_fid *fid);

M0_INTERNAL int m0_fid_init(void);
M0_INTERNAL void m0_fid_fini(void);

enum {
	/** Clears high 8 bits off. */
	M0_FID_TYPE_MASK        = 0x00ffffffffffffffULL,
	M0_FID_DYNAMIC_CNT_MASK = 0x00000000FFFFFFFFULL,
	M0_FID_STR_LEN          = 64,
};

#define FID_F "<%" PRIx64 ":%" PRIx64 ">"
#define FID_SF " < %" SCNx64 " : %" SCNx64 " > "
#define FID_P(f)  (f)->f_container,  (f)->f_key
#define FID_S(f) &(f)->f_container, &(f)->f_key

#define M0_FID_TCONTAINER(type, container)		\
	((((uint64_t)(type)) << (64 - 8)) |		\
	 (((uint64_t)(container)) & M0_FID_TYPE_MASK))

#define M0_FID_INIT(container, key)		\
	((struct m0_fid) {			\
		.f_container = (container),	\
		.f_key = (key)			\
	})

#define M0_FID_TINIT(type, container, key)				\
	M0_FID_INIT(M0_FID_TCONTAINER((type), (container)), (key))

#define M0_FID0 M0_FID_INIT(0ULL, 0ULL)

#define M0_FID_BUF(fid) ((struct m0_buf){	\
	.b_nob = sizeof *(fid),			\
	.b_addr = (fid)				\
})

struct m0_fid_type {
	uint8_t     ft_id;
	const char *ft_name;
	bool      (*ft_is_valid)(const struct m0_fid *fid);
};

M0_INTERNAL void m0_fid_type_register(const struct m0_fid_type *fidt);
M0_INTERNAL void m0_fid_type_unregister(const struct m0_fid_type *fidt);
M0_INTERNAL const struct m0_fid_type *m0_fid_type_get(uint8_t id);
M0_INTERNAL const struct m0_fid_type *m0_fid_type_gethi(uint64_t id);
M0_INTERNAL const struct m0_fid_type *
m0_fid_type_getfid(const struct m0_fid *fid);
M0_INTERNAL const struct m0_fid_type *m0_fid_type_getname(const char *name);
M0_INTERNAL void m0_fid_tassume(struct m0_fid *fid,
				const struct m0_fid_type *ft);
M0_INTERNAL void m0_fid_tgenerate(struct m0_fid *fid,
				  const uint8_t  tid);

M0_INTERNAL uint64_t m0_fid_hash(const struct m0_fid *fid);

M0_INTERNAL int m0_fid_arr_copy(struct m0_fid_arr *to,
				const struct m0_fid_arr *from);
M0_INTERNAL bool m0_fid_arr_eq(const struct m0_fid_arr *a,
			       const struct m0_fid_arr *b);
M0_INTERNAL bool m0_fid_arr_all_unique(const struct m0_fid_arr *a);

/** @} end of fid group */
#endif /* __MOTR_FID_FID_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
