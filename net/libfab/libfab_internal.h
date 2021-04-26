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

extern struct m0_net_xprt m0_net_libfab_xprt;

/**
 * @defgroup netlibfab
 *
 * @{
 */

#define LIBFAB_VERSION FI_VERSION(1,11)

#define LIBFAB_ADDR_LEN_MAX	INET6_ADDRSTRLEN
#define LIBFAB_PORT_LEN_MAX	6
#define LIBFAB_ADDR_STRLEN_MAX  (LIBFAB_ADDR_LEN_MAX + LIBFAB_PORT_LEN_MAX + 1)

/**
 * Parameters required for libfabric configuration
 */
enum m0_fab__libfab_params {
	/** Fabric memory access. */
	FAB_MR_ACCESS     = (FI_READ | FI_WRITE | FI_RECV | FI_SEND | \
			     FI_REMOTE_READ | FI_REMOTE_WRITE),
	/** Fabric memory offset. */
	FAB_MR_OFFSET     = 0,
	/** Fabric memory flag. */
	FAB_MR_FLAG       = 0,
	/** Key used for memory registration. */
	FAB_MR_KEY        = 0xABCD,
	/** Max number of IOV in send/recv/read/write command */
	FAB_IOV_MAX       = 256,
	/** Dummy data used to notify remote end for rma op completions */
	FAB_DUMMY_DATA    = 0xFABC0DE,
	/** Max number of completion events to read from a CQ */
	FAB_MAX_COMP_READ = 8,
	/** Max timeout for waiting on fd in epoll_wait */
	FAB_WAIT_FD_TMOUT = 1000,
	/** Max event entries for active endpoint EQ */
	FAB_MAX_AEP_EQ_EV = 8,
	/** Max event entries for passive endpoint EQ */
	FAB_MAX_PEP_EQ_EV = 256,
	/** Max entries in shared Tx CQ */
	FAB_MAX_TX_CQ_EV = 1024,
	/** Max entries in Rx CQ */
	FAB_MAX_RX_CQ_EV = 64,
};

/**
 * Represents the state of a libfab active endpoint
 */
enum m0_fab__conn_status {
	FAB_NOT_CONNECTED,
	FAB_CONNECTING,
	FAB_CONNECTED
};

/**
 * Libfab structure of list of fabric interfaces in a transfer machine
 */
struct m0_fab__list {
	struct m0_tl fl_head;
};

/**
 * Libfab structure of fabric params
 */
struct m0_fab__fab {
	/** Magic number */
	uint64_t           fab_magic;

	/** Fabric interface info */
	struct fi_info    *fab_fi;
	
	/** Fabric fid */
	struct fid_fabric *fab_fab;

	/** Domain fid */
	struct fid_domain *fab_dom;

	/** List of fabrics */
	struct m0_tlink    fab_link;
};

/**
 * Libfab structure of endpoint name
 */
struct m0_fab__ep_name {
	/** IP address */
	char fen_addr[LIBFAB_ADDR_LEN_MAX];

	/** Port range 0-65535 */
	char fen_port[LIBFAB_PORT_LEN_MAX];

	/** address in string format as passed by the net layer */
	char fen_str_addr[LIBFAB_ADDR_STRLEN_MAX];
};

/**
 * Libfab structure of resources associated to a passive endpoint
 */
struct m0_fab__pep_res{
	/* Event queue for pep*/
	struct fid_eq *fpr_eq;
};

/**
 * Libfab structure of resources associated to a active tx endpoint
 */
struct m0_fab__tx_res{
	/* Event queue for txep*/
	struct fid_eq *ftr_eq;
};

/**
 * Libfab structure of resources associated to a active rx endpoint
 */
struct m0_fab__rx_res{
	/* Event queue for rxep*/
	struct fid_eq *frr_eq;
	
	/* Rx Completion Queue */
	struct fid_cq *frr_cq;
};

/**
 * Libfab structure of active endpoint
 */
struct m0_fab__active_ep {
	/** tx endpoint */
	struct fid_ep            *aep_txep;

	/** rx endpoint */
	struct fid_ep            *aep_rxep;

	/** tx endpoint resources */
	struct m0_fab__tx_res     aep_tx_res;

	/** rx endpoint resources */
	struct m0_fab__rx_res     aep_rx_res;
	
	/** connection status of tx ep */
	enum m0_fab__conn_status  aep_tx_state;
	
	/* connection status of rx ep */
	enum m0_fab__conn_status  aep_rx_state;
};

/**
 * Libfab structure of passive endpoint
 */
