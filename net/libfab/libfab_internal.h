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

/* #define LIBFAB_VERSION FI_VERSION(FI_MAJOR_VERSION,FI_MINOR_VERSION) */
#define LIBFAB_VERSION FI_VERSION(1,11)
#define LIBFAB_WAITSET_TIMEOUT    2 /* in msec TODO: Tbd  */

#define LIBFAB_ADDR_LEN_MAX	INET6_ADDRSTRLEN
#define LIBFAB_PORT_LEN_MAX	6
#define LIBFAB_ADDR_STRLEN_MAX  (LIBFAB_ADDR_LEN_MAX + LIBFAB_PORT_LEN_MAX + 1)

/** Parameters required for libfabric configuration */
enum m0_fab__mr_params {
	/** Fabric memory access. */
	FAB_MR_ACCESS  = (FI_READ | FI_WRITE | FI_RECV | FI_SEND | \
			  FI_REMOTE_READ | FI_REMOTE_WRITE),
	/** Fabric memory offset. */
	FAB_MR_OFFSET  = 0,
	/** Fabric memory flag. */
	FAB_MR_FLAG    = 0,
	/** Key used for memory registration. */
	FAB_MR_KEY     = 0XABCD,
	/** Max number of IOV in send/recv/read/write command */
	FAB_MR_IOV_MAX = 256,
};

enum PORT_SOCK_TYPE {
	PORTFAMILYMAX = 3,
	SOCKTYPEMAX   = 2
};

enum m0_fab__conn_status {
	FAB_NOT_CONNECTED,
	FAB_CONNECTING,
	FAB_CONNECTED
};

struct m0_fab__list {
	struct m0_tl fl_head;
};

struct m0_fab__fab {
	uint64_t           fab_magic;          /* Magic number */
	struct fi_info    *fab_fi;             /* Fabric interface info */
	struct fid_fabric *fab_fab;            /* Fabric fid */
	struct fid_domain *fab_dom;            /* Domain fid */
	struct m0_tlink    fab_link;           /* List of fabrics */
};

struct m0_fab__ep_name {
	char fen_addr[LIBFAB_ADDR_LEN_MAX];    /*  */
	char fen_port[LIBFAB_PORT_LEN_MAX];    /* Port range 0-65535 */
	char fen_str_addr[LIBFAB_ADDR_STRLEN_MAX];
};

struct m0_fab__ep_res {
	struct fid_eq *fer_eq;                 /* Event queue */
	struct fid_cq *fer_rx_cq;              /* Recv Completion Queue */
};

struct m0_fab__active_ep {
	struct fid_ep            *aep_ep;      /* Active Endpoint */
	struct m0_fab__ep_res     aep_ep_res;  /* Endpoint resources */
	enum m0_fab__conn_status  aep_state;   /* EP connection status */
};

struct m0_fab__passive_ep {
	struct fid_pep           *pep_pep;      /* Passive endpoint */
	struct m0_fab__active_ep *pep_tx_ep;
	struct m0_fab__active_ep *pep_rx_ep;
	struct m0_fab__ep_res     pep_ep_res;   /* Endpoint resources */
};

struct m0_fab__ep {
	struct m0_net_end_point    fep_nep;     /* linked into a per-tm list */
	struct m0_fab__ep_name     fep_name;    /* "addr:port" in str format */
	struct m0_fab__active_ep  *fep_send;
	struct m0_fab__passive_ep *fep_listen;
	struct m0_tl               fep_sndbuf;  /* List of buffers to send */
};

struct m0_fab__tm {
	struct m0_net_transfer_mc *ftm_net_ma;  /* Generic transfer machine */
	struct m0_thread           ftm_poller;  /* Poller thread */
	struct fid_wait           *ftm_waitset;
	struct m0_fab__fab        *ftm_fab;
	struct m0_fab__ep         *ftm_pep;     /* Passive ep(listening mode) */
	struct fid_ep             *ftm_rctx;    /* Shared recv context */
	struct fid_cq             *ftm_tx_cq;   /* Transmit Completion Queue */
	bool                       ftm_shutdown;/* tm Shutdown flag */
	struct m0_tl               ftm_done;    /* List of completed buffers */
	struct m0_mutex            ftm_endlock; /* Used betn poller & tm_fini */
	struct m0_mutex            ftm_evpost;  /* Used betn poller & tm_fini */
};

/**
 *    Private data pointed to by m0_net_buffer::nb_xprt_private.
 *
 */

struct m0_fab__buf_mr {
	void          *bm_desc[FAB_MR_IOV_MAX]; /* Buffer descriptor */
	struct fid_mr *bm_mr[FAB_MR_IOV_MAX];   /* Libfab memory region */
	uint64_t       bm_key[FAB_MR_IOV_MAX];  /* Memory registration key */
};

struct m0_fab__buf {
	uint64_t               fb_magic;   /* Magic number */
	uint64_t               fb_sndmagic;/* Magic number */
	uint64_t               fb_rc_buf;  /* For remote completions */
	uint64_t               fb_rem_key;
	struct m0_fab__buf_mr  fb_mr;
	struct fid_domain     *fb_dp;      /* Domain to which the buf is reg */
	struct m0_net_buffer  *fb_nb;      /* Pointer back to network buffer*/
	struct m0_fab__ep     *fb_ev_ep;
	struct m0_fab__ep     *fb_txctx;
	struct m0_tlink        fb_linkage; /* Link in list of completed bufs*/
	struct m0_tlink        fb_snd_link;
	int32_t                fb_status;  /* Buffer completion status */
	m0_bindex_t            fb_length;  /* Total size of data to be rcvd*/
};

struct m0_fab__conn_data {
	uint64_t fcd_netaddr;
	char     fcd_straddr[LIBFAB_ADDR_STRLEN_MAX];
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

