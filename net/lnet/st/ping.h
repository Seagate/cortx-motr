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

#ifndef __MOTR_NET_LNET_PING_H__
#define __MOTR_NET_LNET_PING_H__

#include "lib/bitmap.h" /* m0_bitmap */

struct nlx_ping_ctx;
struct nlx_ping_ops {
	int (*pf)(const char *format, ...)
		__attribute__ ((format (printf, 1, 2)));
	void (*pqs)(struct nlx_ping_ctx *ctx, bool reset);
};

/**
   Context for a ping client or server.
 */
struct nlx_ping_ctx {
	const struct nlx_ping_ops             *pc_ops;
	const struct m0_net_xprt              *pc_xprt;
	struct m0_net_domain                   pc_dom;
	const char                            *pc_network;/* "addr@interface" */
	uint32_t                               pc_pid;
	uint32_t                               pc_portal;
	int32_t                                pc_tmid; /* initialized to < 0 */
	const char                            *pc_rnetwork;
	uint32_t                               pc_rpid;
	uint32_t                               pc_rportal;
	int32_t                                pc_rtmid;
	int32_t                                pc_status;
	uint32_t                               pc_nr_bufs;
	uint32_t                               pc_nr_recv_bufs;
	uint32_t                               pc_segments;
	uint32_t                               pc_seg_size;
	uint32_t                               pc_seg_shift;
	int                                    pc_min_recv_size;
	int                                    pc_max_recv_msgs;
	uint64_t                               pc_bulk_size;
	struct m0_net_buffer                  *pc_nbs;
	const struct m0_net_buffer_callbacks  *pc_buf_callbacks;
	struct m0_bitmap                       pc_nbbm;
	struct m0_net_transfer_mc              pc_tm;
	struct m0_mutex                        pc_mutex;
	struct m0_cond                         pc_cond;
	struct m0_list                         pc_work_queue;
	const char                            *pc_ident;
	const char                            *pc_compare_buf;
	int                                    pc_bulk_timeout;
	int                                    pc_msg_timeout;
	int                                    pc_server_bulk_delay;
	int                                    pc_dom_debug;
	int                                    pc_tm_debug;
	bool                                   pc_ready;
	char                                 **pc_interfaces;
	bool                                   pc_sync_events;
	struct m0_chan                         pc_wq_chan;
	struct m0_clink                        pc_wq_clink;
	uint64_t                               pc_wq_signal_count;
	struct m0_chan                         pc_net_chan;
	struct m0_clink                        pc_net_clink;
	uint64_t                               pc_net_signal_count;
	uint64_t                               pc_blocked_count;
	uint64_t                               pc_worked_count;
	struct m0_atomic64                     pc_errors;
	struct m0_atomic64                     pc_retries;
	int                                    pc_verbose;
};

struct nlx_ping_client_params {
	const struct nlx_ping_ops *ops;
	int loops;
	unsigned int nr_bufs;
	int client_id;
	uint64_t bulk_size;
	int send_msg_size;
	int bulk_timeout;
	int msg_timeout;
	const char *client_network;
	uint32_t    client_pid;
	uint32_t    client_portal;
	int32_t	    client_tmid;
	const char *server_network;
	uint32_t    server_pid;
	uint32_t    server_portal;
	int32_t	    server_tmid;
	int         debug;
	int         verbose;
};

enum {
	PING_CLIENT_PORTAL = 39,
	PING_CLIENT_DYNAMIC_TMID = -1,
	PING_SERVER_PORTAL = 39,
	PING_SERVER_TMID = 12,

	PING_DEF_BUFS = 20,
	PING_MIN_BUFS = 4,
	PING_DEF_CLIENT_THREADS = 1,
	PING_MAX_CLIENT_THREADS = 32,
	PING_DEF_LOOPS = 1,

	PING_DEF_MSG_TIMEOUT = 5,
	PING_DEF_BULK_TIMEOUT = 10,

	PING_SEGMENT_SIZE    = 4096,
	PING_SEGMENT_SHIFT   = 12,
	PING_MAX_SEGMENTS    = 256,
	PING_MAX_BUFFER_SIZE = PING_MAX_SEGMENTS * PING_SEGMENT_SIZE,
	PING_DEF_SEGMENTS    = 8,
	PING_DEF_BUFFER_SIZE = PING_DEF_SEGMENTS * PING_SEGMENT_SIZE,

	PING_MSG_OVERHEAD = 2, /* Msg type byte + '\0' */

	PING_DEF_MIN_RECV_SIZE = 100, /* empirical observation: 58 */

	ONE_MILLION = 1000000ULL,
	SEC_PER_HR = 60 * 60,
	SEC_PER_MIN = 60,
};

/* Debug printf macro */
#ifdef __KERNEL__
#define PING_ERR(fmt, ...) printk(KERN_ERR fmt , ## __VA_ARGS__)
#else
#include <stdio.h>
#define PING_ERR(fmt, ...) fprintf(stderr, fmt , ## __VA_ARGS__)
#endif

#define PING_OUT(ctx, num, fmt, ...)			\
do {							\
	if ((ctx)->pc_verbose >= num)			\
		(ctx)->pc_ops->pf(fmt , ## __VA_ARGS__);\
} while (0)

void nlx_ping_server_spawn(struct m0_thread *server_thread,
			   struct nlx_ping_ctx *sctx);
void nlx_ping_server_should_stop(struct nlx_ping_ctx *ctx);
void nlx_ping_client(struct nlx_ping_client_params *params);

void nlx_ping_print_qstats_tm(struct nlx_ping_ctx *ctx, bool reset);
int  nlx_ping_print_qstats_total(const char *ident,
				 const struct nlx_ping_ops *ops);
uint64_t nlx_ping_parse_uint64(const char *s);
void nlx_ping_init(void);
void nlx_ping_fini(void);

#endif /* __MOTR_NET_LNET_PING_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