struct m0_fab__passive_ep {
	/** Passive endpoint */
	struct fid_pep           *pep_pep;
	
	/** Active endpoint for structure used for loopback in a tm */
	struct m0_fab__active_ep *pep_aep;
	
	/** Endpoint resources */
	struct m0_fab__pep_res    pep_res;
};

/**
 * Libfab structure of endpoint
 */
struct m0_fab__ep {
	/** linked into a per-tm list */
	struct m0_net_end_point    fep_nep;
	
	/** ipaddr, port and strname */
	struct m0_fab__ep_name     fep_name;
	
	/** Active endpoint */
	struct m0_fab__active_ep  *fep_aep;

	/** Passive endpointt */
	struct m0_fab__passive_ep *fep_listen;
	
	/** List of buffers to send */
	struct m0_tl               fep_sndbuf;
};

/**
 * Libfab structure of transfer machine
 */
struct m0_fab__tm {
	/** Net transfer machine */
	struct m0_net_transfer_mc *ftm_ntm;
	
	/** Poller thread */
	struct m0_thread           ftm_poller;
	
	/** Epoll fd */
	int                        ftm_epfd;
	
	/** Fabric params of a transfer machine */
	struct m0_fab__fab        *ftm_fab;

	/** Passive ep(listening mode) */
	struct m0_fab__ep         *ftm_pep;
	
	/** Shared recv context */
	struct fid_ep             *ftm_rctx;
	
	/** Transmit Completion Queue */
	struct fid_cq             *ftm_tx_cq;
	
	/** tm Shutdown flag */
	bool                       ftm_shutdown;

	/** List of completed buffers */
	struct m0_tl               ftm_done;
	
	/** Used betn poller & tm_fini */
	struct m0_mutex            ftm_endlock;
	
	/** Used betn poller & tm_fini */
	struct m0_mutex            ftm_evpost;

	/** Used as lock during bulk op to enable only txcq reads */
	volatile bool              ftm_txcq_only;
};

/**
 * Libfab structure of buffer memory region params
 */
struct m0_fab__buf_mr {
	/** Buffer descriptor */
	void          *bm_desc[FAB_IOV_MAX];
	
	/** Libfab memory region */
	struct fid_mr *bm_mr[FAB_IOV_MAX];
	
	/** Memory registration key */
	uint64_t       bm_key[FAB_IOV_MAX];
};

/**
 * Libfab structure of buffer descriptor 
 * sent from the passive side to the active side
 */
struct m0_fab__bdesc {
	/** Remote buffer iov cnt */
	uint64_t fbd_iov_cnt;

	/** Remote node addr */
	uint64_t fbd_netaddr;
	
	/** Remote buffer addr */
	uint64_t fbd_bufptr;
};

/**
 * Libfab structure of buffer params
 */
struct m0_fab__buf {
	/** Magic number for list of completed buffers */
	uint64_t               fb_magic;
	
	/** Magic number for list of send buffers */
	uint64_t               fb_sndmagic;
	
	/** Dummy data + network buffer ptr */
	uint64_t               fb_dummy[2];
	
	/** Buffer descriptor of the remote node */
	struct m0_fab__bdesc  *fb_rbd;
	
	/** Array of iov for remote node  */
	struct fi_rma_iov     *fb_riov;
	
	/** Buffer memory region params */
	struct m0_fab__buf_mr  fb_mr;
	
	/** Domain to which the buf is reg */
	struct fid_domain     *fb_dp;
	
	/** Pointer back to network buffer*/
	struct m0_net_buffer  *fb_nb;
	
	/** endpoint associated with recv buffer operation */
	struct m0_fab__ep     *fb_ev_ep;
	
	/** Context to be returned in the buffer completion event for tx ops*/
	struct m0_fab__ep     *fb_txctx;
	
	/** Link in list of completed bufs*/
	struct m0_tlink        fb_linkage;
	
	/** Link for list of send buffers */
	struct m0_tlink        fb_snd_link;

	/** Buffer completion status */
	int32_t                fb_status;
	
	/** Total size of data to be rcvd*/
	m0_bindex_t            fb_length;
	
	/** Count of work request generated for bulk rma ops */
	volatile uint32_t      fb_wr_cnt;

	/** Count of work request completions for bulk rma ops */
	volatile uint32_t      fb_wr_comp_cnt;

	/** Flag to denote that bulk op on all segments is done */
	volatile bool          fb_all_seg_done;
};

/**
 * Libfab structure of connection data
 */
struct m0_fab__conn_data {
	/** address in network byte format */
	uint64_t fcd_netaddr;
	
	/** address in string format */
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

