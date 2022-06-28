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

#ifdef ENABLE_LIBFAB

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

/**
 * Parameters required for libfabric configuration
 */
enum m0_fab__libfab_params {
	/** Fabric memory registration access. */
	FAB_MR_ACCESS                  = (FI_READ | FI_WRITE | FI_RECV |
					  FI_SEND | FI_REMOTE_READ |
					  FI_REMOTE_WRITE),
	/** Fabric memory registration offset. */
	FAB_MR_OFFSET                  = 0,
	/** Fabric memory registration flag. */
	FAB_MR_FLAG                    = 0,
	/** Key used for memory registration. */
	FAB_MR_KEY                     = 0xABCD,
	/** Max number of IOV in read/write command for Verbs */
	FAB_VERBS_IOV_MAX              = 16,
	/** Max segment size for bulk buffers for Verbs */
	FAB_VERBS_MAX_BULK_SEG_SIZE    = 131072,
	/** Max number of active work requests for Verbs */
	FAB_VERBS_MAX_QUEUE_SIZE       = 224,

	/** Max number of IOV in read/write command for TCP/Socket provider
	 * (max number of segments) */
	FAB_TCP_SOCK_IOV_MAX           = 256,
	/** Max segment size for bulk buffers for TCP/Socket provider
	 * (4k but can be increased) */
	FAB_TCP_SOCK_MAX_BULK_SEG_SIZE = 4096,
	/** Max number of active work requests for TCP/Socket provider */
	FAB_TCP_SOCK_MAX_QUEUE_SIZE    = 1024,

	/** Max segment size for rpc buffer ( 1MB but can be changed ) */
	FAB_MAX_RPC_SEG_SIZE           = (1 << 20),
	/** Max number of segments for rpc buffer */
	FAB_MAX_RPC_SEG_NR             = 1,
	/** Max number of recevive messages in rpc buffer */
	FAB_MAX_RPC_RECV_MSG_NR        = 1,
	/** Dummy data used to notify remote end for read-rma op completions */
	FAB_DUMMY_DATA                 = 0xFABC0DE,
	/** Max number of completion events to read from a completion queue */
	FAB_MAX_COMP_READ              = 256,
	/** Max timeout for waiting on fd in epoll_wait */
	FAB_WAIT_FD_TMOUT              = 1000,
	/** Max event entries for active endpoint event queue */
	FAB_MAX_AEP_EQ_EV              = 8,
	/** Max event entries for passive endpoint event queue */
	FAB_MAX_PEP_EQ_EV              = 256,
	/** Max entries in shared transmit completion queue */
	FAB_MAX_TX_CQ_EV               = 1024,
	/** Max entries in receive completion queue */
	FAB_MAX_RX_CQ_EV               = 256,
	/** Max receive buffers in a shared receive pool */
	FAB_MAX_SRX_SIZE               = 4096,
	/** Max number of buckets per Qtype */
	FAB_NUM_BUCKETS_PER_QTYPE      = 128,
	/** Min time interval between buffer timeout check (sec) */
	FAB_BUF_TMOUT_CHK_INTERVAL     = 1,
	/** Timeout interval for getting a reply to the CONNREQ (sec) */
	FAB_CONNECTING_TMOUT           = 5,
	/** The step for increasing array size of fids in a tm */
	FAB_TM_FID_MALLOC_STEP         = 1024
};

/**
 * Represents the fabric provider for the transfer machine.
 * The names in the 'const char *provider[]' should match the indices here.
 */
