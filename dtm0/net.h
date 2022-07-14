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

#ifndef __MOTR___DTM0_NET_H__
#define __MOTR___DTM0_NET_H__

#include "fid/fid.h"         /* m0_fid */
#include "fid/fid_xc.h"      /* m0_fid_xc */
#include "lib/buf.h"         /* m0_buf */
#include "lib/buf_xc.h"      /* m0_buf_xc */
#include "be/queue.h"        /* m0_be_queue */
#include "dtm0/tx_desc.h"    /* m0_dtm0_tx_desc */
#include "dtm0/tx_desc_xc.h" /* m0_dtm0_tx_desc_xc */

/**
 * @defgroup dtm0
 *
 * @{
 */

/*
 * DTM0 transport overview
 * -----------------------
 *
 *   DTM0 transport is a component responsible for sending and receiving
 * of DTM0 messages (P, REDO, EOL and so on) over Motr RPC layer.
 * Transport contains a fixed amount of free buffers in snd and rcv queues.
 * Because of that, the caller may need to wait until data is processed.
 */

struct m0_be_op;
struct m0_buf;

struct m0_dtm0_redo {
	struct m0_dtm0_tx_desc redo_txd;
	struct m0_buf          redo_udata;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_dtm0_msg_redo {
	uint64_t             size;
	struct m0_dtm0_redo *buf;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

struct m0_dtm0_msg_eol {
	struct m0_fid dme_initiator;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_dtm0_tid_array {
	uint64_t            size;
	struct m0_dtm0_tid *buf;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

struct m0_dtm0_msg_persistent {
	struct m0_dtm0_tid_array  dmp_tid_array;
	struct m0_fid             dmp_initiator;
	/* XXX: We temporary allow the whole txd to be sent. */
	struct m0_dtm0_tx_desc    dmp_txd;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

enum m0_dtm0_msg_type {
	M0_DMT_REDO,
	M0_DMT_PERSISTENT,
	M0_DMT_BLOB,
	M0_DMT_EOL,
	M0_DMT_NR,
} M0_XCA_ENUM;

struct m0_dtm0_msg_blob {
	struct m0_buf datum;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_dtm0_msg {
	uint32_t dm_type M0_XCA_FENUM(m0_dtm0_msg_type);
	union {
		struct m0_dtm0_msg_redo       redo
			M0_XCA_TAG(_QUOTE(M0_DMT_REDO));
		struct m0_dtm0_msg_eol        eol
			M0_XCA_TAG(_QUOTE(M0_DMT_EOL));
		struct m0_dtm0_msg_persistent persistent
			M0_XCA_TAG(_QUOTE(M0_DMT_PERSISTENT));
		struct m0_dtm0_msg_blob       blob
			M0_XCA_TAG(_QUOTE(M0_DMT_BLOB));
	} dm_msg;
} M0_XCA_UNION M0_XCA_DOMAIN(rpc);

#define M0_DTM0_MSG_EOL_INIT(__initiator) (struct m0_dtm0_msg) { \
	.dm_type = M0_DMT_EOL,                                   \
	.dm_msg.eol = {	                                         \
		.dme_initiator = *(__initiator),                 \
	}                                                        \
}

struct m0_dtm0_net_cfg {
	struct m0_fid   dnc_instance_fid;
	uint64_t        dnc_inflight_max_nr;
	struct m0_reqh *dnc_reqh;
};

struct m0_dtm0_net {
	struct m0_dtm0_net_cfg dnet_cfg;

	struct m0_be_queue     dnet_input[M0_DMT_NR];
};

enum m0_dtm0_remote_state {
	M0_DRS_ALIVE,
	M0_DRS_DEAD,
};

M0_INTERNAL int m0_dtm0_net_init(struct m0_dtm0_net     *dnet,
				 struct m0_dtm0_net_cfg *dnet_cfg);
M0_INTERNAL void m0_dtm0_net_fini(struct m0_dtm0_net  *dnet);

/* IO path */
M0_INTERNAL void m0_dtm0_net_send(struct m0_dtm0_net       *dnet,
				  struct m0_be_op          *op,
				  const struct m0_fid      *target,
				  const struct m0_dtm0_msg *msg,
				  const uint64_t           *parent_sm_id);

M0_INTERNAL void m0_dtm0_net_recv(struct m0_dtm0_net       *dnet,
				  struct m0_be_op          *op,
				  bool                     *success,
				  struct m0_dtm0_msg       *msg,
				  enum m0_dtm0_msg_type     type);

/* Control path */
M0_INTERNAL void m0_dtm0_net_remote_add(struct m0_dtm0_net  *dnet,
					struct m0_be_op     *op,
					const struct m0_fid *remote_id,
					const char          *remote_ep);

M0_INTERNAL void m0_dtm0_net_remote_remove(struct m0_dtm0_net  *dnet,
					   struct m0_be_op     *op,
					   const struct m0_fid *remote_id);

M0_INTERNAL void m0_dtm0_remote_state_set(struct m0_dtm0_net        *dnet,
					  struct m0_be_op           *op,
					  const struct m0_fid       *remote_id,
					  enum m0_dtm0_remote_state  state);
/* Internal */
M0_INTERNAL void m0_dtm0_net_recv__post(struct m0_dtm0_net       *dnet,
					struct m0_be_op          *op,
					const struct m0_dtm0_msg *msg);
M0_INTERNAL struct m0_dtm0_msg *m0_dtm0_msg_dup(const struct m0_dtm0_msg *msg);
M0_INTERNAL int m0_dtm0_msg_copy(struct m0_dtm0_msg *dst,
				 const struct m0_dtm0_msg *src);

M0_INTERNAL int m0_dtm0_net_mod_init(void);
M0_INTERNAL void m0_dtm0_net_mod_fini(void);
/** @} end of dtm0 group */
#endif /* __MOTR___DTM0_NET_H__ */

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
