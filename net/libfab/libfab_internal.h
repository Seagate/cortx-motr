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

#ifndef __MOTR_NET_LIBFAB_LIBFAB_INTERNAL_H__
#define __MOTR_NET_LIBFAB_LIBFAB_INTERNAL_H__
#include <netinet/in.h>                  /* INET_ADDRSTRLEN */

#include "rdma/fabric.h"
#include "rdma/fi_cm.h"
#include "rdma/fi_domain.h"
#include "rdma/fi_eq.h"
#include "rdma/fi_endpoint.h"
#include "rdma/fi_rma.h"
#include "lib/cookie.h"

extern struct m0_net_xprt m0_net_libfab_xprt;

/**
 * @defgroup netlibfab
 *
 * @{
 */

struct m0_fab__fab {
	struct fi_info    *fab_fi;             /* Fabric interface info */
	struct fid_fabric *fab_fab;            /* Fabric fid */
};

struct m0_fab__ep_name {
	char fen_addr[INET6_ADDRSTRLEN];       /*  */
	char fen_port[6];                      /* Port range 0-65535 */
	char fen_str_addr[INET6_ADDRSTRLEN+6+1];
};

struct m0_fab__ep_res {
	struct fid_eq *fer_eq;                 /* Event queue */
	struct fid_cq *fer_tx_cq;              /* Transmit Completion Queue */
	struct fid_cq *fer_rx_cq;              /* Recv Completion Queue */
};

struct m0_fab__active_ep {
	struct fid_ep         *aep_ep;         /* Active Endpoint */
	struct fid_domain     *aep_dom;        /* Domain fid */
	struct m0_fab__ep_res  aep_ep_res;     /* Endpoint resources */
	bool                   aep_is_conn;    /* Is ep in connected state */
};

struct m0_fab__passive_ep {
	struct fid_pep        *pep_pep;        /* Passive endpoint */
	struct fid_domain     *pep_dom;        /* Domain fid */
	struct m0_fab__ep_res  pep_ep_res;     /* Endpoint resources */
};

struct m0_fab__ep {
	struct m0_net_end_point    fep_nep;     /* linked into a per-tm list */
	struct m0_fab__ep_name     fep_name;    /* "addr:port" in str format */
	struct m0_fab__active_ep  *fep_send;
	struct m0_fab__active_ep  *fep_recv;
	struct m0_fab__passive_ep *fep_listen;
	struct m0_tl               fep_sndbuf;  /* List of buffers to send */
};

struct m0_fab__tm {
	struct m0_net_transfer_mc *ftm_net_ma;  /* Generic transfer machine */
	struct m0_thread           ftm_poller;  /* Poller thread */
	struct fid_wait           *ftm_waitset;
	struct m0_fab__fab         ftm_fab;
	struct m0_fab__ep         *ftm_pep;     /* Passive ep(listening mode) */
	bool                       ftm_shutdown;/* tm Shutdown flag */
	struct m0_tl               ftm_rcvbuf;  /* List of recv buffers */
	struct m0_tl               ftm_done;    /* List of completed buffers */
	struct m0_mutex            ftm_endlock; /* Used betn poller & tm_fini */
	struct m0_mutex            ftm_evpost;  /* Used betn poller & tm_fini */
};

/**
 *    Private data pointed to by m0_net_buffer::nb_xprt_private.
 *
 */
struct m0_fab__buf {
	uint64_t              fb_magic;   /* Magic number */
	uint64_t              fb_rcvmagic;/* Magic number */
	uint64_t              fb_sndmagic;/* Magic number */
	uint64_t              fb_mr_key;  /* Memory registration key */
	uint64_t              fb_rc_buf;  /* For remote completetions */
	void                 *fb_mr_desc; /* Buffer descriptor */
	struct fid_domain    *fb_dp;      /* Domain to which the buf is reg */
	struct m0_net_buffer *fb_nb;      /* Pointer back to network buffer*/
	struct fid_mr        *fb_mr;      /* Libfab memory region */
	struct m0_fab__ep    *fb_ev_ep;
	struct m0_tlink       fb_linkage; /* Linkage in list of completed bufs*/
	struct m0_tlink       fb_rcv_link;
	struct m0_tlink       fb_snd_link;
	int32_t               fb_status;  /* Buffer completion status */
	m0_bindex_t           fb_length;  /* Total size of data to be received*/
};

/** @} end of netlibfab group */
#endif /* __MOTR_NET_LIBFAB_LIBFAB_INTERNAL_H__ */

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

