/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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
	/** Fabric memory registration access. */
	FAB_MR_ACCESS            = (FI_READ | FI_WRITE | FI_RECV | FI_SEND |
				    FI_REMOTE_READ | FI_REMOTE_WRITE),
	/** Fabric memory registration offset. */
	FAB_MR_OFFSET            = 0,
	/** Fabric memory registration flag. */
	FAB_MR_FLAG              = 0,
	/** Key used for memory registration. */
	FAB_MR_KEY               = 0xABCD,
	/** Max number of IOV in read/write command (max number of segments) */
	FAB_IOV_MAX              = 256,
	/** Max segment size for bulk buffers (4k but can be increased) */
	FAB_MAX_BULK_SEG_SIZE    = 4096,
	/** 
	 * Max buffer size = FAB_IOV_MAX x FAB_MAX_SEG_SIZE 
	 * (1MB but can be increased)
	*/
	FAB_MAX_BULK_BUFFER_SIZE = (FAB_IOV_MAX * FAB_MAX_BULK_SEG_SIZE),
	/** Max segment size for rpc buffer ( 1MB but can be changed ) */
	FAB_MAX_RPC_SEG_SIZE     = (1 << 20),
	/** Max number of segments for rpc buffer */
	FAB_MAX_RPC_SEG_NR       = 1,
	/** Max number of recevive messages in rpc buffer */
	FAB_MAX_RPC_RECV_MSG_NR  = 1,
	/** Dummy data used to notify remote end for read-rma op completions */
	FAB_DUMMY_DATA           = 0xFABC0DE,
	/** Max number of completion events to read from a completion queue */
	FAB_MAX_COMP_READ        = 256,
	/** Max timeout for waiting on fd in epoll_wait */
	FAB_WAIT_FD_TMOUT        = 1000,
	/** Max event entries for active endpoint event queue */
	FAB_MAX_AEP_EQ_EV        = 8,
	/** Max event entries for passive endpoint event queue */
	FAB_MAX_PEP_EQ_EV        = 256,
	/** Max entries in shared transmit completion queue */
	FAB_MAX_TX_CQ_EV         = 1024,
	/** Max entries in receive completion queue */
	FAB_MAX_RX_CQ_EV         = 64,
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
 * Represents the state of a libfab transfer machine
 */
enum m0_fab__tm_state {
	FAB_TM_INIT,
	FAB_TM_STARTED,
	FAB_TM_SHUTDOWN
};

/**
 * Represents the state of a libfab buffer
 */
enum m0_fab__buf_state {
	FAB_BUF_INITIALIZED,
	FAB_BUF_REGISTERED,
	FAB_BUF_QUEUED,
	FAB_BUF_DEREGISTERED
};

/**
 * Represents the libfabric event type
 */
enum m0_fab__event_type {
	FAB_COMMON_Q_EVENT,
	FAB_PRIVATE_Q_EVENT
};

/**
 * Libfab structure for event context to be returned in the epoll_wait events
 */
struct m0_fab__ev_ctx {
	enum m0_fab__event_type  evctx_type;
	void                    *evctx_ep;
};

/**
 * Libfab structure for list of fabric interfaces in a transfer machine
 */
struct m0_fab__list {
	struct m0_tl fl_head;
};

/**
 * Libfab structure of fabric params
 */
struct m0_fab__fab {
	/** Magic number for list of fabric interfaces in transfer machine*/
	uint64_t           fab_magic;

	/** Fabric interface info */
	struct fi_info    *fab_fi;
	
	/** Fabric fid */
	struct fid_fabric *fab_fab;

	/** Domain fid */
	struct fid_domain *fab_dom;

	/** Link in the list of fabrics */
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
	/** Event queue for passive endpoint */
	struct fid_eq         *fpr_eq;
	
	/** Context to be returned in the epoll_wait event */
	struct m0_fab__ev_ctx  fpr_ctx;
};

/**
 * Libfab structure of resources associated to an active transmit endpoint
 */
struct m0_fab__tx_res{
	/* Event queue for transmit endpoint */
	struct fid_eq         *ftr_eq;
	
	/** Context to be returned in the epoll_wait event */
	struct m0_fab__ev_ctx  ftr_ctx;
};

/**
 * Libfab structure of resources associated to a active receive endpoint
 */
struct m0_fab__rx_res{
	/** Event queue for receive endpoint */
	struct fid_eq         *frr_eq;
	
	/** Completion Queue for receive operations */
	struct fid_cq         *frr_cq;

	/** Context to be returned in the epoll_wait event */
	struct m0_fab__ev_ctx  frr_ctx;
};

/**
 * Libfab structure of active endpoint
 */
struct m0_fab__active_ep {
	/** Transmit endpoint */
	struct fid_ep            *aep_txep;

	/** Receive endpoint */
	struct fid_ep            *aep_rxep;

	/** Transmit endpoint resources */
	struct m0_fab__tx_res     aep_tx_res;

	/** Receive endpoint resources */
	struct m0_fab__rx_res     aep_rx_res;
	
	/** connection status of Transmit ep */
	enum m0_fab__conn_status  aep_tx_state;
	
	/** connection status of Receive ep */
	enum m0_fab__conn_status  aep_rx_state;
	
	/** count of active bulk ops for the transmit endpoint */
	uint32_t                  aep_bulk_cnt;
};

