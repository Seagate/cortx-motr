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

#include "fid/fid.h" /* m0_fid */
#include "lib/vec.h" /* m0_bufvec */

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

struct m0_dtm0_msg_redo {
	/** Vector of m0_dtm0_log_record buffers. */
	struct m0_bufvec dmr_bufs;
};

struct m0_dtm0_msg_eol {
	struct m0_fid dme_initiator;
};

struct m0_dtm0_msg_persistent {
	/** Vector of m0_dtm0_tid buffers. */
	struct m0_bufvec  dmp_tid_array;
	struct m0_fid     dmp_initiator;
};

enum m0_dtm0_msg_type {
	M0_DMT_REDO,
	M0_DMT_PERSISTENT,
	M0_DMT_EOL,
};

struct m0_dtm0_msg {
	enum m0_dtm0_msg_type dm_type;
	union {
		struct m0_dtm0_msg_redo       redo;
		struct m0_dtm0_msg_eol        eol;
		struct m0_dtm0_msg_persistent persistent;
	} dm_msg;
};

struct m0_dtm0_net_cfg {
	struct m0_fid dnc_instance_fid;
	uint64_t      dnc_inflight_max_nr;
};


struct m0_dtm0_net {
	struct m0_dtm0_net_cfg dnet_cfg;
};

M0_INTERNAL int m0_dtm0_net_init(struct m0_dtm0_net     *dnet,
				 struct m0_dtm0_net_cfg *dnet_cfg);
M0_INTERNAL void m0_dtm0_net_fini(struct m0_dtm0_net  *dnet);

M0_INTERNAL void m0_dtm0_net_send(struct m0_dtm0_net       *dnet,
				  struct m0_be_op          *op,
				  const struct m0_fid      *target,
				  const struct m0_dtm0_msg *msg);

M0_INTERNAL void m0_dtm0_net_recv(struct m0_dtm0_net       *dnet,
				  struct m0_be_op          *op,
				  struct m0_fid            *source,
				  struct m0_dtm0_msg       *msg,
				  enum m0_dtm0_msg_type     type);


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
