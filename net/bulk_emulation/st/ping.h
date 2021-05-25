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

#ifndef __MOTR_NET_BULK_MEM_PING_H__
#define __MOTR_NET_BULK_MEM_PING_H__

#include "lib/bitmap.h"
#include "lib/types.h"

struct ping_ctx;
struct ping_ops {
	int (*pf)(const char *format, ...)
		__attribute__ ((format (printf, 1, 2)));
	void (*pqs)(struct ping_ctx *ctx, bool reset);
};

/**
   Context for a ping client or server.
 */
struct ping_ctx {
	const struct ping_ops		     *pc_ops;
	const struct m0_net_xprt	     *pc_xprt;
	struct m0_net_domain		      pc_dom;
	const char		             *pc_hostname; /* dotted decimal */
	short				      pc_port;
	uint32_t			      pc_id;
	int32_t				      pc_status;
	const char			     *pc_rhostname; /* dotted decimal */
	short				      pc_rport;
	uint32_t			      pc_rid;
	uint32_t		              pc_nr_bufs;
	uint32_t		              pc_segments;
	uint32_t		              pc_seg_size;
	int32_t				      pc_passive_size;
	struct m0_net_buffer		     *pc_nbs;
	const struct m0_net_buffer_callbacks *pc_buf_callbacks;
	struct m0_bitmap		      pc_nbbm;
	struct m0_net_transfer_mc	      pc_tm;
	struct m0_mutex			      pc_mutex;
	struct m0_cond			      pc_cond;
	struct m0_list			      pc_work_queue;
	const char		             *pc_ident;
	const char		             *pc_compare_buf;
	int                                   pc_passive_bulk_timeout;
	int                                   pc_server_bulk_delay;
};

enum {
	PING_PORT1 = 12345,
	PING_PORT2 = 27183,
	PART3_SERVER_ID = 141421,
};

/* Debug printf macro */
#ifdef __KERNEL__
#define PING_ERR(fmt, ...) printk(KERN_ERR fmt , ## __VA_ARGS__)
#else
#include <stdio.h>
#define PING_ERR(fmt, ...) fprintf(stderr, fmt , ## __VA_ARGS__)
#endif

void ping_server(struct ping_ctx *ctx);
void ping_server_should_stop(struct ping_ctx *ctx);
int ping_client_init(struct ping_ctx *ctx, struct m0_net_end_point **server_ep);
int ping_client_fini(struct ping_ctx *ctx, struct m0_net_end_point *server_ep);
int ping_client_msg_send_recv(struct ping_ctx *ctx,
			      struct m0_net_end_point *server_ep,
			      const char *data);
int ping_client_passive_recv(struct ping_ctx *ctx,
			     struct m0_net_end_point *server_ep);
int ping_client_passive_send(struct ping_ctx *ctx,
			     struct m0_net_end_point *server_ep,
			     const char *data);

#endif /* __MOTR_NET_BULK_MEM_PING_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
