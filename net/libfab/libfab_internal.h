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

struct m0_fab__ep_name {
	char fen_addr[INET6_ADDRSTRLEN];       /*  */
	char fen_port[6];                      /* Port range 0-65535 */
	char fen_str_addr[INET6_ADDRSTRLEN+6+1];
};

struct m0_fab__ep_res {
	struct fid_eq           *fer_eq;       /* Event queue */
	struct fid_cq           *fer_tx_cq;    /* Transmit Completion Queue */
	struct fid_cq           *fer_rx_cq;    /* Recv Completion Queue */
	struct fid_cntr         *fer_rc_cntr;  /* Remote Completion Counter */
};

struct m0_fab__ep {
	struct m0_net_end_point  fep_nep;      /* linked into a per-tm list */
	struct m0_fab__ep_name   fep_name;     /* "addr:port" in str format */
	struct fi_info          *fep_fi;       /* Fabric interface info */
	struct fid_fabric       *fep_fabric;   /* Fabric fid */
	struct fid_domain       *fep_domain;   /* Domain fid */
	struct fid_ep           *fep_ep;       /* Active Endpoint */
	struct fid_pep          *fep_pep;      /* Passive endpoint */
	struct m0_fab__ep_res    fep_ep_res;   /* Endpoint resources */
};

struct m0_fab__tm {
	struct m0_net_transfer_mc *ftm_net_ma;  /* Generic transfer machine */
	struct m0_thread           ftm_poller;  /* Poller thread */
	int                        ftm_epollfd; /* epoll(2) file descriptor. */
	struct fid_poll           *ftm_pollset;
	struct fid_wait           *ftm_waitset;
	struct m0_fab__ep         *ftm_pep;     /* Passive ep(listening mode) */
	bool                       ftm_shutdown;/* tm Shutdown flag */
	struct m0_tl               ftm_done;    /* List of completed buffers */
	struct m0_tl               ftm_rcomp;   /* List of remote comp to chk */
	struct m0_mutex            ftm_endlock; /* Used betn poller & tm_fini */
};

/**
 *    Private data pointed to by m0_net_buffer::nb_xprt_private.
 *
 */
struct m0_fab__buf {
	uint64_t              fb_magic;   /* Magic number */
	uint64_t              fbp_cookie; /* Cookie identifying the buffer */
	uint64_t              fb_mr_key;  /* Memory registration key */
	void                 *fb_mr_desc; /* Buffer descriptor */
	struct fid_domain    *fb_dp;      /* Domain to which the buf is reg */
	struct m0_net_buffer *fb_nb;      /* Pointer back to network buffer*/
	struct fid_mr        *fb_mr;      /* Libfab memory region */
	struct m0_fab__ep    *fb_ev_ep;
	struct m0_tlink       fb_linkage; /* Linkage in list of completed bufs*/
	m0_bindex_t           fb_length;  /* Total size of data to be received*/
};

struct m0_fab__rcomp {
	uint64_t         frc_magic;     /* Magic number */
	uint32_t         frc_prv_cnt;   /* Previous counter value */
	struct fid_cntr *frc_cntr;      /* Cntr to check for remote rma events*/
	void            *frc_ctx;       /* Context used to signal completion */
	struct m0_tlink  frc_linkage;   /* Linkage in list of remote comp*/
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