enum m0_fab__prov_type {
	FAB_FABRIC_PROV_VERBS,
	FAB_FABRIC_PROV_TCP,
	FAB_FABRIC_PROV_SOCK,
	/* Add all supported fabric providers above this line */
	FAB_FABRIC_PROV_MAX
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
	FAB_BUF_CANCELED,
	FAB_BUF_TIMEDOUT,
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
 * Represents the libfabric connection link status for a libfabric endpoint
 */
enum m0_fab__connlink_status {
	FAB_CONNLINK_DOWN              = 0x00,
	FAB_CONNLINK_TXEP_RDY          = 0x01,
	FAB_CONNLINK_RXEP_RDY          = 0x02,
	FAB_CONNLINK_RDY_TO_SEND       = 0x03,
	FAB_CONNLINK_PENDING_SEND_DONE = 0x07
};

/**
 * Represents the interface type in the endpoint name
 */
enum m0_fab__ep_iface {
	FAB_LO,
	FAB_TCP,
	FAB_O2IB
};

union m0_fab__token
{
	uint32_t t_val;
	struct {
		/* m0_log2(roundup_power2(M0_NET_QT_NR+1)) */
		uint32_t tf_queue_id  : 4;
		/* m0_log2(LIBFAB_AL_NUM_HASH_Q_PER_QUEUE) */
		uint32_t tf_queue_num : 8;
		/* 32 - m0_log2(LIBFAB_AL_NUM_HASH_Q_PER_QUEUE) - m0_log2(roundup_power2(M0_NET_QT_NR+1)) */
		uint32_t tf_tag       : 20;
	} t_Fields;
};

/**
 * Libfab structure for event context to be returned in the epoll_wait events
 */
struct m0_fab__ev_ctx {
	/* Type of event (common queue/ private queue). */
	enum m0_fab__event_type  evctx_type;

	/* Endpoint context associated for event. */
	void                    *evctx_ep;

	/* Debug data. */
	char                    *evctx_dbg;

	/* Position in array of fids. */
	uint32_t                 evctx_pos;
};

/**
 * Libfab structure of buffer hash table.
 */
struct m0_fab__bufht {
	/** Magic number for buffer hash table */
	uint64_t         bht_magic;

	/** Buffer hash table */
	struct m0_htable bht_hash;
};

/**
 * Libfab structure equivalent for network domain
 */
struct m0_fab__ndom {

	/** Pointer to the m0net  domain structure */
	struct m0_net_domain *fnd_ndom;

	/** Lock used betn poller & tm_fini during shutdown */
	struct m0_mutex       fnd_lock;

	/** List of fabric interfaces in a domain */
	struct m0_tl          fnd_fabrics;

	/** local ip address */
	char                  fnd_loc_ip[INET_ADDRSTRLEN];

	/** Number of segments */
	uint32_t              fnd_seg_nr;

	/** Segments size */
	uint32_t              fnd_seg_size;
};

/**
 * Libfab structure of fabric params
 */
struct m0_fab__fab {
	/** Magic number for list of fabric interfaces in transfer machine*/
	uint64_t               fab_magic;

	/** Fabric interface info */
	struct fi_info        *fab_fi;
	
	/** Fabric fid */
	struct fid_fabric     *fab_fab;

	/** Domain fid */
	struct fid_domain     *fab_dom;

	/** Link in the list of fabrics */
	struct m0_tlink        fab_link;

	/** Fabric provider type */
	enum m0_fab__prov_type fab_prov;

	/** Max iov limit for the fabric interface */
	uint32_t               fab_max_iov;
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

	/** Context to be returned in the epoll_wait event for CQ */
	struct m0_fab__ev_ctx  frr_cq_ctx;

	/** Context to be returned in the epoll_wait event for EQ */
	struct m0_fab__ev_ctx  frr_eq_ctx;
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
	volatile uint32_t         aep_bulk_cnt;

	/** flag to indicate that the tx queue of the endpoint is full */
	bool                      aep_txq_full;

	/**
	 * Timeout after which the endpoint connecting state would be reset if
	 * no reply is received for the connection request.
	 */
	m0_time_t                 aep_connecting_tmout;
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
	
	/** ipaddr, port and strname in printable format */
	struct m0_net_ip_addr      fep_name;

	/** Name in numeric format <IP_Addr, 32bit>:<Port, 32 bit> */
	uint64_t                   fep_name_n;
	
	/** Active endpoint, this is used for TX and RX operation */
	struct m0_fab__active_ep  *fep_aep;

	/** Passive endpoint, this is used for listening and loopback */
	struct m0_fab__passive_ep *fep_listen;
	
	/** List of buffers to send after connection establishment */
	struct m0_tl               fep_sndbuf;

	/** Flag to denote that the connection link status */
	uint8_t                    fep_connlink;
};

/**
 * Libfab structure for management of all fids of a transfer machine which are
 * to be monitored in epoll_wait().
 */
struct m0_fab__tm_fids {
	/**
	 * Lock used to protect the transfer machine fids structure.
	 * The fids and contexts of event queue, completion queue are
	 * added/removed from ftf_head and ftf_ctx arrays during
	 * endpoint creation/deletion which can happen in main or poller thread.
	 * And the same arrays are used by fi_trywait().
	 */
	struct m0_mutex         ftf_lock;