/**
 * Libfab structure of passive endpoint
 */
struct m0_fab__passive_ep {
	/** Passive endpoint */
	struct fid_pep           *pep_pep;
	
	/** Active endpoint used for loopback ping and bulk operations */
	struct m0_fab__active_ep *pep_aep;
	
	/** Endpoint resources */
	struct m0_fab__pep_res    pep_res;
};

/**
 * Libfab structure of endpoint
 */
struct m0_fab__ep {
	/** Network endpoint structure linked into a per-tm list */
	struct m0_net_end_point    fep_nep;
	
	/** ipaddr, port and strname */
	struct m0_fab__ep_name     fep_name;
	
	/** Active endpoint */
	struct m0_fab__active_ep  *fep_aep;

	/** Passive endpoint */
	struct m0_fab__passive_ep *fep_listen;
	
	/** List of buffers to send after connection establishment*/
	struct m0_tl               fep_sndbuf;
};

/**
 * Libfab structure of transfer machine
 */
struct m0_fab__tm {
	/** Net transfer machine */
	struct m0_net_transfer_mc      *ftm_ntm;
	
	/** Poller thread */
	struct m0_thread                ftm_poller;
	
	/** Epoll file descriptor */
	int                             ftm_epfd;
	
	/** Fabric parameters of a transfer machine */
	struct m0_fab__fab             *ftm_fab;

	/** Passive endpoint (listening/server mode) */
	struct m0_fab__ep              *ftm_pep;
	
	/** Shared receive context for shared buffer pools */
	struct fid_ep                  *ftm_rctx;
	
	/** Transmit Completion Queue */
	struct fid_cq                  *ftm_tx_cq;

	/** Completion queue context to be returned in the epoll_wait event */
	struct m0_fab__ev_ctx           ftm_txcq_ctx;
	
	/** State of the transfer machine */
	volatile enum m0_fab__tm_state  ftm_state;

	/** List of completed buffers */
	struct m0_tl                    ftm_done;
	
	/** Lock used betn poller & tm_fini during shutdown */
	struct m0_mutex                 ftm_endlock;
	
	/** Lock used betn poller & tm_fini for posting event */
	struct m0_mutex                 ftm_evpost;

	/** List of pending bulk operations */
	struct m0_tl                    ftm_bulk;
};

/**
 * Libfab structure of buffer memory region parameters
 */
struct m0_fab__buf_mr {
	/** Local memory region (buffer) descriptor */
	void          *bm_desc[FAB_IOV_MAX];
	
	/** Memory region registration */
	struct fid_mr *bm_mr[FAB_IOV_MAX];
};

/**
 * Libfab structure of buffer descriptor 
 * sent from the passive side to the active side
 */
struct m0_fab__bdesc {
	/** Remote buffer iov count */
	uint64_t fbd_iov_cnt;

	/** Remote node address */
	uint64_t fbd_netaddr;
	
	/** Remote buffer address */
	uint64_t fbd_bufptr;
};

/**
 * Libfab structure of buffer params
 */
struct m0_fab__buf {
	/** Magic number for list of completed buffers */
	uint64_t                         fb_magic;
	
	/** Magic number for list of send buffers */
	uint64_t                         fb_sndmagic;
	
	/** Dummy data + network buffer ptr */
	uint64_t                         fb_dummy[2];
	
	/** Buffer descriptor of the remote node */
	struct m0_fab__bdesc            *fb_rbd;
	
	/** Array of iov for remote node  */
	struct fi_rma_iov               *fb_riov;
	
	/** Buffer memory region registration params */
	struct m0_fab__buf_mr            fb_mr;
	
	/** Domain to which the buffer is registered */
	struct fid_domain               *fb_dp;
	
	/** Pointer network buffer structure */
	struct m0_net_buffer            *fb_nb;
	
	/** endpoint associated with receive operation */
	struct m0_fab__ep               *fb_ev_ep;
	
	/** Transmit Context to be returned in the buffer completion event */
	struct m0_fab__ep               *fb_txctx;
	
	/** Link in list of completed buffers */
	struct m0_tlink                  fb_linkage;
	
	/** Link in list of send buffers */
	struct m0_tlink                  fb_snd_link;

	/** Buffer completion status */
	int32_t                          fb_status;
	
	/** Total size of data to be received/sent/read/written */
	m0_bindex_t                      fb_length;
	
	/** Count of work request generated for bulk rma operation */
	volatile uint32_t                fb_wr_cnt;

	/** Count of work request completions for bulk rma operation */
	volatile uint32_t                fb_wr_comp_cnt;

	/** Pointer to the m0_fab__bulk_op structure */
	void*                            fb_bulk_op;

	/** State of the buffer */
	volatile enum m0_fab__buf_state  fb_state;
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

/**
 * Libfab structure of bulk operation which is posted to the bulk list and
 * processed in the poller thread.
 */
struct m0_fab__bulk_op {
	/** Magic number for list of bulk buffers */
	uint64_t                   fbl_magic;
	
	/** Bulk buffer pointer */
	struct m0_fab__buf        *fbl_buf;
	
	/** endpoint on which to post the bulk buffer */
	struct m0_fab__active_ep  *fbl_aep;
	
	/** Link for list of bulk buffers */
	struct m0_tlink            fbl_link;
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

