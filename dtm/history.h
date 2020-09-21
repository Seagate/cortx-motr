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

#ifndef __MOTR_DTM_HISTORY_H__
#define __MOTR_DTM_HISTORY_H__

#include "lib/tlist.h"
#include "lib/queue.h"
#include "lib/cookie.h"

#include "dtm/nucleus.h"

/**
 * @addtogroup dtm
 *
 * @{
 */

/* import */
#include "dtm/operation.h"
struct m0_dtm;
struct m0_dtm_remote;

/* export */
struct m0_dtm_history;
struct m0_dtm_history_ops;
struct m0_dtm_history_type;
struct m0_dtm_history_type_ops;

/**
 * DTM history.
 */
struct m0_dtm_history {
	struct m0_dtm_hi                 h_hi;
	struct m0_tlink                  h_exclink;
	struct m0_tlink                  h_catlink;
	struct m0_dtm_remote            *h_rem;
	struct m0_dtm_update            *h_persistent;
	struct m0_dtm_update            *h_undo;
	struct m0_dtm_update            *h_reint;
	struct m0_dtm_update            *h_known;
	struct m0_dtm_update            *h_reset;
	const struct m0_dtm_history_ops *h_ops;
	uint64_t                         h_gen;
	uint64_t                         h_epoch;
	struct m0_cookie                 h_remcookie;
	m0_dtm_ver_t                     h_max_ver;
};
M0_INTERNAL bool m0_dtm_history_invariant(const struct m0_dtm_history *history);

enum m0_dtm_history_flags {
	M0_DHF_CLOSED = M0_DHF_LAST,
	M0_DHF_AMNESIA
};

struct m0_dtm_history_ops {
	const struct m0_dtm_history_type *hio_type;
	const struct m0_uint128 *(*hio_id)(const struct m0_dtm_history *history);
	void (*hio_persistent)(struct m0_dtm_history *history);
	void (*hio_fixed     )(struct m0_dtm_history *history);
	int  (*hio_update    )(struct m0_dtm_history *history, uint8_t id,
			       struct m0_dtm_update *update);
};

struct m0_dtm_history_type {
	uint8_t                               hit_id;
	uint8_t                               hit_rem_id;
	const char                           *hit_name;
	const struct m0_dtm_history_type_ops *hit_ops;
};

struct m0_dtm_history_type_ops {
	int (*hito_find)(struct m0_dtm *dtm,
			 const struct m0_dtm_history_type *ht,
			 const struct m0_uint128 *id,
			 struct m0_dtm_history **out);
};

struct m0_dtm_controlh {
	struct m0_dtm_history ch_history;
	struct m0_dtm_oper    ch_clop;
	struct m0_dtm_update  ch_clup;
	struct m0_dtm_update  ch_clup_rem;
};

M0_INTERNAL void m0_dtm_history_init(struct m0_dtm_history *history,
				     struct m0_dtm *dtm);
M0_INTERNAL void m0_dtm_history_fini(struct m0_dtm_history *history);

M0_INTERNAL void m0_dtm_history_persistent(struct m0_dtm_history *history,
					   m0_dtm_ver_t upto);
M0_INTERNAL void m0_dtm_history_reset(struct m0_dtm_history *history,
				      m0_dtm_ver_t since);
M0_INTERNAL void m0_dtm_history_undo(struct m0_dtm_history *history,
				     m0_dtm_ver_t upto);
M0_INTERNAL void m0_dtm_history_close(struct m0_dtm_history *history);

M0_INTERNAL void m0_dtm_history_update_get(const struct m0_dtm_history *history,
					   enum m0_dtm_up_rule rule,
					   struct m0_dtm_update_data *data);

M0_INTERNAL void
m0_dtm_history_type_register(struct m0_dtm *dtm,
			     const struct m0_dtm_history_type *ht);
M0_INTERNAL void
m0_dtm_history_type_deregister(struct m0_dtm *dtm,
			       const struct m0_dtm_history_type *ht);
M0_INTERNAL const struct m0_dtm_history_type *
m0_dtm_history_type_find(struct m0_dtm *dtm, uint8_t id);

M0_INTERNAL void m0_dtm_history_pack(const struct m0_dtm_history *history,
				     struct m0_dtm_history_id *id);

M0_INTERNAL int m0_dtm_history_unpack(struct m0_dtm *dtm,
				      const struct m0_dtm_history_id *id,
				      struct m0_dtm_history **out);

M0_INTERNAL void m0_dtm_history_add_nop(struct m0_dtm_history *history,
					struct m0_dtm_oper *oper,
					struct m0_dtm_update *cupdate);
M0_INTERNAL void m0_dtm_history_add_close(struct m0_dtm_history *history,
					  struct m0_dtm_oper *oper,
					  struct m0_dtm_update *cupdate);

M0_INTERNAL void m0_dtm_controlh_init(struct m0_dtm_controlh *ch,
				      struct m0_dtm *dtm);
M0_INTERNAL void m0_dtm_controlh_fini(struct m0_dtm_controlh *ch);
M0_INTERNAL void m0_dtm_controlh_add(struct m0_dtm_controlh *ch,
				     struct m0_dtm_oper *oper);
M0_INTERNAL void m0_dtm_controlh_close(struct m0_dtm_controlh *ch);
M0_INTERNAL int m0_dtm_controlh_update(struct m0_dtm_history *history,
				       uint8_t id,
				       struct m0_dtm_update *update);
M0_INTERNAL bool
m0_dtm_controlh_update_is_close(const struct m0_dtm_update *update);

M0_INTERNAL void m0_dtm_controlh_fuse_close(struct m0_dtm_update *update);

/** @} end of dtm group */

#endif /* __MOTR_DTM_HISTORY_H__ */


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