	/* Pointer to the head of the list (array in this case) */
	struct fid            **ftf_head;

	/* Pointer to the ctx which is stored in this array */
	struct m0_fab__ev_ctx **ftf_ctx;

	/* Size of array used for storing fids */
	uint32_t                ftf_arr_size;

	/* Count of fids in the array */
	volatile uint32_t       ftf_cnt;
};

/**
 * Libfab structure of transfer machine
 */
struct m0_fab__tm {
	/** Net transfer machine */
	struct m0_net_transfer_mc      *ftm_ntm;
	
	/**
	 * Poller thread.
	 *
	 * All asynchronous activity happens in this thread:
	 *
	 *     - Notifications about incoming connection requests;
	 *
	 *     - Buffer completion events (libfab_buf_done());
	 *
	 *     - Buffer timeouts (libfab_tm_buf_timeout());
	 *
	 */
	struct m0_thread                ftm_poller;
	
	/** Epoll file descriptor */
	int                             ftm_epfd;

	/** Structure for fid management for fi_trywait() */
	struct m0_fab__tm_fids          ftm_fids;

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

	/** Array of remote side iovecs used in bulk ops */
	struct fi_rma_iov              *ftm_rem_iov;

	/** Array of local side iovecs used in bulk ops */
	struct iovec                   *ftm_loc_iov;

	/** Time when the buffer timeouts should be checked again */
	m0_time_t                       ftm_tmout_check;

	/** Hash table of buffers associated to the tm */
	struct m0_fab__bufht            ftm_bufhash;

	/** Memory registration key index */
	uint64_t                        ftm_mr_key_idx;

	/** Index for queue type for the buffer token */
	uint32_t                        ftm_rr_qt[M0_NET_QT_NR+1];

	/** Buffer operation id for the transfer machine */
	uint32_t                        ftm_op_id;
};

/**
 * Libfab structure of buffer memory region parameters
 */
struct m0_fab__buf_mr {
	/** Local memory region (buffer) descriptor */
	void          **bm_desc;
	
	/** Memory region registration */
	struct fid_mr **bm_mr;
};

/**
 * Libfab structure of buffer descriptor 
 * sent from the passive side to the active side
 */
struct m0_fab__bdesc {
	/** Remote node address */
	struct m0_net_ip_params fbd_netaddr;

	/** Remote buffer iov count */
	uint32_t                fbd_iov_cnt;

	/** Remote buffer token */
	uint32_t                fbd_buftoken;
};

/**
 * Libfab structure buffer params
 */
struct m0_fab__buf_xfer_params {
	/** Local index to track how many buffer segment sent */
	uint32_t bxp_loc_sidx;

	/** Remote Index to track how many buffer segment sent */
	uint32_t bxp_rem_sidx;

	/** Current transfer length sent */
	uint32_t bxp_xfer_len;

	/** Current remote segment offset */
	uint32_t bxp_rem_soff;

	/** Current remote segment offset */
	uint32_t bxp_loc_soff;
};

/**
 * Libfab structure of buffer params
 */
struct m0_fab__buf {
	/** Magic number for list of completed buffers */
	uint64_t                         fb_magic;
	
	/** Magic number for list of send buffers */
	uint64_t                         fb_sndmagic;
	
	/** Magic number for buffer hash table */
	uint64_t                         fb_htmagic;
	
	/** Dummy data + network buffer ptr */
	uint32_t                         fb_dummy[2];
	
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

	/** Link in buffer hash table */
	struct m0_hlink                  fb_htlink;

	/** Buffer completion status */
	int32_t                          fb_status;
	
	/** Total size of data to be received/sent/read/written */
	m0_bindex_t                      fb_length;
	
	/** Count of work request generated for bulk rma operation */
	volatile uint32_t                fb_wr_cnt;

	/** Bulk buffer current transfer parmas */
	struct m0_fab__buf_xfer_params   fb_xfer_params;


	/** Pointer to the m0_fab__bulk_op structure */
	void*                            fb_bulk_op;

	/** State of the buffer */
	volatile enum m0_fab__buf_state  fb_state;
	
	/** Token used for passive recv buffer */
	uint32_t                         fb_token;
};

/**
 * Libfab structure of connection data
 */
struct m0_fab__conn_data {
	/** network address */
	struct m0_net_ip_params fcd_addr;
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

#endif /* ENABLE_LIBFAB */

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

