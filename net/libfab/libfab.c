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

/**
 * @addtogroup netlibfab
 *
 * Overview
 * --------
 *
 * net/libfab/libfab.[ch] contains an implementation of the interfaces defined
 * in net/net.h. Together these interfaces define a network transport layer used
 * by motr. A network transport provides unreliable, unordered, asynchronous
 * point-to-point messaging interface.
 *
 * Libfabric network transport is implemented using libfabric library.
 * It is user-space only.
 *
 * Libfabric has no inherent fragmentation restrictions on segment size and
 * buffer length. Using a large number of small segments is pure overhead
 * because it will increase send/recv operations. Hence, to get best
 * performance, optimal values of segment size and number of segments should
 * be used.
 *
 * Data structures
 * ---------------
 *
 * libfab_internal.h introduces data-structures corresponding to the
 * abstractions in net.h and some of the additional data structures required for
 * libfabric transport.
 *
 * The libfabric end-point structure (struct m0_fab__ep) keeps
 * a) A passive endpoint which is mainly used for listening connection requests,
 * loopback ping and bulk operations and endpoint resources.
 * B) A active endpoint which is used for
 * transmit and receive operation with remote endpoint, it also stores current
 * connection state for Tx and Rx endpoints and endpoint resources.
 *
 * The libfabric transfer machine data structure (struct m0_fab__tm) contains
 * hashtable for buffers associated with tm, this is required to keep track of
 * valid operations within tm. In some cases, buffers are cancelled by RPC
 * layer and libfabric library does not provide mechanism to cancel ongoing
 * operation on respective buffer in such cases buffer token is removed from
 * hashtable and if same buffer is re-used for other operation then
 * new unique hash key is generated and added into hashtable.
 *
 *
 * Whenever any net buffer is posted successfully then unique token is generated
 * and token is added into hashtable.
 * Before generating a completion event of any buffer (i.e. libfab_buf_done()),
 * token lookup is done within hashtable to check if buffer operation is valid,
 * If token is present then token for that buffer is removed from hashtable
 * while buffer completion callback, else buffer completion event is skipped.
 *
 * Libfabric module is having two types of activities:
 *
 * 1) Synchronous Activity: These are initiated by net/net.h entry-points
 *                          being called by a user and
 * 2) Asynchronous Activity: Initiated by network related events, such as
 *                           incoming connections or buffer completions.
 *
 * Synchronous activities:
 * ----------------------
 *
 * A buffer is added to a transfer machine
 * (m0_net_buf_add()->...->libfab_buf_add()).
 *
 * If the buffer is added to M0_NET_QT_MSG_RECV queue it is just placed on the
 * libfabric internal receive queue, waiting for the incoming data.
 *
 * If the buffer is added to M0_NET_QT_MSG_SEND, M0_NET_QT_ACTIVE_BULK_RECV,
 * M0_NET_QT_ACTIVE_BULK_SEND then if the connection is established between two
 * endpoints then buffer is posted to libfabric library, else connection request
 * is sent to remote endpoint and buffer is added into send list, once
 * connection is establihsed in poller thread then all then buffers from send
 * list gets posted to libfabric library.
 * If buffer is added to M0_NET_QT_PASSIVE_BULK_RECV,
 * M0_NET_QT_PASSIVE_BULK_SEND queue buffer descriptors are encoded and in case
 * of M0_NET_QT_PASSIVE_BULK_SEND dummy buffer is posted for remote end
 * completion if extra receieve buffer is not available.
 *
 * Asynchronous activities:
 * -----------------------
 *
 * When a transfer machine is started, an end-point, representing the
 * local peer, is created, i.e. a passive libfabric endpoint is created.
 * This is a "listening endpoint" and poller thread checks for
 * new incoming connection request events on this endpoint.
 *
 * The starting point of asynchronous activity associated with a libfabric
 * transfer machine is libfab_poller(). Currently, this function is ran as a
 * separate thread.
 *
 * The poller thread checks for connection request events and transmitting
 * buffer completions, even poller thread checks for receiving buffer completion
 * events using epoll_wait() a list of libfabric evert queues configured.
 *
 * In case of RMA operations, single RMA operation is split into multiple
 * requests based on segment size on local and remote endpoint, and
 * buffer completion event shall be generated once last request is processed.
 * In case of M0_NET_QT_ACTIVE_BULK_RECV, one extra dummy message is sent to
 * remote endpoint (target of RDMA) to notify operation completion.
 *
 * Concurrency
 * -----------
 *
 * Libfabric module uses a very simple locking model: all state transitions are
 * protected by a per-tm mutex: m0_net_transfer_mc::ntm_mutex. For synchronous
 * activity, this mutex is taken by the entry-point code in net/ and is not
 * released until the entry-point completes. For asynchronous activity,
 * libfab_poller() keeps the lock taken most of the time.
 *
 * Few points about concurrency:
 *     - the tm lock is not held while epoll_wait() is executed by
 *       libfab_poller(). It is acquired after exiting the epoll_wait either
 * 	 due to some event or due to timeout.
 *
 *     - buffer completion (libfab_buf_done()) includes notifying the 
 * 	 application by invoking a user-supplied call-back.
 * 	 Completion event can happen both asynchronously (when a transfer
 * 	 operation completes for the buffer or the buffer times out,
 * 	 and synchronously (when the buffer is canceled by the user.
 * 	 Libfabric code will not support generation of synchronous callback
 * 	 events for buffer cancel operations. It will instead mark the buffer
 * 	 status as cancelled and add it to a list which will be processed
 * 	 in the poller thread and the callback would be invoked in
 * 	 asynchronous manner only.
 *
 * Libfabric interface:
 * --------------------
 *
 * Libfabric module uses fi_endpoint() to create transport level communication
 * portals. Libfabric supports two types of endpoints: Passive endpoint and
 * Active endpoint. Passive endpoints are most often used to listen for
 * incoming connection requests. However, a passive endpoint may be used to
 * reserve a fabric address that can be granted to an active endpoint.
 * Active endpoints belong to access domains and can perform data transfers.
 *
 * Below Libfabric interfaces are used:
 * 1) fi_fabric : Used for fabric domain operations.
 *    Reference: https://ofiwg.github.io/libfabric/v1.6.2/man/fi_fabric.3.html
 *
 * 2) fi_domain: Used for accessing fabric domain.
 *    Reference: https://ofiwg.github.io/libfabric/v1.1.1/man/fi_domain.3.html
 *
 * 3) fi_endpoint: USed for fabric endpoint operations.
 *    Reference: https://ofiwg.github.io/libfabric/master/man/fi_endpoint.3.html
 *
 * 4) fi_mr: Used for memory region operations.
 *    Reference: https://ofiwg.github.io/libfabric/v1.1.1/man/fi_mr.3.html
 *
 * 5) fi_rma: Used for remote memory access operations.
 *    Reference: https://ofiwg.github.io/libfabric/v1.1.1/man/fi_rma.3.html
 *
 * @{
 */


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_NET
#include "lib/trace.h"          /* M0_ENTRY() */

#ifdef ENABLE_LIBFAB

#include <netinet/in.h>         /* INET_ADDRSTRLEN */
#include <arpa/inet.h>          /* inet_pton, htons */
#include <netdb.h>              /* hostent */
#include <sched.h>              /* sched_yield */
#include <stdlib.h>             /* atoi */
#include <sys/epoll.h>          /* struct epoll_event */
#include <unistd.h>             /* close */
#include "net/buffer_pool.h"    /* struct m0_net_buffer_pool */
#include "net/net.h"            /* struct m0_net_domain */
#include "lib/errno.h"          /* errno */
#include "lib/finject.h"        /* M0_FI_ENABLED */
#include "lib/hash.h"           /* m0_htable */
#include "lib/memory.h"         /* M0_ALLOC_PTR()*/
#include "libfab_internal.h"
#include "lib/string.h"         /* m0_streq */
#include "net/net_internal.h"   /* m0_net__buffer_invariant() */

#define U32_TO_VPTR(a)     ((void*)((uintptr_t)a))
#define VPTR_TO_U32(a)     ((uint32_t)((uintptr_t)a))

/* Assert the equivalence of return code for libfabric and motr */
M0_BASSERT(FI_SUCCESS == 0);

static const char *providers[FAB_FABRIC_PROV_MAX] = { "verbs",
						      "tcp",
						      "sockets" };
static const char *protf[]     = { "inet", "inet6" };
static const char *socktype[]  = { "tcp", "o2ib", "stream", "dgram" };

/** 
 * Bitmap of used transfer machine identifiers. 1 is for used,
 * and 0 is for free.
 */
static uint8_t fab_autotm[1024] = {};

M0_TL_DESCR_DEFINE(fab_buf, "libfab_buf",
		   static, struct m0_fab__buf, fb_linkage, fb_magic,
		   M0_NET_LIBFAB_BUF_MAGIC, M0_NET_LIBFAB_BUF_HEAD_MAGIC);
M0_TL_DEFINE(fab_buf, static, struct m0_fab__buf);

M0_TL_DESCR_DEFINE(fab_sndbuf, "libfab_sndbuf",
		   static, struct m0_fab__buf, fb_snd_link, fb_sndmagic,
		   M0_NET_LIBFAB_SNDBUF_MAGIC, M0_NET_LIBFAB_SNDBUF_HEAD_MAGIC);
M0_TL_DEFINE(fab_sndbuf, static, struct m0_fab__buf);

M0_TL_DESCR_DEFINE(fab_fabs, "libfab_fabrics",
		   static, struct m0_fab__fab, fab_link, fab_magic,
		   M0_NET_LIBFAB_FAB_MAGIC, M0_NET_LIBFAB_FAB_HEAD_MAGIC);
M0_TL_DEFINE(fab_fabs, static, struct m0_fab__fab);

M0_TL_DESCR_DEFINE(fab_bulk, "libfab_bulkops",
		   static, struct m0_fab__bulk_op, fbl_link, fbl_magic,
		   M0_NET_LIBFAB_BULK_MAGIC, M0_NET_LIBFAB_BULK_HEAD_MAGIC);
M0_TL_DEFINE(fab_bulk, static, struct m0_fab__bulk_op);

static uint32_t libfab_bht_func(const struct m0_htable *ht, const void *key)
{
	const union m0_fab__token *token = key;

	/*
	 * Max buckets = ((M0_NET_QT_NR + 1) * FAB_NUM_BUCKETS_PER_QTYPE)
	 * The bucket id is defined by the queue_id and the queue_num
	 * fields of the token 
	 */
	return ((token->t_Fields.tf_queue_id * FAB_NUM_BUCKETS_PER_QTYPE) +
		token->t_Fields.tf_queue_num);
}

static bool libfab_bht_key_eq(const void *key1, const void *key2)
{
	const union m0_fab__token *token1 = key1;
	const union m0_fab__token *token2 = key2;

	return token1->t_val == token2->t_val;
}

M0_HT_DESCR_DEFINE(fab_bufhash, "Hash of bufs", static, struct m0_fab__buf,
		   fb_htlink, fb_htmagic, M0_NET_LIBFAB_BUF_HT_MAGIC,
		   M0_NET_LIBFAB_BUF_HT_HEAD_MAGIC,
		   fb_token, libfab_bht_func, libfab_bht_key_eq);

M0_HT_DEFINE(fab_bufhash, static, struct m0_fab__buf, uint32_t);

static int libfab_ep_txres_init(struct m0_fab__active_ep *aep,
				struct m0_fab__tm *tm, void *ctx);
static int libfab_ep_rxres_init(struct m0_fab__active_ep *aep,
				struct m0_fab__tm *tm, void *ctx);
static int libfab_pep_res_init(struct m0_fab__passive_ep *pep,
			       struct m0_fab__tm *tm, void *ctx);
static struct m0_fab__ep *libfab_ep(struct m0_net_end_point *net);
static bool libfab_ep_cmp(struct m0_fab__ep *ep, const char *name,
			  uint64_t *ep_name_n);
static int libfab_ep_find(struct m0_net_transfer_mc *tm, const char *name,
			  struct m0_fab__ep_name *epn,
			  struct m0_net_end_point **epp);
static int libfab_ep_create(struct m0_net_transfer_mc *tm, const char *name, 
			    struct m0_fab__ep_name *epn,
			    struct m0_net_end_point **epp);
static int libfab_active_ep_create(struct m0_fab__ep *ep,
				   struct m0_fab__tm *tm);
static int libfab_passive_ep_create(struct m0_fab__ep *ep,
				    struct m0_fab__tm *tm);
static int libfab_aep_param_free(struct m0_fab__active_ep *aep,
				 struct m0_fab__tm *tm);
static int libfab_pep_param_free(struct m0_fab__passive_ep *pep,
				 struct m0_fab__tm *tm);
static int libfab_ep_param_free(struct m0_fab__ep *ep, struct m0_fab__tm *tm);
static int libfab_pep_res_free(struct m0_fab__pep_res *pep_res, 
			       struct m0_fab__tm *tm);
static int libfab_ep_txres_free(struct m0_fab__tx_res *tx_res, 
				struct m0_fab__tm *tm);
static int libfab_ep_rxres_free(struct m0_fab__rx_res *rx_res, 
				struct m0_fab__tm *tm);
static void libfab_poller(struct m0_fab__tm *ma);
static int libfab_waitfd_init(struct m0_fab__tm *tm);
static void libfab_tm_event_post(struct m0_fab__tm *tm, 
				 enum m0_net_tm_state state);
static inline void libfab_tm_lock(struct m0_fab__tm *tm);
static inline void libfab_tm_unlock(struct m0_fab__tm *tm);
static inline void libfab_tm_evpost_lock(struct m0_fab__tm *tm);
static inline void libfab_tm_evpost_unlock(struct m0_fab__tm *tm);
static inline bool libfab_tm_is_locked(const struct m0_fab__tm *tm);
static void libfab_buf_complete(struct m0_fab__buf *buf);
static void libfab_buf_done(struct m0_fab__buf *buf, int rc, bool add_to_list);
static inline struct m0_fab__tm *libfab_buf_tm(struct m0_fab__buf *buf);
static inline struct m0_fab__tm *libfab_buf_ma(struct m0_net_buffer *buf);
static bool libfab_tm_invariant(const struct m0_fab__tm *tm);
static void libfab_buf_del(struct m0_net_buffer *nb);
static inline void libfab_ep_get(struct m0_fab__ep *ep);
static void libfab_ep_release(struct m0_ref *ref);
static uint64_t libfab_mr_keygen(struct m0_fab__tm *tm);
static int libfab_check_for_event(struct fid_eq *eq, uint32_t *ev);
static int libfab_check_for_comp(struct fid_cq *cq, uint32_t *ctx,
				 m0_bindex_t *len, uint64_t *rem_cq_data);
static void libfab_tm_fini(struct m0_net_transfer_mc *tm);
static int libfab_buf_dom_reg(struct m0_net_buffer *nb, struct m0_fab__tm *tm);
static int libfab_buf_dom_dereg(struct m0_fab__buf *fbp);
static void libfab_pending_bufs_send(struct m0_fab__ep *ep);
static int libfab_target_notify(struct m0_fab__buf *buf);
static int libfab_conn_init(struct m0_fab__ep *ep, struct m0_fab__tm *ma,
			    struct m0_fab__buf *fbp);
static int libfab_conn_accept(struct m0_fab__ep *ep, struct m0_fab__tm *tm,
			      struct fi_info *info);
static int libfab_fab_ep_find(struct m0_fab__tm *tm, struct m0_fab__ep_name *en,
			      const char *name, struct m0_fab__ep **ep);
static void libfab_ep_pton(struct m0_fab__ep_name *name, uint64_t *out);
static void libfab_ep_ntop(uint64_t netaddr, struct m0_fab__ep_name *name);
static void libfab_txep_event_check(struct m0_fab__ep *txep,
				    struct m0_fab__active_ep *aep,
				    struct m0_fab__tm *tm);
static int libfab_txep_init(struct m0_fab__active_ep *aep,
			    struct m0_fab__tm *tm, void *ctx);
static int libfab_waitfd_bind(struct fid* fid, struct m0_fab__tm *tm,
			      void *ctx);
static inline struct m0_fab__active_ep *libfab_aep_get(struct m0_fab__ep *ep);
static int libfab_ping_op(struct m0_fab__active_ep *ep, struct m0_fab__buf *fb);
static int libfab_bulk_op(struct m0_fab__active_ep *ep, struct m0_fab__buf *fb);
static inline bool libfab_is_verbs(struct m0_fab__tm *tm);
static int libfab_txbuf_list_add(struct m0_fab__tm *tm, struct m0_fab__buf *fb,
				 struct m0_fab__active_ep *aep);
static void libfab_bufq_process(struct m0_fab__tm *tm);
static uint32_t libfab_buf_token_get(struct m0_fab__tm *tm,
				     struct m0_fab__buf *fb);
static bool libfab_buf_invariant(const struct m0_fab__buf *buf);


/* libfab init and fini() : initialized in motr init */
M0_INTERNAL int m0_net_libfab_init(void)
{
	m0_net_xprt_register(&m0_net_libfab_xprt);
	if (m0_streq(M0_DEFAULT_NETWORK, "LF"))
		m0_net_xprt_default_set(&m0_net_libfab_xprt);
	return M0_RC(0);
}

M0_INTERNAL void m0_net_libfab_fini(void)
{
	m0_net_xprt_deregister(&m0_net_libfab_xprt);
}

static void libfab_straddr_copy(struct m0_fab__conn_data *cd, char *buf,
                               uint8_t len, struct m0_fab__ep_name *en)
{
	sprintf(buf, "%s@%s:12345:%d:%d",
                      cd->fcd_iface == FAB_LO ? "0" : en->fen_addr,
                      cd->fcd_iface == FAB_LO ? "lo" :
                              ((cd->fcd_iface == FAB_TCP) ? "tcp" : "o2ib"),
                      cd->fcd_portal, cd->fcd_tmid);

	M0_ASSERT(len >= strlen(buf));
}


static int libfab_hostname_to_ip(char *hostname , char* ip)
{
	struct hostent *hname;
	struct in_addr **addr;
	int             i;
	int             n;
	char           *cp;
	char            name[50];

	cp = strchr(hostname, '@');
	if (cp == NULL)
		return M0_ERR(-EINVAL);

	n = cp - hostname;
	memcpy(name, hostname, n);
	name[n] = '\0';
	M0_LOG(M0_DEBUG, "in %s out %s", (char*)hostname, (char*)name);
	if ((hname = gethostbyname(name)) == NULL)
		return M0_ERR(-EPROTO);

	addr = (struct in_addr **) hname->h_addr_list;
	for(i = 0; addr[i] != NULL; i++)
	{
		//Return the first one;
		strcpy(ip , inet_ntoa(*addr[i]));
		n=strlen(ip);
		return M0_RC(n);
	}

	return M0_ERR(-EPROTO);
}

/**
 * This function decodes the lnet format address and extracts the ip address and
 * port number from it.
 * This is also used to allocate unique transfer machine identifiers for LNet
 * network addresses with wildcard transfer machine identifier (like
 * "192.168.96.128@tcp1:12345:31:*").
 */
static int libfab_ep_addr_decode_lnet(const char *name, char *node,
				      size_t nodeSize, char *port,
				      size_t portSize, struct m0_fab__ndom *fnd)
{
	char     *at = NULL;
	int       nr;
	int       i;
	unsigned  pid;
	unsigned  portal;
	unsigned  portnum;
	unsigned  tmid;

	if (strncmp(name, "0@lo", 4) == 0) {
		M0_PRE(nodeSize > strlen(fnd->fnd_loc_ip));
		memcpy(node, fnd->fnd_loc_ip, strlen(fnd->fnd_loc_ip)+1);
	} else {
		at = strchr(name, '@');
		if (at == NULL || at - name >= nodeSize)
			return M0_ERR(-EPROTO);

		M0_PRE(nodeSize >= (at-name)+1);
		memcpy(node, name, at - name);
	}
	at = at == NULL ? (char *)name : at;
	if ((at = strchr(at, ':')) == NULL) /* Skip 'tcp...:' bit. */
		return M0_ERR(-EPROTO);
	nr = sscanf(at + 1, "%u:%u:%u", &pid, &portal, &tmid);
	if (nr != 3) {
		nr = sscanf(at + 1, "%u:%u:*", &pid, &portal);
		if (nr != 2)
			return M0_ERR(-EPROTO);
		for (i = 0; i < ARRAY_SIZE(fab_autotm); ++i) {
			if (fab_autotm[i] == 0) {
				tmid = i;
				break;
			}
		}
		if (i == ARRAY_SIZE(fab_autotm))
			return M0_ERR(-EADDRNOTAVAIL);
	}
	
	if (pid != 12345)
		return M0_ERR(-EPROTO);
	/*
	 * Deterministically combine portal and tmid into a unique 16-bit port
	 * number (greater than 1024). Tricky.
	 *
	 * Port number is, in binary: tttttttttt1ppppp, that is, 10 bits of tmid
	 * (which must be less than 1024), followed by a set bit (guaranteeing
	 * that the port is not reserved), followed by 5 bits of (portal - 30),
	 * so that portal must be in the range 30..61.
	 *
	 * if (tmid >= 1024 || (portal - 30) >= 32)
	 * 	return M0_ERR_INFO(-EPROTO,
	 * 		"portal: %u, tmid: %u", portal, tmid);
	 */

	if (portal < 30)
		portal = 30 + portal;

	portnum  = tmid | (1 << 10) | ((portal - 30) << 11);
	M0_ASSERT(portnum < 65536);
	sprintf(port, "%d", (int)portnum);
	fab_autotm[tmid] = 1;
	return M0_RC(0);
}

/**
 * This function decodes the socket format address and extracts the ip address
 * and port number from it. The socket address format is of the type
 *    family:type:ipaddr[@port]
 *    for example: "inet:stream:lanl.gov@23",
 *                 "inet6:dgram:FE80::0202:B3FF:FE1E:8329@6663" or
 *                 "unix:dgram:/tmp/socket".
 */
static int libfab_ep_addr_decode_sock(const char *ep_name, char *node,
				      size_t nodeSize, char *port,
				      size_t portSize, uint8_t *addr_frmt)
{
	int   shift = 0;
	int   f;
	int   s;
	char *at;
	char  ip[LIBFAB_ADDR_LEN_MAX];
	int   n;

	for (f = 0; f < ARRAY_SIZE(protf); ++f) {
		if (protf[f] != NULL) {
			shift = strlen(protf[f]);
			if (strncmp(ep_name, protf[f], shift) == 0)
				break;
		}
	}
	if (ep_name[shift] != ':')
		return M0_ERR(-EINVAL);
	ep_name += shift + 1;
	for (s = 0; s < ARRAY_SIZE(socktype); ++s) {
		if (socktype[s] != NULL) {
			shift = strlen(socktype[s]);
			if (strncmp(ep_name, socktype[s], shift) == 0)
				break;
		}
	}
	if (ep_name[shift] != ':')
		return M0_ERR(-EINVAL);
	ep_name += shift + 1;
	at = strchr(ep_name, '@');
	if (at == NULL) {
		return M0_ERR(-EINVAL);
	} else {
		at++;
		if (at == NULL)
			return M0_ERR(-EINVAL);
		M0_PRE(portSize >= (strlen(at)+1));
		memcpy(port, at, (strlen(at)+1));
	}
	//M0_ASSERT(nodeSize >= (at - ep_name));
	n = libfab_hostname_to_ip((char *)ep_name, ip);
        if (n > 0) {
                memcpy(node, ip, n);
                *addr_frmt = FAB_NATIVE_HOSTNAME_FORMAT;
        }
        else {
		memcpy(node, ep_name, ((at - ep_name)-1));
                *addr_frmt = FAB_NATIVE_IP_FORMAT;
        }

	return 0;
}

/**
 * Parses network address.
 *
 * The following address formats are supported:
 *
 *     - lnet compatible, see nlx_core_ep_addr_decode():
 *
 *           nid:pid:portal:tmid
 *
 *       for example: "10.0.2.15@tcp:12345:34:123" or
 *       "192.168.96.128@tcp1:12345:31:*"
 *
 *     - sock format, see socket(2):
 *           family:type:ipaddr[@port]
 *
 *     - libfab also follows sock addr format 
 *           family:type:ipaddr@port
 *           family:type:hostname@port
 *
 */
static int libfab_ep_addr_decode(struct m0_fab__ep *ep, const char *name,
				 struct m0_fab__ndom *fnd)
{
	char *node = ep->fep_name_p.fen_addr;
	char *port = ep->fep_name_p.fen_port;
	size_t nodeSize = ARRAY_SIZE(ep->fep_name_p.fen_addr);
	size_t portSize = ARRAY_SIZE(ep->fep_name_p.fen_port);
	uint8_t addr_fmt = 0;
	int result;

	M0_ENTRY("name=%s", name);
	
	if (name == NULL || name[0] == 0)
		result =  M0_ERR(-EPROTO);
	else if ((strncmp(name, "inet", 4)) == 0 ) {
		result = libfab_ep_addr_decode_sock(name, node, nodeSize,
						      port, portSize, &addr_fmt);
		ep->fep_name_p.fen_addr_frmt = addr_fmt;
	}
	else {
		/* Lnet format. */
		ep->fep_name_p.fen_addr_frmt = FAB_LNET_FORMAT;
		result = libfab_ep_addr_decode_lnet(name, node, nodeSize,
						    port, portSize, fnd);
	}

	if (result == 0)
		strcpy(ep->fep_name_p.fen_str_addr, name);

	return M0_RC(result);
}

/**
 * Used to lock the transfer machine mutex
 */
static inline void libfab_tm_lock(struct m0_fab__tm *tm)
{
	m0_mutex_lock(&tm->ftm_ntm->ntm_mutex);
}

/**
 * Used to unlock the transfer machine mutex
 */
static inline void libfab_tm_unlock(struct m0_fab__tm *tm)
{
	m0_mutex_unlock(&tm->ftm_ntm->ntm_mutex);
}

static inline int libfab_tm_trylock(struct m0_fab__tm *tm)
{
	return m0_mutex_trylock(&tm->ftm_ntm->ntm_mutex);
}

/**
 * Used to lock the transfer machine event post mutex
 */
static inline void libfab_tm_evpost_lock(struct m0_fab__tm *tm)
{
	m0_mutex_lock(&tm->ftm_evpost);
}

/**
 * Used to unlock the transfer machine event post mutex
 */
static inline void libfab_tm_evpost_unlock(struct m0_fab__tm *tm)
{
	m0_mutex_unlock(&tm->ftm_evpost);
}

/**
 * Used to check if the transfer machine mutex is locked.
 * Returns true if locked, else false.
 */
static inline bool libfab_tm_is_locked(const struct m0_fab__tm *tm)
{
	return m0_mutex_is_locked(&tm->ftm_ntm->ntm_mutex);
}

/**
 * Helper function that posts a tm state change event.
 */
static void libfab_tm_event_post(struct m0_fab__tm *tm,
				 enum m0_net_tm_state state)
{
	struct m0_net_end_point *listen = NULL;

	if (state == M0_NET_TM_STARTED) {
		/* Check for LISTENING Passive endpoint */
		listen = &tm->ftm_pep->fep_nep;
		M0_ASSERT(listen != NULL);
	}
	
	m0_net_tm_event_post(&(struct m0_net_tm_event) {
		.nte_type       = M0_NET_TEV_STATE_CHANGE,
		.nte_next_state = state,
		.nte_time       = m0_time_now(),
		.nte_ep         = listen,
		.nte_tm         = tm->ftm_ntm,
	});
}

/**
 * Finds queued buffers that timed out and completes them with a
 * ETIMEDOUT error.
 */
static void libfab_tm_buf_timeout(struct m0_fab__tm *ftm)
{
	struct m0_net_transfer_mc *net = ftm->ftm_ntm;
	struct m0_net_buffer      *nb;
	struct m0_fab__buf        *fb;
	int                        i;
	m0_time_t                  now = m0_time_now();

	M0_PRE(libfab_tm_is_locked(ftm));
	M0_PRE(libfab_tm_invariant(ftm));

	ftm->ftm_tmout_check = m0_time_from_now(FAB_BUF_TMOUT_CHK_INTERVAL, 0);
	for (i = 0; i < ARRAY_SIZE(net->ntm_q); ++i) {
		m0_tl_for(m0_net_tm, &net->ntm_q[i], nb) {
			if (nb->nb_timeout < now) {
				fb = nb->nb_xprt_private;
				nb->nb_flags |= M0_NET_BUF_TIMED_OUT;
				libfab_buf_dom_dereg(fb);
				fb->fb_state = FAB_BUF_TIMEDOUT;
				libfab_buf_done(fb, -ETIMEDOUT, false);
			}
		} m0_tl_endfor;
	}
	M0_POST(libfab_tm_invariant(ftm));
}

/**
 * Finds pending buffers completions and completes them.
 *
 * A buffer is placed on tm::ftm_done queue when its operation is done, but the
 * completion call-back cannot be immediately invoked, for example, because
 * completion happened in a synchronous context.
 */
static void libfab_tm_buf_done(struct m0_fab__tm *ftm)
{
	struct m0_fab__buf *buffer;
	int                 nr = 0;

	M0_PRE(libfab_tm_is_locked(ftm) && libfab_tm_invariant(ftm));
	m0_tl_teardown(fab_buf, &ftm->ftm_done, buffer) {
		libfab_buf_complete(buffer);
		nr++;
	}

	if (nr > 0 && ftm->ftm_ntm->ntm_callback_counter == 0)
		m0_chan_broadcast(&ftm->ftm_ntm->ntm_chan);
	M0_POST(libfab_tm_invariant(ftm));
}

/**
 * Constructs an address in string format from the connection data parameters
 */
static void libfab_straddr_gen(struct m0_fab__conn_data *cd, char *buf,
			       uint8_t len, struct m0_fab__ep_name *en)
{
	libfab_ep_ntop(cd->fcd_netaddr, en);

	if (cd->fcd_addr_frmt == FAB_LNET_FORMAT) {
		if (cd->fcd_tmid == 0xFFFF)
			sprintf(buf, "%s@%s:12345:%d:*",
				cd->fcd_iface == FAB_LO ? "0" : en->fen_addr,
				cd->fcd_iface == FAB_LO ? "lo" :
					((cd->fcd_iface == FAB_TCP) ? "tcp" :
								      "o2ib"),
				cd->fcd_portal);
		else
			libfab_straddr_copy(cd, buf, len, en);
	} else if (cd->fcd_addr_frmt == FAB_NATIVE_IP_FORMAT) 
			libfab_straddr_copy(cd, buf, len, en);
	  else if (cd->fcd_addr_frmt == FAB_NATIVE_HOSTNAME_FORMAT)
		sprintf(buf, "%s", cd->fcd_hostname);

	M0_ASSERT(len >= strlen(buf));
}

/**
 * Used to handle incoming connection request events
 * 
 * This function is called from the poller thread and there is no action
 * on failure to accept an incoming request.
 * Also, at the receiving side of the CONNREQ, there is no means to notify the
 * application about the failure to accept the connection request.
 * Hence the errors can be ignored and the function always returns success.
 */
static uint32_t libfab_handle_connect_request_events(struct m0_fab__tm *tm)
{
	struct m0_fab__ep        *ep = NULL;
	struct m0_fab__conn_data *cd;
	struct m0_fab__ep_name    en = {};
	struct fid_eq            *eq;
	struct fi_eq_err_entry    eq_err = {};
	struct fi_eq_cm_entry    *cm_entry;
	char                      entry[(sizeof(struct fi_eq_cm_entry) +
					sizeof(struct m0_fab__conn_data))];
	uint32_t                  event;
	int                       rc;
	char                      straddr[LIBFAB_ADDR_STRLEN_MAX] = {};

	eq = tm->ftm_pep->fep_listen->pep_res.fpr_eq;
	rc = fi_eq_read(eq, &event, &entry, sizeof(entry), 0);
	if (rc >= (int)sizeof(struct fi_eq_cm_entry) && event == FI_CONNREQ) {
		cm_entry = (struct fi_eq_cm_entry *)entry;
		cd = (struct m0_fab__conn_data*)(cm_entry->data);
		libfab_straddr_gen(cd, straddr, sizeof(straddr), &en);
		rc = libfab_fab_ep_find(tm, &en, straddr, &ep);
		if (rc == 0) {
			rc = libfab_conn_accept(ep, tm, cm_entry->info);
			if (rc != 0)
				M0_LOG(M0_ERROR, "Conn accept failed %d", rc);
		} else
			M0_LOG(M0_ERROR, "libfab_fab_ep_find failed rc=%d", rc);
		fi_freeinfo(cm_entry->info);
	} else if (rc == -FI_EAVAIL) {
		rc = fi_eq_readerr(eq, &eq_err, 0);
		if (rc != sizeof(eq_err)) {
			M0_LOG(M0_ERROR, "fi_eq_readerr returns error =%s",
			       fi_strerror((int) -rc));
		} else {
			M0_LOG(M0_ERROR, "fi_eq_readerr provider err no %d:%s",
				eq_err.prov_errno,
				fi_eq_strerror(eq, eq_err.prov_errno,
					       eq_err.err_data, NULL, 0));
		}
	} else if (rc != -EAGAIN)
		/*
		 * For all other events, there is no error info available.
		 * Hence, all such events can be ignored.
		 */
		M0_LOG(M0_ERROR, "Unexpected event tm=%p rc=%d", tm, rc);
	return 0;
}

/**
 * Check connection established and shutdown events for transmit endpoint.
 * All other types of events are ignored.
 */
static void libfab_txep_event_check(struct m0_fab__ep *txep,
				    struct m0_fab__active_ep *aep,
				    struct m0_fab__tm *tm)
{
	struct m0_fab__buf *fbp;
	uint32_t            event;
	int                 rc;

	if (aep->aep_rx_state == FAB_CONNECTING) {
		rc = libfab_check_for_event(aep->aep_rx_res.frr_eq, &event);
		if (rc >= 0 && event == FI_CONNECTED) {
			aep->aep_rx_state = FAB_CONNECTED;
			if (txep == tm->ftm_pep)
				txep->fep_connlink |= FAB_CONNLINK_RXEP_READY;
		}
	}

	rc = libfab_check_for_event(aep->aep_tx_res.ftr_eq, &event);
	if (rc >= 0) {
		if (event == FI_CONNECTED) {
			aep->aep_tx_state = FAB_CONNECTED;
			if (txep == tm->ftm_pep)
				txep->fep_connlink |= FAB_CONNLINK_TXEP_READY;
			else
				txep->fep_connlink |= FAB_CONNLINK_TXEP_READY |
						      FAB_CONNLINK_RXEP_READY;
		} else if (event == FI_SHUTDOWN) {
			/* Reset and reopen endpoint */
			libfab_txep_init(aep, tm, txep);
		}
	} else if (rc == -ECONNREFUSED && aep->aep_tx_state == FAB_CONNECTING) {
		libfab_txep_init(aep, tm, txep);
		m0_tl_teardown(fab_sndbuf, &txep->fep_sndbuf, fbp) {
			libfab_buf_done(fbp, rc, false);
		}
	}
	/* All other types of events can be ignored */

	if (txep->fep_connlink == FAB_CONNLINK_READY_TO_SEND) {
		libfab_pending_bufs_send(txep);
		txep->fep_connlink = FAB_CONNLINK_PENDING_SEND_DONE;
	}
}

/**
 * Check for completion events on the completion queue for the receive endpoint
 */
static void libfab_rxep_comp_read(struct fid_cq *cq, struct m0_fab__ep *ep,
				  struct m0_fab__tm *tm)
{
	struct m0_fab__buf  *fb = NULL;
	uint32_t             token[FAB_MAX_COMP_READ];
	m0_bindex_t          len[FAB_MAX_COMP_READ];
	uint64_t             data[FAB_MAX_COMP_READ];
	int                  i;
	int                  cnt;
	uint32_t             rem_token;

	if (cq != NULL) {
		cnt = libfab_check_for_comp(cq, token, len, data);
		for (i = 0; i < cnt; i++) {
			fb = fab_bufhash_htable_lookup(
				&tm->ftm_bufhash.bht_hash,
				&token[i]);
			if (fb != NULL) {
				if (fb->fb_length == 0)
					fb->fb_length = len[i];
				fb->fb_ev_ep = ep;
				libfab_buf_done(fb, 0, false);
			}
			if (data[i] != 0) {
				rem_token = (uint32_t)data[i];
				fb = fab_bufhash_htable_lookup(
					&tm->ftm_bufhash.bht_hash,
					&rem_token);
				if (fb != NULL)
					libfab_buf_done(fb, 0, false);
			}
		}
	}
}

/**
 * Check for completion events on the completion queue for the transmit endpoint
 */
static void libfab_txep_comp_read(struct fid_cq *cq, struct m0_fab__tm *tm)
{
	struct m0_fab__active_ep *aep;
	struct m0_fab__buf       *fb = NULL;
	uint32_t                  token[FAB_MAX_COMP_READ];
	int                       i;
	int                       cnt;

	cnt = libfab_check_for_comp(cq, token, NULL, NULL);
	for (i = 0; i < cnt; i++) {
		if (token[i] != 0)
			fb = fab_bufhash_htable_lookup(
				&tm->ftm_bufhash.bht_hash,
				&token[i]);
		else
			fb = NULL;
		if (fb != NULL) {
			aep = libfab_aep_get(fb->fb_txctx);
			if ((fb->fb_token & M0_NET_QT_NR) == M0_NET_QT_NR) {
				fab_bufhash_htable_del(
						 &tm->ftm_bufhash.bht_hash, fb);
				M0_ASSERT(aep->aep_bulk_cnt);
				--aep->aep_bulk_cnt;
				aep->aep_txq_full = false;
				m0_free(fb);
			} else {
				if (M0_IN(fb->fb_nb->nb_qtype,
					(M0_NET_QT_MSG_SEND,
					 M0_NET_QT_ACTIVE_BULK_RECV,
					 M0_NET_QT_ACTIVE_BULK_SEND))) {
					M0_ASSERT(aep->aep_bulk_cnt >=
								 fb->fb_wr_cnt);
					aep->aep_bulk_cnt -= fb->fb_wr_cnt;
					aep->aep_txq_full = false;
				}
				libfab_target_notify(fb);
				libfab_buf_done(fb, 0, false);
			}
		}
	}
}

/**
 * Used to poll for connection events, completion events and process the queued
 * bulk buffer operations.
 */
static void libfab_poller(struct m0_fab__tm *tm)
{
	struct m0_fab__ev_ctx    *ctx;
	struct m0_fab__ep        *xep;
	struct m0_fab__active_ep *aep;
	struct fid_cq            *cq;
	struct epoll_event        ev;
	int                       ev_cnt;
	int                       ret;

	libfab_tm_event_post(tm, M0_NET_TM_STARTED);
	while (tm->ftm_state != FAB_TM_SHUTDOWN) {
		/*
		 * It is observed that with epoll_wait,
		 * the thread is waiting in a busy-loop for events
		 * thus not releasing CPU.
		 * Hence, adding a sched_yield() will release the CPU for
		 * other processes and reduce CPU consumption.
		 */
		sched_yield();
		ev_cnt = epoll_wait(tm->ftm_epfd, &ev, 1, FAB_WAIT_FD_TMOUT);

		while (1) {
			m0_mutex_lock(&tm->ftm_endlock);
			if (tm->ftm_state == FAB_TM_SHUTDOWN) {
				m0_mutex_unlock(&tm->ftm_endlock);
				return;
			}

			ret = libfab_tm_trylock(tm);
			m0_mutex_unlock(&tm->ftm_endlock);
			if (ret == 0) {
				/*
				 * Got tm lock.
				 * Let's continue processing events.
				 */
				break;
			}
		}

		M0_ASSERT(libfab_tm_is_locked(tm) && libfab_tm_invariant(tm));

		/* Check the common queue of the transfer machine for events */
		libfab_handle_connect_request_events(tm);
		libfab_txep_comp_read(tm->ftm_tx_cq, tm);

		if (ev_cnt > 0) {
			ctx = ev.data.ptr;
			if (ctx->evctx_type != FAB_COMMON_Q_EVENT) {
				/*
				 * Check the private queue of the
				 * endpoint for events 
				 */
				xep = ctx->evctx_ep;
				aep = libfab_aep_get(xep);
				libfab_txep_event_check(xep, aep, tm);
				cq = aep->aep_rx_res.frr_cq;
				libfab_rxep_comp_read(cq, xep, tm);
			}
		}

		libfab_bufq_process(tm);
		if (m0_time_is_in_past(tm->ftm_tmout_check))
			libfab_tm_buf_timeout(tm);
		libfab_tm_buf_done(tm);

		M0_ASSERT(libfab_tm_invariant(tm));
		libfab_tm_unlock(tm);
	}
}

/** 
 * Converts network end-point to its libfabric structure.
 */
static inline struct m0_fab__ep *libfab_ep(struct m0_net_end_point *net)
{
	return container_of(net, struct m0_fab__ep, fep_nep);
}

/**
 * Compares the endpoint name with the passed name string and
 * returns true if equal, or else returns false
 * If name is null then the ipaddress and port fields are compared to check for
 * a matching endpoint.
 */
static bool libfab_ep_cmp(struct m0_fab__ep *ep, const char *name,
			  uint64_t *ep_name_n)
{
	return (name != NULL &&
		strcmp(ep->fep_name_p.fen_str_addr, name) == 0) ||
	       (name == NULL && *ep_name_n == ep->fep_name_n);
}

static int libfab_addr_port_verify(struct m0_net_transfer_mc *tm, const char *name,
			    struct m0_fab__ep_name *epn,
			    struct m0_fab__ep *ep)
{
	struct m0_fab__ep_name  name_p = ep->fep_name_p;
	uint8_t                 addr_frmt = name_p.fen_addr_frmt;
	char                   *wc = NULL;
	int                     rc = FI_SUCCESS;
	struct m0_fab__tm      *ma;
	struct m0_fab__active_ep *aep;

	/* Wildchar can be present only in LNET_ADDR_FORMAT */
	wc = strchr(name, '*');
	if ((wc != NULL && strcmp(name_p.fen_port, epn->fen_port) != 0) ||
	    (addr_frmt == FAB_NATIVE_HOSTNAME_FORMAT &&
	      (strcmp(name_p.fen_addr, epn->fen_addr) != 0 ||
	       strcmp(name_p.fen_port, epn->fen_port) != 0))) {
		strcpy(ep->fep_name_p.fen_addr, epn->fen_addr);
		strcpy(ep->fep_name_p.fen_port, epn->fen_port);
		libfab_ep_pton(&ep->fep_name_p,
				&ep->fep_name_n);
		aep = libfab_aep_get(ep);
		ma = tm->ntm_xprt_private;
		if (aep->aep_tx_state == FAB_CONNECTED)
			rc = libfab_txep_init(aep, ma, ep);
	}

	return rc;
}

/**
 * Search for the ep in the existing ep list using one of the following -
 *   1) Name in str format      OR
 *   2) ipaddr and port 
 * If found then return the ep structure, or else create a new endpoint
 * with the name
 * Returns 0 if the endpoint is found/created succesfully,
 * else returns error code.
 */
static int libfab_ep_find(struct m0_net_transfer_mc *tm, const char *name,
			  struct m0_fab__ep_name *epn,
			  struct m0_net_end_point **epp)
{
	struct m0_net_end_point  *net;
	struct m0_fab__ep        *ep;
	uint64_t                  ep_name_n = 0;
	char                      ep_str[LIBFAB_ADDR_STRLEN_MAX + 9] = {'\0'};
	int                       rc = 0;

	if (epn != NULL)
		libfab_ep_pton(epn, &ep_name_n);

	M0_ASSERT(libfab_tm_is_locked(tm->ntm_xprt_private));
	net = m0_tl_find(m0_nep, net, &tm->ntm_end_points,
			 libfab_ep_cmp(libfab_ep(net), name, &ep_name_n));

	if (net == NULL) {
		if (name != NULL)
			rc = libfab_ep_create(tm, name, epn, epp);
		else {
			M0_ASSERT(epn != NULL);
			M0_ASSERT((strlen(epn->fen_addr) + strlen(epn->fen_port)
				  + 8) < LIBFAB_ADDR_STRLEN_MAX);
			sprintf(ep_str, "inet:tcp:%s@%s", epn->fen_addr,
				epn->fen_port);
			rc = libfab_ep_create(tm, ep_str, epn, epp);
		}
	} else {
		ep = libfab_ep(net);
		*epp = &ep->fep_nep;
		if (name != NULL && epn != NULL)
			rc = libfab_addr_port_verify(tm, name, epn, ep);

		if (rc == 0)
			libfab_ep_get(ep);

	}

	return M0_RC(rc);
}

/**
 * Creates a new active endpoint
 */
static int libfab_ep_create(struct m0_net_transfer_mc *tm, const char *name,
			    struct m0_fab__ep_name *epn,
			    struct m0_net_end_point **epp)
{
	struct m0_fab__ndom  *fnd = tm->ntm_dom->nd_xprt_private;
	struct m0_fab__tm    *ma = tm->ntm_xprt_private;
	struct m0_fab__ep    *ep = NULL;
	char                 *wc;
	int                   rc;

	M0_ENTRY("name=%s", name);
	M0_PRE(name != NULL);

	M0_ALLOC_PTR(ep);
	if (ep == NULL)
		return M0_ERR(-ENOMEM);

	M0_ALLOC_PTR(ep->fep_aep);
	if (ep->fep_aep == NULL) {
		m0_free(ep);
		return M0_ERR(-ENOMEM);
	}

	ep->fep_listen = NULL;

	rc = libfab_ep_addr_decode(ep, name, fnd);
	if (rc != 0) {
		libfab_aep_param_free(ep->fep_aep, ma);
		m0_free(ep);
		return M0_ERR(rc);
	}

	wc = strchr(name, '*');
	if (epn != NULL && wc != NULL) {
		strcpy(ep->fep_name_p.fen_addr, epn->fen_addr);
		strcpy(ep->fep_name_p.fen_port, epn->fen_port);
	}

	rc = libfab_active_ep_create(ep, ma);
	if (rc != 0) {
		libfab_aep_param_free(ep->fep_aep, ma);
		m0_free(ep);
		return M0_ERR(rc);
	}

	fab_sndbuf_tlist_init(&ep->fep_sndbuf);
	*epp = &ep->fep_nep;
	return M0_RC(0);
}

/**
 * Init resources for a transfer machine
 */
static int libfab_tm_res_init(struct m0_fab__tm *tm)
{
	struct m0_fab__fab        *fab;
	struct m0_fab__passive_ep *pep;
	struct fi_cq_attr          cq_attr = {};
	int                        rc = 0;
	
	M0_PRE(tm != NULL);

	pep = tm->ftm_pep->fep_listen;
	fab = tm->ftm_fab;
	/* Initialise completion queues for tx */
	cq_attr.wait_obj = FI_WAIT_FD;
	cq_attr.format = FI_CQ_FORMAT_DATA;
	cq_attr.size = FAB_MAX_TX_CQ_EV;
	rc = fi_cq_open(fab->fab_dom, &cq_attr, &tm->ftm_tx_cq, NULL);
	if (rc != 0)
		return M0_ERR(rc);

	/* Initialize and bind resources to tx ep */
	tm->ftm_txcq_ctx.evctx_type = FAB_COMMON_Q_EVENT;
	tm->ftm_txcq_ctx.evctx_ep = NULL;
	rc = libfab_waitfd_bind(&tm->ftm_tx_cq->fid, tm, &tm->ftm_txcq_ctx);
	if (rc != 0)
		return M0_ERR(rc);

	return M0_RC(libfab_txep_init(pep->pep_aep, tm, tm->ftm_pep));
}

/**
 * Initialize transmit endpoint resources and associate
 * it to the active transmit endpoint.
 */
static int libfab_ep_txres_init(struct m0_fab__active_ep *aep,
				struct m0_fab__tm *tm, void *ctx)
{
	struct fi_eq_attr   eq_attr = {};
	struct m0_fab__fab *fab;
	int                 rc;

	fab = tm->ftm_fab;

	/* Bind the ep to tx completion queue */
	rc = fi_ep_bind(aep->aep_txep, &tm->ftm_tx_cq->fid,
			FI_TRANSMIT | FI_RECV | FI_SELECTIVE_COMPLETION);
	if (rc != 0)
		return M0_ERR(rc);

	/* Initialise and bind event queue */
	eq_attr.wait_obj = FI_WAIT_FD;
	eq_attr.size = FAB_MAX_AEP_EQ_EV;
	rc = fi_eq_open(fab->fab_fab, &eq_attr, &aep->aep_tx_res.ftr_eq, NULL);
	if (rc != 0)
		return M0_ERR(rc);

	aep->aep_tx_res.ftr_ctx.evctx_type = FAB_PRIVATE_Q_EVENT;
	aep->aep_tx_res.ftr_ctx.evctx_ep = ctx;
	rc = libfab_waitfd_bind(&aep->aep_tx_res.ftr_eq->fid, tm,
				&aep->aep_tx_res.ftr_ctx);
	if (rc != 0)
		return M0_ERR(rc);

	rc = fi_ep_bind(aep->aep_txep, &aep->aep_tx_res.ftr_eq->fid, 0);

	return rc != 0 ? M0_ERR(rc) : M0_RC(0);
}

/**
 * Initialize receive endpoint resources and associate
 * it to the active receive endpoint.
 */
static int libfab_ep_rxres_init(struct m0_fab__active_ep *aep,
				struct m0_fab__tm *tm, void *ctx)
{
	struct fi_cq_attr   cq_attr = {};
	struct fi_eq_attr   eq_attr = {};
	struct m0_fab__fab *fab;
	int                 rc;

	fab = tm->ftm_fab;

	/* Initialise and bind completion queues for rx */
	cq_attr.wait_obj = FI_WAIT_FD;
	cq_attr.wait_cond = FI_CQ_COND_NONE;
	cq_attr.format = FI_CQ_FORMAT_DATA;
	cq_attr.size = FAB_MAX_RX_CQ_EV;
	rc = fi_cq_open(fab->fab_dom, &cq_attr, &aep->aep_rx_res.frr_cq, NULL);
	if (rc != 0)
		return M0_ERR(rc);

	aep->aep_rx_res.frr_ctx.evctx_type = FAB_PRIVATE_Q_EVENT;
	aep->aep_rx_res.frr_ctx.evctx_ep = ctx;
	rc = libfab_waitfd_bind(&aep->aep_rx_res.frr_cq->fid, tm,
				&aep->aep_rx_res.frr_ctx);
	if (rc != 0)
		return M0_ERR(rc);
	
	rc = fi_ep_bind(aep->aep_rxep, &tm->ftm_tx_cq->fid,
			FI_TRANSMIT | FI_SELECTIVE_COMPLETION) ? :
	     fi_ep_bind(aep->aep_rxep, &aep->aep_rx_res.frr_cq->fid, FI_RECV);
	if (rc != 0)
		return M0_ERR(rc);

	/* Initialise and bind event queue */
	eq_attr.wait_obj = FI_WAIT_FD;
	eq_attr.size = FAB_MAX_AEP_EQ_EV;
	rc = fi_eq_open(fab->fab_fab, &eq_attr, &aep->aep_rx_res.frr_eq,
			NULL) ? :
	     libfab_waitfd_bind(&aep->aep_rx_res.frr_eq->fid, tm,
				&aep->aep_rx_res.frr_ctx) ? :
	     fi_ep_bind(aep->aep_rxep, &aep->aep_rx_res.frr_eq->fid, 0) ? :
	     fi_ep_bind(aep->aep_rxep, &tm->ftm_rctx->fid, 0);

	return rc != 0 ? M0_ERR(rc) : M0_RC(0);
}

/**
 * Initialize passive endpoint resources and associate
 * it to the passive endpoint.
 */
static int libfab_pep_res_init(struct m0_fab__passive_ep *pep,
			       struct m0_fab__tm *tm, void *ctx)
{
	struct fi_eq_attr eq_attr = {};
	int               rc = 0;

	/* Initialise and bind event queue */
	eq_attr.wait_obj = FI_WAIT_FD;
	eq_attr.size = FAB_MAX_PEP_EQ_EV;
	rc = fi_eq_open(tm->ftm_fab->fab_fab, &eq_attr, &pep->pep_res.fpr_eq,
			NULL);
	if (rc != 0)
		return M0_ERR(rc);
	
	pep->pep_res.fpr_ctx.evctx_type = FAB_COMMON_Q_EVENT;
	pep->pep_res.fpr_ctx.evctx_ep = ctx;
	rc = libfab_waitfd_bind(&pep->pep_res.fpr_eq->fid, tm,
				&pep->pep_res.fpr_ctx) ? :
	     fi_pep_bind(pep->pep_pep, &pep->pep_res.fpr_eq->fid, 0);
	
	return rc != 0 ? M0_ERR(rc) : M0_RC(0);
}

/**
 * Used to accept an incoming connection request
 */
static int libfab_conn_accept(struct m0_fab__ep *ep, struct m0_fab__tm *tm,
			      struct fi_info *info)
{
	struct m0_fab__active_ep *aep;
	struct fid_domain        *dp;
	int                       rc;

	M0_ENTRY("from ep=%s -> tm = %s", (char*)ep->fep_name_p.fen_str_addr,
		 (char*)tm->ftm_pep->fep_name_p.fen_str_addr);

	aep = libfab_aep_get(ep);
	dp = tm->ftm_fab->fab_dom;
	
	if (aep->aep_rxep != NULL) {
		rc = fi_close(&aep->aep_rxep->fid);
		if (rc != 0)
			M0_LOG(M0_ERROR, "ep close = %d",rc);
		libfab_ep_rxres_free(&aep->aep_rx_res, tm);
	}
	aep->aep_rx_state = FAB_NOT_CONNECTED;
	ep->fep_connlink = FAB_CONNLINK_DOWN;

	rc = fi_endpoint(dp, info, &aep->aep_rxep, NULL) ? :
	     libfab_ep_rxres_init(aep, tm, ep) ? :
	     fi_enable(aep->aep_rxep) ? :
	     fi_accept(aep->aep_rxep, NULL, 0);

	if (rc != 0) {
		libfab_aep_param_free(aep, tm);
		return M0_ERR(rc);
	}

	aep->aep_rx_state = FAB_CONNECTING;

	return M0_RC(0);
}

/**
 * Used to create an active endpoint
 */
static int libfab_active_ep_create(struct m0_fab__ep *ep, struct m0_fab__tm *tm)
{
	struct m0_net_end_point  *net;
	struct m0_fab__active_ep *aep;
	int                       rc;

	M0_ASSERT(libfab_tm_is_locked(tm));
	aep = ep->fep_aep;
	rc = libfab_txep_init(aep, tm, ep);
	if (rc != 0) {
		libfab_aep_param_free(aep, tm);
		return M0_ERR(rc);
	}

	net = &ep->fep_nep;
	net->nep_tm = tm->ftm_ntm;
	libfab_ep_pton(&ep->fep_name_p, &ep->fep_name_n);
	m0_nep_tlink_init_at_tail(net, &tm->ftm_ntm->ntm_end_points);
	net->nep_addr = (const char *)(&ep->fep_name_p.fen_str_addr);
	m0_ref_init(&ep->fep_nep.nep_ref, 1, &libfab_ep_release);
	
	return M0_RC(0);
}

/**
 * Used to create a passive endpoint which will
 * listen for incoming connection requests. (Server)
 */
static int libfab_passive_ep_create(struct m0_fab__ep *ep, 
				    struct m0_fab__tm *tm)
{
	struct m0_fab__passive_ep *pep;
	struct fi_info            *hints;
	struct fi_info            *fi;
	enum m0_fab__prov_type     idx;
	int                        rc;
	int                        rx_size;
	char                      *addr = NULL;
	char                      *port = NULL;

	M0_ENTRY("ep=%s addr=%s port=%s", (char*)ep->fep_name_p.fen_str_addr,
		 (char*)ep->fep_name_p.fen_addr,
		 (char*)ep->fep_name_p.fen_port);

	M0_ALLOC_PTR(ep->fep_listen);
	if (ep->fep_listen == NULL)
		return M0_ERR(-ENOMEM);
	M0_ALLOC_PTR(ep->fep_listen->pep_aep);
	if (ep->fep_listen->pep_aep == NULL) {
		m0_free(ep->fep_listen);
		return M0_ERR(-ENOMEM);
	}

	pep = ep->fep_listen;
	ep->fep_listen->pep_aep->aep_rxep = NULL;
	ep->fep_listen->pep_aep->aep_txep = NULL;

	if (strlen(ep->fep_name_p.fen_port) != 0) {
		addr = ep->fep_name_p.fen_addr;
		port = ep->fep_name_p.fen_port;
	}

	hints = fi_allocinfo();
	if (hints == NULL) {
		m0_free(pep->pep_aep);
		m0_free(pep);
		return M0_ERR(-ENOMEM);
	}

	hints->ep_attr->type = FI_EP_MSG;
	hints->caps = FI_MSG | FI_RMA;
	hints->mode |= FI_RX_CQ_DATA;
	hints->domain_attr->mr_mode = FI_MR_LOCAL | FI_MR_ALLOCATED |
				      FI_MR_PROV_KEY | FI_MR_VIRT_ADDR;
	hints->domain_attr->cq_data_size = 4;

	for (idx = 0; idx < FAB_FABRIC_PROV_MAX; idx++) {
		hints->fabric_attr->prov_name = (char *)providers[idx];
		rc = fi_getinfo(LIBFAB_VERSION, addr, port, FI_SOURCE, hints,
				&fi);
		if (rc == 0)
			break;
	}

	if (rc != 0)
		return M0_ERR(rc);

	M0_ASSERT(idx < FAB_FABRIC_PROV_MAX);

	M0_LOG(M0_DEBUG, "tm = %s Provider selected %s",
	       (char*)ep->fep_name_p.fen_str_addr, fi->fabric_attr->prov_name);
	hints->fabric_attr->prov_name = NULL;
	tm->ftm_fab->fab_fi = fi;
	tm->ftm_fab->fab_prov = idx;
	fi_freeinfo(hints);

	rc = fi_fabric(tm->ftm_fab->fab_fi->fabric_attr, &tm->ftm_fab->fab_fab,
		       NULL) ? :
	     libfab_waitfd_init(tm) ? :
	     fi_passive_ep(tm->ftm_fab->fab_fab, tm->ftm_fab->fab_fi,
			   &pep->pep_pep, NULL) ? :
	     libfab_pep_res_init(pep, tm, ep) ? :
	     fi_listen(pep->pep_pep) ? :
	     fi_domain(tm->ftm_fab->fab_fab, tm->ftm_fab->fab_fi,
		       &tm->ftm_fab->fab_dom, NULL);

	if (rc != 0) {
		libfab_pep_param_free(pep, tm);
		return M0_ERR(rc);
	}

	rx_size = tm->ftm_fab->fab_fi->rx_attr->size;
	tm->ftm_fab->fab_fi->rx_attr->size = FAB_MAX_SRX_SIZE;
	rc = fi_srx_context(tm->ftm_fab->fab_dom, tm->ftm_fab->fab_fi->rx_attr,
			    &tm->ftm_rctx, NULL);
	tm->ftm_fab->fab_fi->rx_attr->size = rx_size;
	if (rc != 0) {
		M0_LOG(M0_ERROR," \n fi_srx_context = %d \n ", rc);
		libfab_pep_param_free(pep, tm);
		return M0_ERR(rc);
	}

	rc = libfab_tm_res_init(tm);
	if(rc != 0){
		M0_LOG(M0_ERROR," \n libfab_tm_res_init = %d \n ", rc);
		libfab_pep_param_free(pep, tm);
		return M0_ERR(rc);
	}

	fab_sndbuf_tlist_init(&ep->fep_sndbuf);
	m0_ref_init(&tm->ftm_pep->fep_nep.nep_ref, 1, &libfab_ep_release);
	libfab_ep_get(tm->ftm_pep);

	return M0_RC(0);
}

/**
 * Used to free the resources attached to an passive endpoint
 */
static int libfab_pep_res_free(struct m0_fab__pep_res *pep_res,
			       struct m0_fab__tm *tm)
{
	int rc = 0;

	if (pep_res->fpr_eq != NULL) {
		rc = fi_close(&pep_res->fpr_eq->fid);
		if (rc != 0)
			M0_LOG(M0_ERROR, "fpr_eq fi_close ret=%d fid=%d",
			       rc, (int)pep_res->fpr_eq->fid.fclass);
		pep_res->fpr_eq = NULL;
	}

	return M0_RC(rc);
}

/**
 * Used to free the resources attached to an active transmit endpoint
 */
static int libfab_ep_txres_free(struct m0_fab__tx_res *tx_res,
				struct m0_fab__tm *tm)
{
	int rc = 0;

	if (tx_res->ftr_eq != NULL) {
		rc = fi_close(&tx_res->ftr_eq->fid);
		if (rc != 0)
			M0_LOG(M0_ERROR, "ftr_eq fi_close ret=%d fid=%d",
			       rc, (int)tx_res->ftr_eq->fid.fclass);
		tx_res->ftr_eq = NULL;
	}

	return M0_RC(rc);
}

/**
 * Used to free the resources attached to an active receive endpoint
 */
static int libfab_ep_rxres_free(struct m0_fab__rx_res *rx_res,
				struct m0_fab__tm *tm)
{
	int rc = 0;

	if (rx_res->frr_eq != NULL) {
		rc = fi_close(&rx_res->frr_eq->fid);
		if (rc != 0)
			M0_LOG(M0_ERROR, "frr_eq fi_close ret=%d fid=%d",
			       rc, (int)rx_res->frr_eq->fid.fclass);
		rx_res->frr_eq = NULL;
	}

	if (rx_res->frr_cq != NULL) {
		rc = fi_close(&rx_res->frr_cq->fid);
		if (rc != 0)
			M0_LOG(M0_ERROR, "frr_cq fi_close ret=%d fid=%d",
			       rc, (int)rx_res->frr_cq->fid.fclass);
		rx_res->frr_cq = NULL;
	}

	return M0_RC(rc);
}

/**
 * Used to free the active endpoint
 */
static int libfab_aep_param_free(struct m0_fab__active_ep *aep,
				 struct m0_fab__tm *tm)
{
	int rc = 0;

	if (aep == NULL)
		return M0_RC(0);
	if (aep->aep_txep != NULL) {
		rc = fi_close(&aep->aep_txep->fid);
		if (rc != 0)
			M0_LOG(M0_ERROR, "aep_txep fi_close ret=%d fid=%d",
			       rc, (int)aep->aep_txep->fid.fclass);
		aep->aep_txep = NULL;
	}

	if (aep->aep_rxep != NULL) {
		rc = fi_close(&aep->aep_rxep->fid);
		if (rc != 0)
			M0_LOG(M0_ERROR, "aep_rxep fi_close ret=%d fid=%d",
			       rc, (int)aep->aep_rxep->fid.fclass);
		aep->aep_rxep = NULL;
	}

	rc = libfab_ep_txres_free(&aep->aep_tx_res, tm);
	if (rc != 0)
		M0_LOG(M0_ERROR, "ep_txres_free failed %d", rc);
	
	rc = libfab_ep_rxres_free(&aep->aep_rx_res, tm);
	if (rc != 0)
		M0_LOG(M0_ERROR, "ep_rxres_free failed %d", rc);
	
	m0_free(aep);

	return M0_RC(rc);
}

/**
 * Used to free the passive endpoint resources.
 */
static int libfab_pep_param_free(struct m0_fab__passive_ep *pep,
				 struct m0_fab__tm *tm)
{
	int rc = 0;

	if (pep == NULL)
		return M0_RC(0);
	
	if (pep->pep_pep != NULL) {
		rc = fi_close(&pep->pep_pep->fid);
		if (rc != 0)
			M0_LOG(M0_ERROR, "fep_pep fi_close ret=%d fid=%d",
			       rc, (int)pep->pep_pep->fid.fclass);
		pep->pep_pep = NULL;
	}
	
	rc = libfab_aep_param_free(pep->pep_aep, tm);
	if (rc != 0)
		M0_LOG(M0_ERROR, "aep_param_free failed %d", rc);

	rc = libfab_pep_res_free(&pep->pep_res, tm);
	if (rc != 0)
		M0_LOG(M0_ERROR, "pep_res_free failed %d", rc);
	
	m0_free(pep);

	return M0_RC(rc);
}

/**
 * Used to free the endpoint and its resources.
 */
static int libfab_ep_param_free(struct m0_fab__ep *ep, struct m0_fab__tm *tm)
{
	int rc;

	if (ep == NULL)
		return M0_RC(0);

	rc = libfab_pep_param_free(ep->fep_listen, tm) ? :
	     libfab_aep_param_free(ep->fep_aep, tm);

	if (rc != 0)
		return M0_ERR(rc);

	M0_SET0(&ep->fep_name_p);

	m0_free(ep);
	return M0_RC(0);
}

/**
 * Used to free the transfer machine parameters
 */
static int libfab_tm_param_free(struct m0_fab__tm *tm)
{
	struct m0_fab__bulk_op  *op;
	struct m0_net_end_point *net;
	struct m0_fab__ep       *xep;
	struct m0_fab__buf      *fbp;
	int                      rc;

	if (tm == NULL)
		return M0_RC(0);

	if (tm->ftm_poller.t_func != NULL) {
		m0_thread_join(&tm->ftm_poller);
		m0_thread_fini(&tm->ftm_poller);
	}

	M0_ASSERT(libfab_tm_is_locked(tm));
	m0_tl_teardown(m0_nep, &tm->ftm_ntm->ntm_end_points, net) {
		xep = libfab_ep(net);
		rc = libfab_ep_param_free(xep, tm);
	}
	M0_ASSERT(m0_nep_tlist_is_empty(&tm->ftm_ntm->ntm_end_points));
	tm->ftm_ntm->ntm_ep = NULL;
	
	if (tm->ftm_rctx != NULL) {
		rc = fi_close(&tm->ftm_rctx->fid);
		if (rc != 0)
			M0_LOG(M0_ERROR, "ftm_rctx fi_close ret=%d fid=%d",
			       rc, (int)tm->ftm_rctx->fid.fclass);
		tm->ftm_rctx = NULL;
	}

	if (tm->ftm_tx_cq != NULL) {
		rc = fi_close(&tm->ftm_tx_cq->fid);
		if (rc != 0)
			M0_LOG(M0_ERROR, "tx_cq fi_close ret=%d fid=%d",
			       rc, (int)tm->ftm_tx_cq->fid.fclass);
		tm->ftm_tx_cq = NULL;
	}
	
	close(tm->ftm_epfd);

	m0_htable_for(fab_bufhash, fbp, &tm->ftm_bufhash.bht_hash) {
		fab_bufhash_htable_del(&tm->ftm_bufhash.bht_hash, fbp);
	} m0_htable_endfor;
	fab_bufhash_htable_fini(&tm->ftm_bufhash.bht_hash);
	
	m0_tl_teardown(fab_bulk, &tm->ftm_bulk, op) {
		m0_free(op);
	}
	fab_bulk_tlist_fini(&tm->ftm_bulk);

	return M0_RC(0);
}

/**
 * Used to initialize the epoll file descriptor for the transfer machine
 * This is the wait mechanism for the event and completion queues.
 */
static int libfab_waitfd_init(struct m0_fab__tm *tm)
{
	M0_PRE(tm->ftm_epfd == -1);

	tm->ftm_epfd = epoll_create(1);
	if (tm->ftm_epfd < 0)
		return M0_ERR(-errno);

	return M0_RC(0);
}

/**
 * Used to fetch the transfer machine associated with the buffer.
 */
static inline struct m0_fab__tm *libfab_buf_tm(struct m0_fab__buf *buf)
{
	return buf->fb_nb->nb_tm->ntm_xprt_private;
}

/**
 * Used to fetch the transfer machine associated with the network buffer.
 */
static inline struct m0_fab__tm *libfab_buf_ma(struct m0_net_buffer *buf)
{
	return buf->nb_tm != NULL ? buf->nb_tm->ntm_xprt_private : NULL;
}

/**
 * Used to clean up the libfabric buffer structure.
 */
static void libfab_buf_fini(struct m0_fab__buf *buf)
{
	struct m0_fab__buf *fbp;

	M0_ENTRY("fb=%p q=%d rc=%d", buf, buf->fb_nb->nb_qtype, buf->fb_status);

	libfab_buf_invariant(buf);

	fab_buf_tlink_fini(buf);
	if (buf->fb_ev_ep != NULL)
		buf->fb_ev_ep = NULL;
	if (buf->fb_bulk_op != NULL && fab_bulk_tlink_is_in(buf->fb_bulk_op)) {
		fab_bulk_tlist_del(buf->fb_bulk_op);
		m0_free(buf->fb_bulk_op);
	}

	if(buf->fb_txctx != NULL) {
		fbp = m0_tl_find(fab_sndbuf, fbp, &buf->fb_txctx->fep_sndbuf,
				 fbp == buf);
		if (fbp != NULL) {
			fab_sndbuf_tlist_del(fbp);
			M0_LOG(M0_DEBUG, "buf=%p tmout/del before queued", fbp);
		}
	}
	buf->fb_status = 0;
	buf->fb_length = 0;
	buf->fb_token = 0;
	/*
	 * If the buffer operation has timedout or has been cancelled by
	 * application, then the buffer has also been de-registered to prevent
	 * data corruption due to any ongoing operations. In such cases, the
	 * buffer state is reset to FAB_BUF_INITIALIZED so that it will be
	 * re-registered when the application will try to re-use it.
	 */
	buf->fb_state = (buf->fb_state == FAB_BUF_CANCELED ||
			 buf->fb_state == FAB_BUF_TIMEDOUT) ?
			FAB_BUF_INITIALIZED : FAB_BUF_REGISTERED;

	M0_LEAVE("fb_state=%d", buf->fb_state);
}

/**
 * Check sanity of domain structure
 */
static bool libfab_dom_invariant(const struct m0_net_domain *dom)
{
	struct m0_fab__ndom *fnd = dom->nd_xprt_private;
	return _0C(!fab_fabs_tlist_is_empty(&fnd->fnd_fabrics)) &&
	       _0C(dom->nd_xprt == &m0_net_libfab_xprt);
}

/**
 * Check sanity of transfer machine structure
 */
static bool libfab_tm_invariant(const struct m0_fab__tm *fab_tm)
{
	return fab_tm != NULL &&
	       fab_tm->ftm_ntm->ntm_xprt_private == fab_tm &&
	       libfab_dom_invariant(fab_tm->ftm_ntm->ntm_dom);
}

/**
 * Check sanity of buffer structure
 */
static bool libfab_buf_invariant(const struct m0_fab__buf *buf)
{
	const struct m0_net_buffer *nb = buf->fb_nb;

	return  (nb->nb_flags == M0_NET_BUF_REGISTERED &&
		 nb->nb_tm == NULL) ^ /* or (exclusively) ... */
		/* it is queued to a machine. */
		(_0C(nb->nb_flags & (M0_NET_BUF_REGISTERED|M0_NET_BUF_QUEUED))&&
		 _0C(nb->nb_tm != NULL) &&
		 _0C(m0_net__buffer_invariant(nb)));
}

/**
 * Invokes completion call-back (releasing transfer-machine lock).
 */
static void libfab_buf_complete(struct m0_fab__buf *buf)
{
	struct m0_fab__tm    *ma = libfab_buf_tm(buf);
	struct m0_net_buffer *nb = buf->fb_nb;
	struct m0_net_buffer_event ev = {
		.nbe_buffer = nb,
		.nbe_status = buf->fb_status,
		.nbe_time   = m0_time_now()
	};

	M0_ENTRY("fb=%p nb=%p q=%d rc=%d", buf, nb, buf->fb_nb->nb_qtype, 
		 buf->fb_status);
	if (M0_IN(nb->nb_qtype, (M0_NET_QT_MSG_RECV,
				 M0_NET_QT_PASSIVE_BULK_RECV,
				 M0_NET_QT_ACTIVE_BULK_RECV))) {
		ev.nbe_length = buf->fb_length;
	}
	
	if (nb->nb_qtype == M0_NET_QT_MSG_RECV) {
		if (ev.nbe_status == 0 && buf->fb_ev_ep != NULL) {
			ev.nbe_ep = &buf->fb_ev_ep->fep_nep;
			libfab_ep_get(buf->fb_ev_ep);
		}
	}
	ma->ftm_ntm->ntm_callback_counter++;

	fab_bufhash_htable_del(&ma->ftm_bufhash.bht_hash, buf);
	libfab_buf_fini(buf);
	M0_ASSERT(libfab_tm_invariant(ma));
	libfab_tm_evpost_lock(ma);
	libfab_tm_unlock(ma);
	m0_net_buffer_event_post(&ev);
	libfab_tm_lock(ma);
	libfab_tm_evpost_unlock(ma);
	M0_ASSERT(libfab_tm_invariant(ma));
	M0_ASSERT(M0_IN(ma->ftm_ntm->ntm_state, (M0_NET_TM_STARTED,
						 M0_NET_TM_STOPPING)));
	ma->ftm_ntm->ntm_callback_counter--;
}

/**
 * This function will check if the received message is a dummy message for
 * notification of RDMA operation completion.
 * If yes it will return 0 or else return -1.
 */
static int libfab_dummy_msg_rcv_chk(struct m0_fab__buf *fbp)
{
	struct m0_fab__tm    *ma = libfab_buf_tm(fbp);
	struct m0_net_buffer *nb = fbp->fb_nb;
	struct m0_fab__buf   *pas_buf;
	struct iovec          iv;
	uint32_t             *ptr;
	uint32_t              token;
	int                   ret = -1;

	if (fbp->fb_length == (sizeof(uint32_t) * 2)) {
		ptr = (uint32_t *)nb->nb_buffer.ov_buf[0];
		if (*ptr == FAB_DUMMY_DATA) {
			ptr++;
			token = *ptr;
			pas_buf = fab_bufhash_htable_lookup(
				&ma->ftm_bufhash.bht_hash,
				&token);
			if (pas_buf != NULL) {
				pas_buf->fb_status = 0;
				libfab_buf_complete(pas_buf);
			}

			/*
			 * Repost this buffer to the receive
			 * queue without generating a callback
			 * as it contains only dummy data
			 */
			fbp->fb_length = nb->nb_length;
			iv.iov_base = nb->nb_buffer.ov_buf[0];
			iv.iov_len =  nb->nb_buffer.ov_vec.v_count[0];
			M0_ASSERT(fi_recvv(ma->ftm_rctx, &iv,
				  fbp->fb_mr.bm_desc, 1, 0, 
				  U32_TO_VPTR(fbp->fb_token)) == 0);
			ret = 0;
		}
	}

	return ret;
}

/**
 * Completes the buffer operation.
 */
static void libfab_buf_done(struct m0_fab__buf *buf, int rc, bool add_to_list)
{
	struct m0_fab__tm    *ma = libfab_buf_tm(buf);
	struct m0_net_buffer *nb = buf->fb_nb;
	
	M0_ENTRY("fb=%p nb=%p q=%d len=%d rc=%d", buf, nb, nb->nb_qtype,
		 (int)buf->fb_length, rc);
	M0_PRE(libfab_tm_is_locked(ma));
	/*
	 * Multiple libfab_buf_done() calls on the same buffer are possible if
	 * the buffer is cancelled.
	 */
	if (!fab_buf_tlink_is_in(buf)) {
		buf->fb_status = buf->fb_status == 0 ? rc : buf->fb_status;
		/* Try to finalise. */
		if (m0_thread_self() == &ma->ftm_poller && !add_to_list) {
			if (libfab_dummy_msg_rcv_chk(buf) != 0)
				libfab_buf_complete(buf);
		} else {
			/*
			 * Otherwise, postpone finalisation to
			 * libfab_tm_buf_done().
			 */
			buf->fb_status = rc;
			fab_buf_tlist_add_tail(&ma->ftm_done, buf);
		}
	}
}

/**
 * Increments the ref count of the endpoint.
 */
static inline void libfab_ep_get(struct m0_fab__ep *ep)
{
	m0_ref_get(&ep->fep_nep.nep_ref);
}

/**
 * End-point finalisation call-back.
 *
 * Used as m0_net_end_point::nep_ref::release(). This call-back is called when
 * end-point reference count drops to 0.
 */
static void libfab_ep_release(struct m0_ref *ref)
{
	struct m0_net_end_point *nep;
	struct m0_fab__ep       *ep;
	struct m0_fab__tm       *tm;

	nep = container_of(ref, struct m0_net_end_point, nep_ref);
	ep = libfab_ep(nep);
	tm = nep->nep_tm->ntm_xprt_private;
	M0_LOG(M0_DEBUG, "free endpoint %s", (char*)ep->fep_name_p.fen_str_addr);

	m0_nep_tlist_del(nep);
	libfab_ep_param_free(ep, tm);
}

/**
 * Generate unique key for memory registration.
 */
static uint64_t libfab_mr_keygen(struct m0_fab__tm *tm)
{
	uint64_t key = FAB_MR_KEY + tm->ftm_mr_key_idx;
	tm->ftm_mr_key_idx++;
	return key;
}

/**
 * Read single event from event queue.
 */
static int libfab_check_for_event(struct fid_eq *eq, uint32_t *ev)
{
	struct fi_eq_cm_entry  entry;
	struct fi_eq_err_entry err_entry;
	uint32_t               event = 0;
	int                    rc;

	rc = fi_eq_read(eq, &event, &entry, sizeof(entry), 0);
	if (rc == -FI_EAVAIL) {
		fi_eq_readerr(eq, &err_entry, 0);
		rc = -err_entry.err;
		M0_LOG(M0_DEBUG, "Error = %d %s %s\n", rc,
		       fi_strerror(err_entry.err),
		       fi_eq_strerror(eq, err_entry.prov_errno,
				      err_entry.err_data,NULL, 0));
	}

	*ev = rc < 0 ? 0xFF : event;
	return rc;
}

/**
 * A helper function to read the entries from a completion queue
 * If success, returns the number of entries read
 * else returns the negative error code
 */
static int libfab_check_for_comp(struct fid_cq *cq, uint32_t *ctx,
				 m0_bindex_t *len, uint64_t *data)
{
	struct fi_cq_data_entry entry[FAB_MAX_COMP_READ];
	struct fi_cq_err_entry  err_entry;
	uint64_t                wr_cqdata = FI_REMOTE_WRITE | FI_REMOTE_CQ_DATA;
	int                     i;
	int                     ret;
	
	ret = fi_cq_read(cq, entry, FAB_MAX_COMP_READ);
	if (ret > 0) {
		for (i = 0; i < ret; i++) {
			ctx[i] = entry[i].op_context == NULL ? 0 :
				 VPTR_TO_U32(entry[i].op_context);
			if (len != NULL)
				len[i] = entry[i].len;
			if (data != NULL)
				data[i] = ((entry[i].flags & wr_cqdata)) ? 
					  entry[i].data : 0;
		}
	} else if (ret != -FI_EAGAIN) {
		fi_cq_readerr(cq, &err_entry, 0);
		M0_LOG(M0_DEBUG, "Error = %d %s %s\n", ret,
		       fi_strerror(err_entry.err),
		       fi_cq_strerror(cq, err_entry.prov_errno,
				      err_entry.err_data, NULL, 0));
	}

	return ret;
}

/**
 * Finalises the transfer machine.
 *
 * This is called from the normal finalisation path (ma_fini(), ma_stop()) and
 * in error cleanup case during initialisation (tm_init()).
 */
static void libfab_tm_fini(struct m0_net_transfer_mc *tm)
{
	struct m0_fab__tm *ma = tm->ntm_xprt_private;
	int                rc;

	if (ma->ftm_state != FAB_TM_SHUTDOWN) {
		while (1) {
			libfab_tm_lock(ma);
			if (m0_mutex_trylock(&ma->ftm_evpost) != 0) {
				libfab_tm_unlock(ma);
			} else
				break;
		}
		m0_mutex_unlock(&ma->ftm_evpost);
		m0_mutex_lock(&ma->ftm_endlock);
		ma->ftm_state = FAB_TM_SHUTDOWN;
		m0_mutex_unlock(&ma->ftm_endlock);

		libfab_tm_buf_done(ma);

		rc = libfab_tm_param_free(ma);
		if (rc != 0)
			M0_LOG(M0_ERROR, "libfab_tm_param_free ret=%d", rc);

		m0_mutex_fini(&ma->ftm_endlock);
		m0_mutex_fini(&ma->ftm_evpost);
		libfab_tm_unlock(ma);
	}
	
	M0_LEAVE();
}

/**
 * Encodes the descriptor for a (passive) network buffer.
 */
static int libfab_bdesc_encode(struct m0_fab__buf *buf)
{
	struct m0_fab__bdesc   *fbd;
	struct fi_rma_iov      *iov;
	struct m0_net_buf_desc *nbd = &buf->fb_nb->nb_desc;
	struct m0_net_buffer   *nb = buf->fb_nb;
	struct m0_fab__tm      *tm = libfab_buf_ma(nb);
	int                     seg_nr = nb->nb_buffer.ov_vec.v_nr;
	struct m0_fab__ndom    *nd = nb->nb_dom->nd_xprt_private;
	int                     i;
	bool                    is_verbs = libfab_is_verbs(tm);

	M0_PRE(seg_nr <= nd->fnd_seg_nr);

	nbd->nbd_len = (sizeof(struct m0_fab__bdesc) +
			(sizeof(struct fi_rma_iov) * seg_nr));
	nbd->nbd_data = m0_alloc(nbd->nbd_len);
	if (nbd->nbd_data == NULL)
		return M0_RC(-ENOMEM);

	fbd = (struct m0_fab__bdesc *)nbd->nbd_data;
	fbd->fbd_netaddr = tm->ftm_pep->fep_name_n;
	fbd->fbd_buftoken = buf->fb_token;

	fbd->fbd_iov_cnt = (uint32_t)seg_nr;
	iov = (struct fi_rma_iov *)(nbd->nbd_data + 
				    sizeof(struct m0_fab__bdesc));

	for (i = 0; i < seg_nr; i++) {
		iov[i].addr = is_verbs ? (uint64_t)nb->nb_buffer.ov_buf[i] : 0;
		iov[i].key  = fi_mr_key(buf->fb_mr.bm_mr[i]);
		iov[i].len  = nb->nb_buffer.ov_vec.v_count[i];
	}

	return M0_RC(0);
}

/**
 * Decodes the descriptor of a (passive) network buffer.
 */
static void libfab_bdesc_decode(struct m0_fab__buf *fb, 
				struct m0_fab__ep_name *epname)
{
	struct m0_net_buffer *nb = fb->fb_nb;
	struct m0_fab__ndom  *ndom = nb->nb_dom->nd_xprt_private;

	fb->fb_rbd = (struct m0_fab__bdesc *)(nb->nb_desc.nbd_data);
	fb->fb_riov = (struct fi_rma_iov *)(nb->nb_desc.nbd_data + 
					    sizeof(struct m0_fab__bdesc));
	libfab_ep_ntop(fb->fb_rbd->fbd_netaddr, epname);
	M0_ASSERT(fb->fb_rbd->fbd_iov_cnt <= ndom->fnd_seg_nr);
}

/**
 * Register the buffer with the appropriate access to the domain
 */
static int libfab_buf_dom_reg(struct m0_net_buffer *nb, struct m0_fab__tm *tm)
{
	struct m0_fab__buf    *fbp;
	struct m0_fab__buf_mr *mr;
	struct m0_fab__ndom   *ndom;
	struct fid_domain     *dp;
	uint64_t               key;
	uint32_t               retry_cnt;
	int                    seg_nr;
	int                    i;
	int                    ret = 0;

	M0_PRE(nb != NULL && nb->nb_dom != NULL && tm != NULL);
	fbp = nb->nb_xprt_private;
	seg_nr = nb->nb_buffer.ov_vec.v_nr;
	ndom = nb->nb_dom->nd_xprt_private;
	dp = tm->ftm_fab->fab_dom;

	M0_ASSERT(fbp != NULL && dp != NULL && ndom != NULL);
	M0_ASSERT(seg_nr <= ndom->fnd_seg_nr);

	mr = &fbp->fb_mr;
	if (fbp->fb_dp == dp)
		return M0_RC(ret);

	if (fbp->fb_state == FAB_BUF_REGISTERED)
		M0_LOG(M0_ERROR,"Re-registration of buffer");

	for (i = 0; i < seg_nr; i++) {
		/*
		 * Sometimes the requested key is not available and
		 * hence try with some other key for registration
		 */
		ret = -1;
		retry_cnt = 20;

		while (ret != 0 && retry_cnt > 0) {
			key = libfab_mr_keygen(tm);
			ret = fi_mr_reg(dp, nb->nb_buffer.ov_buf[i],
					nb->nb_buffer.ov_vec.v_count[i],
					FAB_MR_ACCESS, FAB_MR_OFFSET, key,
					FAB_MR_FLAG, &mr->bm_mr[i], NULL);
			--retry_cnt;
		}

		if (ret != 0) {
			M0_LOG(M0_ERROR, "fi_mr_reg failed %d key=0x%"PRIx64,
			       ret, key);
			break;
		}

		mr->bm_desc[i] = fi_mr_desc(mr->bm_mr[i]);
	}

	if (ret == 0) {
		fbp->fb_dp = dp;
		fbp->fb_state = FAB_BUF_REGISTERED;
	}

	return M0_RC(ret);
}

/**
 * Buffers are queued before connection establishment, send those buffers
 * after connection establishment
 */
static void libfab_pending_bufs_send(struct m0_fab__ep *ep)
{
	struct m0_fab__active_ep *aep;
	struct m0_fab__buf       *fbp;
	struct m0_net_buffer     *nb = NULL;
	int                       ret = 0;

	aep = libfab_aep_get(ep);
	m0_tl_teardown(fab_sndbuf, &ep->fep_sndbuf, fbp) {
		nb = fbp->fb_nb;
		fbp->fb_txctx = ep;
		switch (nb->nb_qtype) {
			case M0_NET_QT_MSG_SEND:
			case M0_NET_QT_ACTIVE_BULK_RECV:
			case M0_NET_QT_ACTIVE_BULK_SEND:
				ret = libfab_txbuf_list_add(libfab_buf_ma(nb),
							    fbp, aep);
				break;
			default:
				M0_ASSERT(0); /* Invalid queue type */
				break;
		}
		if (ret != 0)
			libfab_buf_done(fbp, ret, false);
	}

	if (nb != NULL)
		libfab_bufq_process(libfab_buf_ma(nb));
}

/**
 * Notify target endpoint about RDMA read completion,
 * so that buffer on remote endpoint shall be released.
 */
static int libfab_target_notify(struct m0_fab__buf *buf)
{
	struct m0_fab__active_ep *aep;
	struct m0_fab__buf       *fbp;
	struct m0_fab__tm        *tm;
	struct iovec              iv;
	struct fi_msg             op_msg;
	int                       ret = 0;

	if (buf->fb_nb->nb_qtype == M0_NET_QT_ACTIVE_BULK_RECV) {
		M0_ALLOC_PTR(fbp);
		if (fbp == NULL)
			return M0_ERR(-ENOMEM);
		
		fbp->fb_nb = NULL;
		fbp->fb_dummy[0] = FAB_DUMMY_DATA;
		fbp->fb_dummy[1] = buf->fb_rbd->fbd_buftoken;
		fbp->fb_txctx = buf->fb_txctx;
		aep = libfab_aep_get(fbp->fb_txctx);
		tm = libfab_buf_tm(buf);
		fbp->fb_token = libfab_buf_token_get(tm, fbp);
		aep->aep_bulk_cnt++;
		m0_tlink_init(&fab_bufhash_tl, fbp);
		fab_bufhash_htable_add(&tm->ftm_bufhash.bht_hash, fbp);
		
		iv.iov_base = fbp->fb_dummy;
		iv.iov_len = sizeof(fbp->fb_dummy);
		op_msg.msg_iov = &iv;
		op_msg.desc = NULL;
		op_msg.iov_count = 1;
		op_msg.addr = 0;
		op_msg.context = U32_TO_VPTR(fbp->fb_token);
		op_msg.data = 0;
		fbp->fb_wr_cnt = 1;
		ret = fi_sendmsg(aep->aep_txep, &op_msg, FI_COMPLETION);
		if (ret != 0) {
			M0_LOG(M0_ERROR,"tgt notify fail %d opcnt=%d", ret,
			       aep->aep_bulk_cnt);
			fab_bufhash_htable_del(&tm->ftm_bufhash.bht_hash, fbp);
			--aep->aep_bulk_cnt;
			m0_free(fbp);
		}
	}

	return M0_RC(ret);
}

/**
 * Allocate new fabric pointer and add it into list.
 */
static struct m0_fab__fab *libfab_newfab_init(struct m0_fab__ndom *fnd)
{
	struct m0_fab__fab *fab = NULL;

	M0_ALLOC_PTR(fab);
	if (fab != NULL)
		fab_fabs_tlink_init_at_tail(fab, &fnd->fnd_fabrics);
	return fab;
}

/**
 * This function fills out the connection data fields with the appropriate
 * values by parsing the source endpoint address
 */
static void libfab_conn_data_fill(struct m0_fab__conn_data *cd,
				  struct m0_fab__tm *tm)
{
	char   *h_ptr = tm->ftm_pep->fep_name_p.fen_str_addr;
	char   *t_ptr;
	char    str_portal[10]={'\0'};
	uint8_t addr_frmt = tm->ftm_pep->fep_name_p.fen_addr_frmt;
	int     len;

	cd->fcd_netaddr = tm->ftm_pep->fep_name_n;
	if (strncmp(h_ptr, "0@lo", 4) == 0)
		cd->fcd_iface = FAB_LO;
	else {
		h_ptr = strchr(h_ptr, '@');
		if (strncmp(h_ptr+1, "tcp", 3) == 0)
			cd->fcd_iface = FAB_TCP;
		else
			cd->fcd_iface = FAB_O2IB;
	}

	h_ptr = strchr(h_ptr, ':');
	if (addr_frmt == FAB_LNET_FORMAT) {
		h_ptr = strchr(h_ptr+1, ':');	/* Skip the pid "12345" */
		t_ptr = strchr(h_ptr+1, ':');
		len = t_ptr - (h_ptr+1);
		strncpy(str_portal, h_ptr+1, len);
		cd->fcd_portal = (uint16_t)atoi(str_portal);
		if(*(t_ptr+1) == '*')
			cd->fcd_tmid = 0xFFFF;
		else
			cd->fcd_tmid = (uint16_t)atoi(t_ptr+1);
	} else if (addr_frmt == FAB_NATIVE_HOSTNAME_FORMAT)
		strcpy(cd->fcd_hostname, tm->ftm_pep->fep_name_p.fen_str_addr);
	cd->fcd_addr_frmt = addr_frmt;
}

/**
 * Send out a connection request to the destination of the network buffer
 * and add given buffer into pending buffers list.
 */
static int libfab_conn_init(struct m0_fab__ep *ep, struct m0_fab__tm *ma,
			    struct m0_fab__buf *fbp)
{
	struct m0_fab__active_ep *aep;
	uint64_t                  dst;
	size_t                    cm_max_size = 0;
	size_t                    opt_size = sizeof(size_t);
	struct m0_fab__conn_data  cd;
	int                       ret = 0;

	aep = libfab_aep_get(ep);
	if (aep->aep_tx_state == FAB_NOT_CONNECTED) {
		dst = ep->fep_name_n | 0x02;
		libfab_conn_data_fill(&cd, ma);

		ret = fi_getopt(&aep->aep_txep->fid, FI_OPT_ENDPOINT,
				FI_OPT_CM_DATA_SIZE,
				&cm_max_size, &opt_size);
		M0_ASSERT(ret == 0 && sizeof(cd) < cm_max_size);

		ret = fi_connect(aep->aep_txep, &dst, &cd, sizeof(cd));
		if (ret == 0)
			aep->aep_tx_state = FAB_CONNECTING;
		else
			M0_LOG(M0_DEBUG, " Conn req failed ret=%d dst=%"PRIx64,
			       ret, dst);
	}
	
	if (ret == 0)
		fab_sndbuf_tlink_init_at_tail(fbp, &ep->fep_sndbuf);

	/*
	 * If fi_connect immediately returns -ECONNREFUSED, that means the
	 * the remote service has not yet started. In this case, set the buffer
	 * status as -ECONNREFUSED and return the status as 0 so as to avoid
	 * flooding the network with repeated retries by the RPC layer. The
	 * buffer status will be automatically returned when the buf_done list
	 * is processed.
	 */
	if (ret == -ECONNREFUSED) {
		libfab_buf_done(fbp, -ECONNREFUSED, true);
		ret = 0;
		M0_LOG(M0_DEBUG, "Err=%d fb=%p nb=%p", fbp->fb_status, fbp,
		       fbp->fb_nb);
	}

	return ret;
}

/**
 * Find endpoint with given name from the transfer machine endpoint list.
 */
static int libfab_fab_ep_find(struct m0_fab__tm *tm, struct m0_fab__ep_name *en,
			      const char *name, struct m0_fab__ep **ep)
{
	struct m0_net_transfer_mc *ntm = tm->ftm_ntm;
	struct m0_net_end_point   *nep;
	int                        ret;

	ret = libfab_ep_find(ntm, name, en, &nep);
	if (ret == 0)
		*ep = libfab_ep(nep);

	return M0_RC(ret);
}

/**
 * Convert the endpoint name from printable format to numeric format.
 */
static void libfab_ep_pton(struct m0_fab__ep_name *name, uint64_t *out)
{
	uint32_t addr = 0;
	uint32_t port = 0;

	inet_pton(AF_INET, name->fen_addr, &addr);
	port = (uint32_t)atoi(name->fen_port);
	M0_ASSERT(port < 65536);
	port = htonl(port);

	*out = ((uint64_t)addr << 32) | port;
}

/**
 * Convert the endpoint name from numeric format to printable format.
 */
static void libfab_ep_ntop(uint64_t netaddr, struct m0_fab__ep_name *name)
{
	union adpo {
		uint32_t ap[2];
		uint64_t net_addr;
	} ap;
	ap.net_addr = netaddr;
	inet_ntop(AF_INET, &ap.ap[1], name->fen_addr, LIBFAB_ADDR_LEN_MAX);
	ap.ap[0] = ntohl(ap.ap[0]);
	M0_ASSERT(ap.ap[0] < 65536);
	sprintf(name->fen_port, "%d", (int)ap.ap[0]);
}

/**
 * Initialize transmit endpoint.
 * 
 * This function creates new transmit endpoint or
 * reinitializes existing trasmit endpoint and also
 * initialize associated resources and enables the endpoint.
 */
static int libfab_txep_init(struct m0_fab__active_ep *aep,
			    struct m0_fab__tm *tm, void *ctx)
{
	struct m0_fab__ep      *ep = (struct m0_fab__ep *)ctx;
	struct m0_fab__ep_name *en = &ep->fep_name_p;
	struct m0_fab__fab     *fab = tm->ftm_fab;
	struct fi_info         *info;
	struct fi_info         *hints = NULL;
	int                     rc;
	bool                    is_verbs = libfab_is_verbs(tm);
	
	if (aep->aep_txep != NULL) {
		rc = fi_close(&aep->aep_txep->fid);
		if (rc != 0)
			M0_LOG(M0_ERROR,"aep_txep close failed %d",rc);
		
		rc = libfab_ep_txres_free(&aep->aep_tx_res, tm);
		if (rc != 0)
			M0_LOG(M0_ERROR,"ep_txres_free failed %d",rc);
	}
	aep->aep_tx_state = FAB_NOT_CONNECTED;
	aep->aep_txq_full = false;
	ep->fep_connlink = FAB_CONNLINK_DOWN;

	if (is_verbs) {
		hints = fi_allocinfo();
		if (hints == NULL)
			return M0_ERR(-ENOMEM);
		hints->ep_attr->type = FI_EP_MSG;
		hints->caps = FI_MSG | FI_RMA;

		hints->mode |= FI_RX_CQ_DATA;
		hints->domain_attr->cq_data_size = 4;
		hints->domain_attr->mr_mode = FI_MR_LOCAL | FI_MR_ALLOCATED |
					      FI_MR_PROV_KEY | FI_MR_VIRT_ADDR;
		hints->fabric_attr->prov_name =
					    fab->fab_fi->fabric_attr->prov_name;

		rc = fi_getinfo(LIBFAB_VERSION, en->fen_addr, en->fen_port, 0,
				hints, &info);
		if (rc != 0)
			return M0_ERR(rc);
	} else
		info = tm->ftm_fab->fab_fi;

	rc = fi_endpoint(fab->fab_dom, info, &aep->aep_txep, NULL) ? :
	     libfab_ep_txres_init(aep, tm, ctx) ? :
	     fi_enable(aep->aep_txep);

	if (is_verbs) {
		hints->fabric_attr->prov_name = NULL;
		fi_freeinfo(hints);
		fi_freeinfo(info);
	}

	return M0_RC(rc);
}

/**
 * Associate the event queue or completion queue to the epollfd wait mechanism
 */
static int libfab_waitfd_bind(struct fid* fid, struct m0_fab__tm *tm, void *ctx)
{
	struct epoll_event ev;
	int                fd;
	int                rc;

	rc = fi_control(fid, FI_GETWAIT, &fd);
	if (rc != 0)
		return M0_ERR(rc);

	ev.events = EPOLLIN;
	ev.data.ptr = ctx;
	rc = epoll_ctl(tm->ftm_epfd, EPOLL_CTL_ADD, fd, &ev);

	return M0_RC(rc);
}

/**
 * Return the pointer to the active endpoint from the m0_fab__ep struct
 */
static inline struct m0_fab__active_ep *libfab_aep_get(struct m0_fab__ep *ep)
{
	return ep->fep_listen == NULL ? ep->fep_aep : ep->fep_listen->pep_aep;
}

/**
 * Returns true if verbs provider is selected else false.
 */
static inline bool libfab_is_verbs(struct m0_fab__tm *tm)
{
	return tm->ftm_fab->fab_prov == FAB_FABRIC_PROV_VERBS;
}

/**
 * Add bulk operation into transfer machine bulk operation list.
 */
static int libfab_txbuf_list_add(struct m0_fab__tm *tm, struct m0_fab__buf *fb,
				 struct m0_fab__active_ep *aep)
{
	struct m0_fab__bulk_op *op;

	M0_ALLOC_PTR(op);
	if (op == NULL)
		return M0_ERR(-ENOMEM);
	op->fbl_aep = aep;
	op->fbl_buf = fb;
	fb->fb_bulk_op = op;
	fb->fb_wr_cnt = 0;
	M0_SET0(&fb->fb_xfer_params);
	fab_bulk_tlink_init_at_tail(op, &tm->ftm_bulk);

	return M0_RC(0);
}

/**
 * Process bulk operation from transfer machine bulk operation list.
 */
static void libfab_bufq_process(struct m0_fab__tm *tm)
{
	struct m0_fab__bulk_op *op;
	int                     ret;

	m0_tl_for(fab_bulk, &tm->ftm_bulk, op) {
		/*
		 * Only post the bulk buffer if the endpoint is in
		 * connected state.
		 */
		if (op->fbl_aep->aep_tx_state == FAB_CONNECTED &&
		    !op->fbl_aep->aep_txq_full) {
			if (op->fbl_buf->fb_nb->nb_qtype == M0_NET_QT_MSG_SEND)
				ret = libfab_ping_op(op->fbl_aep, op->fbl_buf);
			else
				ret = libfab_bulk_op(op->fbl_aep, op->fbl_buf);
	
			if (ret == 0) {
				fab_bulk_tlist_del(op);
				op->fbl_buf->fb_bulk_op = NULL;
				m0_free(op);
			} else
				op->fbl_aep->aep_txq_full = true;
		}
	} m0_tl_endfor;


}

/** 
 * This function will call the ping transfer operation (send/recv) on the
 * net-buffer.
 */
static int libfab_ping_op(struct m0_fab__active_ep *aep, struct m0_fab__buf *fb)
{
	struct fi_msg op_msg;
	struct iovec  iv;
	int           ret;

	iv.iov_base = fb->fb_nb->nb_buffer.ov_buf[0];
	iv.iov_len = fb->fb_nb->nb_buffer.ov_vec.v_count[0];
	op_msg.msg_iov = &iv;
	op_msg.desc = fb->fb_mr.bm_desc;
	op_msg.iov_count = 1;
	op_msg.addr = 0;
	op_msg.context = U32_TO_VPTR(fb->fb_token);
	op_msg.data = 0;
	fb->fb_wr_cnt = 1;
	ret = fi_sendmsg(aep->aep_txep, &op_msg, FI_COMPLETION);
	if (ret == 0)
		aep->aep_bulk_cnt += fb->fb_wr_cnt;

	return ret;
}

/** 
 * This function will call the bulk transfer operation (read/write) on the
 * net-buffer.
 */
static int libfab_bulk_op(struct m0_fab__active_ep *aep, struct m0_fab__buf *fb)
{
	struct m0_fab__buf_xfer_params xp;
	struct fi_msg_rma              op_msg;
	struct fi_rma_iov             *r_iov;
	struct fi_rma_iov              remote[FAB_MAX_IOV_PER_TX];
	struct iovec                   loc_iv[FAB_MAX_IOV_PER_TX];
	m0_bcount_t                   *v_cnt;
	uint64_t                       op_flag;
	uint32_t                       loc_slen;
	uint32_t                       rem_slen;
	uint32_t                       wr_cnt = 0;
	uint32_t                       idx;
	uint32_t                       max_iov_per_tx;
	int                            ret = 0;
	bool                           isread;
	bool                           last_seg = false;

	M0_ENTRY("loc_buf=%p q=%d loc_seg=%d rem_buf=%d rem_seg=%d",
		 fb, fb->fb_nb->nb_qtype, fb->fb_nb->nb_buffer.ov_vec.v_nr, 
		 (int)fb->fb_rbd->fbd_buftoken, (int)fb->fb_rbd->fbd_iov_cnt);
	M0_PRE(fb->fb_rbd != NULL);

	v_cnt = fb->fb_nb->nb_buffer.ov_vec.v_count;
	max_iov_per_tx = libfab_is_verbs(libfab_buf_tm(fb)) ?
			 FAB_VERBS_MAX_IOV_PER_TX : FAB_TCP_SOCK_MAX_IOV_PER_TX;
	/* Pick last succesfully transfered bulk buf params */
	xp = fb->fb_xfer_params;
	r_iov = fb->fb_riov;
	isread = (fb->fb_nb->nb_qtype == M0_NET_QT_ACTIVE_BULK_RECV);

	while (xp.bxp_xfer_len < fb->fb_nb->nb_length) {
		for (idx = 0; idx < max_iov_per_tx && !last_seg; idx++) {
			M0_ASSERT(xp.bxp_rem_sidx <= fb->fb_rbd->fbd_iov_cnt);
			loc_slen = v_cnt[xp.bxp_loc_sidx] - xp.bxp_loc_soff;
			rem_slen = r_iov[xp.bxp_rem_sidx].len - xp.bxp_rem_soff;
			
			loc_iv[idx].iov_base = fb->fb_nb->nb_buffer.ov_buf[
					       xp.bxp_loc_sidx] +
					       xp.bxp_loc_soff;
			loc_iv[idx].iov_len = min64u(loc_slen, rem_slen);
			remote[idx] = r_iov[xp.bxp_rem_sidx];
			remote[idx].addr += xp.bxp_rem_soff;
			remote[idx].len -= xp.bxp_rem_soff;

			if (loc_slen > rem_slen) {
				xp.bxp_rem_sidx++;
				xp.bxp_rem_soff = 0;
				xp.bxp_loc_soff += loc_iv[idx].iov_len;
			} else {
				xp.bxp_loc_sidx++;
				xp.bxp_loc_soff = 0;
				xp.bxp_rem_soff += loc_iv[idx].iov_len;
				if(xp.bxp_rem_soff >= 
						   r_iov[xp.bxp_rem_sidx].len) {
					xp.bxp_rem_sidx++;
					xp.bxp_rem_soff = 0;
				}
			}

			xp.bxp_xfer_len += loc_iv[idx].iov_len;
			if (xp.bxp_xfer_len >= fb->fb_nb->nb_length)
				last_seg = true;
		}
		
		op_msg.msg_iov       = &loc_iv[0];
		op_msg.desc          = &fb->fb_mr.bm_desc[xp.bxp_loc_sidx];
		op_msg.iov_count     = idx;
		op_msg.addr          = xp.bxp_rem_soff;
		op_msg.rma_iov       = &remote[0];
		op_msg.rma_iov_count = idx;
		op_msg.context       = U32_TO_VPTR(fb->fb_token);
		
		op_msg.data = (isread || (!last_seg)) ? 0 :
			      fb->fb_rbd->fbd_buftoken;
		op_flag = (isread || (!last_seg)) ? 0 : FI_REMOTE_CQ_DATA;
		op_flag |= last_seg ? FI_COMPLETION : 0;
		
		ret = isread ? fi_readmsg(aep->aep_txep, &op_msg, op_flag) :
		      fi_writemsg(aep->aep_txep, &op_msg, op_flag);

		if (ret != 0) {
			M0_LOG(M0_ERROR,"bulk-op failed %d b=%p q=%d l_seg=%d opcnt=%d",
			       ret, fb, fb->fb_nb->nb_qtype, xp.bxp_loc_sidx, 
			       aep->aep_bulk_cnt);
			break;
		} else {
			wr_cnt++;
			aep->aep_bulk_cnt++;
			/* Save last succesfully transfered bulk buf params */
			fb->fb_xfer_params = xp;
		}
	}
	fb->fb_wr_cnt += wr_cnt;
	return M0_RC(ret);
}

/** 
 * This function returns a unique token number.
 */
static uint32_t libfab_buf_token_get(struct m0_fab__tm *tm,
				     struct m0_fab__buf *fb)
{
	union m0_fab__token token;

	token.t_val = 0;
	token.t_Fields.tf_queue_id = (fb->fb_nb == NULL) ? M0_NET_QT_NR :
				     fb->fb_nb->nb_qtype;
	++tm->ftm_op_id;
	if (tm->ftm_op_id == 0)
		++tm->ftm_op_id; /* 0 is treated as an invalid value for token*/
	/* Queue selection round robin for a queue type */
	++tm->ftm_rr_qt[token.t_Fields.tf_queue_id];

	token.t_Fields.tf_queue_num = (tm->ftm_rr_qt[token.t_Fields.tf_queue_id]
				       % FAB_NUM_BUCKETS_PER_QTYPE);
	token.t_Fields.tf_tag = tm->ftm_op_id;

	return token.t_val;
}

static int libfab_domain_params_get(struct m0_fab__ndom *fab_ndom)
{
	struct fi_info     *hints;
	struct fi_info     *fi;
	struct sockaddr_in *v_src;
	struct sockaddr_in  t_src;
	int                 result = 0;

	hints = fi_allocinfo();
	if (hints == NULL)
		return M0_ERR(-ENOMEM);
	hints->fabric_attr->prov_name = (char *)providers[FAB_FABRIC_PROV_VERBS];
	result = fi_getinfo(FI_VERSION(1,11), NULL, NULL, 0, hints, &fi);
	if (result == 0) {
		/* For Verbs provider */
		v_src = fi->src_addr;
		inet_ntop(AF_INET, &v_src->sin_addr, fab_ndom->fnd_loc_ip,
			  ARRAY_SIZE(fab_ndom->fnd_loc_ip));
		fab_ndom->fnd_seg_nr = FAB_VERBS_IOV_MAX;
		fab_ndom->fnd_seg_size = FAB_VERBS_MAX_BULK_SEG_SIZE;
	} else {
		/* For TCP/Socket provider */
		t_src.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		inet_ntop(AF_INET, &t_src.sin_addr, fab_ndom->fnd_loc_ip,
			  ARRAY_SIZE(fab_ndom->fnd_loc_ip));
		fab_ndom->fnd_seg_nr = FAB_TCP_SOCK_IOV_MAX;
		fab_ndom->fnd_seg_size = FAB_TCP_SOCK_MAX_BULK_SEG_SIZE;
	}

	hints->fabric_attr->prov_name = NULL;
	fi_freeinfo(hints);
	fi_freeinfo(fi);
	return M0_RC(0);
}

static int libfab_buf_dom_dereg(struct m0_fab__buf *fbp)
{
	struct m0_fab__ndom *nd = fbp->fb_nb->nb_dom->nd_xprt_private;
	m0_time_t            tmout;
	int                  i;
	int                  ret = 0;

	for (i = 0; i < nd->fnd_seg_nr; i++) {
		if (fbp->fb_mr.bm_mr[i] != NULL) {
			/*
			 * If fi_close returns -EBUSY, that means that the
			 * buffer is in use. In this case keep retry for a max
			 * time of 5 min to deregister buffer till fi_close
			 * returns success or some other error code.
			 */
			tmout = m0_time_from_now(300, 0);
			ret = -EBUSY;
			while (ret == -EBUSY && !m0_time_is_in_past(tmout)) {
				ret = fi_close(&fbp->fb_mr.bm_mr[i]->fid);
			}
			if (ret != 0) {
				M0_LOG(M0_ERROR,"mr[%d] close failed %d fb=%p",
				       i, ret, fbp);
				break;
			}
			fbp->fb_mr.bm_mr[i] = NULL;
		}
	}

	if (ret == 0) {
		fbp->fb_dp = NULL;
		fbp->fb_state = FAB_BUF_DEREGISTERED;
	}

	return M0_RC(ret);
}

/*============================================================================*/

/** 
 * Used as m0_net_xprt_ops::xo_dom_init(). 
 */
static int libfab_dom_init(const struct m0_net_xprt *xprt,
			   struct m0_net_domain *dom)
{
	struct m0_fab__ndom *fab_ndom;
	int                  ret = 0;

	M0_ENTRY();

	M0_ALLOC_PTR(fab_ndom);
	if (fab_ndom == NULL)
		return M0_ERR(-ENOMEM);

	ret = libfab_domain_params_get(fab_ndom);
	if (ret != 0)
		m0_free(fab_ndom);
	else {
		dom->nd_xprt_private = fab_ndom;
		fab_ndom->fnd_ndom = dom;
		m0_mutex_init(&fab_ndom->fnd_lock);
		fab_fabs_tlist_init(&fab_ndom->fnd_fabrics);
	}
	return M0_RC(ret);
}

/** 
 * Used as m0_net_xprt_ops::xo_dom_fini(). 
 */
static void libfab_dom_fini(struct m0_net_domain *dom)
{
	struct m0_fab__ndom *fnd;
	struct m0_fab__fab  *fab;
	int                  rc;
	
	M0_ENTRY();
	libfab_dom_invariant(dom);
	fnd = dom->nd_xprt_private;
	m0_tl_teardown(fab_fabs, &fnd->fnd_fabrics, fab) {
		if (fab->fab_dom != NULL) {
			rc = fi_close(&fab->fab_dom->fid);
			if (rc != 0)
				M0_LOG(M0_ERROR, "fab_dom fi_close ret=%d", rc);
			fab->fab_dom = NULL;
		}
		
		if (fab->fab_fab != NULL) {
			rc = fi_close(&fab->fab_fab->fid);
			if (rc != 0)
				M0_LOG(M0_ERROR, "fab_fabric fi_close ret=%d",
				       rc);
			fab->fab_fab = NULL;
		}

		if (fab->fab_fi != NULL) {
			fi_freeinfo(fab->fab_fi);
			fab->fab_fi = NULL;
		}

		m0_free(fab);
	}
	fab_fabs_tlist_fini(&fnd->fnd_fabrics);
	
	m0_mutex_fini(&fnd->fnd_lock);
	fnd->fnd_ndom = NULL;
	m0_free(fnd);
	dom->nd_xprt_private = NULL;

	M0_LEAVE();
}

/**
 * Used as m0_net_xprt_ops::xo_ma_fini().
 */
static void libfab_ma_fini(struct m0_net_transfer_mc *tm)
{
	struct m0_fab__tm *ma = tm->ntm_xprt_private;
	
	M0_ENTRY();
	libfab_tm_fini(tm);
	tm->ntm_xprt_private = NULL;

	fab_buf_tlist_fini(&ma->ftm_done);
	m0_free(ma);

	M0_LEAVE();
}

/**
 * Initialises transport-specific part of the transfer machine.
 *
 * Used as m0_net_xprt_ops::xo_tm_init().
 */
static int libfab_ma_init(struct m0_net_transfer_mc *ntm)
{
	struct m0_fab__tm *ftm;
	int                rc = 0;

	M0_ASSERT(ntm->ntm_xprt_private == NULL);
	M0_ALLOC_PTR(ftm);
	if (ftm != NULL) {
		ftm->ftm_epfd = -1;
		ftm->ftm_state = FAB_TM_INIT;
		ntm->ntm_xprt_private = ftm;
		ftm->ftm_ntm = ntm;
		fab_buf_tlist_init(&ftm->ftm_done);
		fab_bulk_tlist_init(&ftm->ftm_bulk);
		ftm->ftm_bufhash.bht_magic = M0_NET_LIBFAB_BUF_HT_HEAD_MAGIC;
		rc = fab_bufhash_htable_init(&ftm->ftm_bufhash.bht_hash,
					     ((M0_NET_QT_NR + 1) *
					      FAB_NUM_BUCKETS_PER_QTYPE));
	} else
		rc = M0_ERR(-ENOMEM);

	if (rc != 0 && ftm != NULL)
		libfab_ma_fini(ntm);
	return M0_RC(rc);
}

/**
 * Starts initialised ma.
 * Used as m0_net_xprt_ops::xo_tm_start().
 */
static int libfab_ma_start(struct m0_net_transfer_mc *ntm, const char *name)
{
	struct m0_fab__tm       *ftm = ntm->ntm_xprt_private;
	struct m0_fab__ndom     *fnd;
	struct m0_net_end_point *nep;
	int                      rc = 0;

	M0_ASSERT(libfab_tm_is_locked(ftm));
	M0_ALLOC_PTR(ftm->ftm_pep);
	if (ftm->ftm_pep != NULL) {
		fnd = ntm->ntm_dom->nd_xprt_private;
		libfab_ep_addr_decode(ftm->ftm_pep, name, fnd);

		ftm->ftm_fab = libfab_newfab_init(fnd);
		ftm->ftm_fab->fab_prov = FAB_FABRIC_PROV_MAX;
		rc = libfab_passive_ep_create(ftm->ftm_pep, ftm);
		if (rc != 0)
			return M0_ERR(rc);

		nep = &ftm->ftm_pep->fep_nep;
		nep->nep_tm = ntm;
		libfab_ep_pton(&ftm->ftm_pep->fep_name_p,
			       &ftm->ftm_pep->fep_name_n);
		m0_nep_tlink_init_at_tail(nep, &ntm->ntm_end_points);
		ftm->ftm_pep->fep_nep.nep_addr =
					ftm->ftm_pep->fep_name_p.fen_str_addr;

		m0_mutex_init(&ftm->ftm_endlock);
		m0_mutex_init(&ftm->ftm_evpost);

		rc = M0_THREAD_INIT(&ftm->ftm_poller, struct m0_fab__tm *, NULL,
				    &libfab_poller, ftm, "libfab_tm");
	} else
		return M0_ERR(-ENOMEM);

	ftm->ftm_state = FAB_TM_STARTED;
	ftm->ftm_tmout_check = m0_time_from_now(FAB_BUF_TMOUT_CHK_INTERVAL, 0);

	return M0_RC(rc);
}

/**
 * Stops a ma that has been started or is being started.
 *
 * Used as m0_net_xprt_ops::xo_tm_stop().
 */
static int libfab_ma_stop(struct m0_net_transfer_mc *net, bool cancel)
{
	struct m0_fab__tm *tm = net->ntm_xprt_private;

	M0_PRE(net->ntm_state == M0_NET_TM_STOPPING);

	if (cancel)
		m0_net__tm_cancel(net);
	
	libfab_tm_unlock(tm);
	libfab_tm_fini(net);
	libfab_tm_event_post(tm, M0_NET_TM_STOPPED);
	libfab_tm_lock(tm);

	return M0_RC(0);
}

/**
 * Used as m0_net_xprt_ops::xo_ma_confine().
 */
static int libfab_ma_confine(struct m0_net_transfer_mc *ma,
			     const struct m0_bitmap *processors)
{
	return M0_ERR(-ENOSYS);
}

/**
 * Returns an end-point with the given name.
 *
 * Used as m0_net_xprt_ops::xo_end_point_create().
 *
 * @see m0_net_end_point_create().
 */
static int libfab_end_point_create(struct m0_net_end_point **epp,
				   struct m0_net_transfer_mc *tm,
				   const char *name)
{
	M0_ENTRY("name=%s", name);
	return (libfab_ep_find(tm, name, NULL, epp));
}

/**
 * Deregister a network buffer.
 *
 * Used as m0_net_xprt_ops::xo_buf_deregister().
 *
 * @see m0_net_buffer_deregister().
 */
static void libfab_buf_deregister(struct m0_net_buffer *nb)
{
	struct m0_fab__buf  *fb = nb->nb_xprt_private;

	M0_ENTRY("fb=%p nb=%p q=%d", fb, nb, nb->nb_qtype);
	M0_PRE(nb->nb_flags == M0_NET_BUF_REGISTERED &&
	       libfab_buf_invariant(fb));

	libfab_buf_dom_dereg(fb);
	libfab_buf_fini(fb);
	m0_free(fb->fb_mr.bm_desc);
	m0_free(fb->fb_mr.bm_mr);
	m0_free(fb);
	nb->nb_xprt_private = NULL;
}

/**
 * Register a network buffer that can be used for 
 * send/recv and local/remote RMA
 * Used as m0_net_xprt_ops::xo_buf_register().
 *
 * @see m0_net_buffer_register().
 */
static int libfab_buf_register(struct m0_net_buffer *nb)
{
	struct m0_fab__buf  *fb;
	struct m0_fab__ndom *nd = nb->nb_dom->nd_xprt_private;

	M0_ENTRY("nb=%p q=%d", nb, nb->nb_qtype);

	M0_PRE(nb->nb_xprt_private == NULL);
	M0_PRE(nb->nb_dom != NULL);

	M0_ALLOC_PTR(fb);
	if (fb == NULL)
		return M0_ERR(-ENOMEM);

	M0_ALLOC_ARR(fb->fb_mr.bm_desc, nd->fnd_seg_nr);
	M0_ALLOC_ARR(fb->fb_mr.bm_mr, nd->fnd_seg_nr);

	if (fb->fb_mr.bm_desc == NULL || fb->fb_mr.bm_mr == NULL) {
		m0_free(fb->fb_mr.bm_desc);
		m0_free(fb->fb_mr.bm_mr);
		m0_free(fb);
		return M0_ERR(-ENOMEM);
	}

	fab_buf_tlink_init(fb);
	nb->nb_xprt_private = fb;
	fb->fb_nb = nb;
	fb->fb_state = FAB_BUF_INITIALIZED;

	return M0_RC(0);
}

/**
 * Adds a network buffer to a transfer machine queue.
 *
 * Used as m0_net_xprt_ops::xo_buf_add().
 *
 * @see m0_net_buffer_add().
 */
static int libfab_buf_add(struct m0_net_buffer *nb)
{
	struct m0_fab__buf       *fbp = nb->nb_xprt_private;
	struct m0_fab__tm        *ma  = libfab_buf_ma(nb);
	struct m0_fab__ep        *ep = NULL;
	struct m0_fab__active_ep *aep;
	struct iovec              iv;
	struct m0_fab__ep_name    epname = {};
	int                       ret = 0;

	M0_ENTRY("fb=%p nb=%p q=%d l=%"PRIu64, fbp, nb, nb->nb_qtype,
		 nb->nb_length);

	M0_PRE(libfab_tm_is_locked(ma) && libfab_tm_invariant(ma) &&
	       libfab_buf_invariant(fbp));
	M0_PRE(nb->nb_offset == 0); /* Do not support an offset during add. */
	M0_PRE((nb->nb_flags & M0_NET_BUF_RETAIN) == 0);

	fbp->fb_token = libfab_buf_token_get(ma, fbp);
	libfab_buf_dom_reg(nb, ma);
	fbp->fb_status = 0;

	switch (nb->nb_qtype) {
	case M0_NET_QT_MSG_RECV: {
		M0_ASSERT(nb->nb_buffer.ov_vec.v_nr == 1);
		fbp->fb_length = nb->nb_length;
		iv.iov_base = nb->nb_buffer.ov_buf[0];
		iv.iov_len = nb->nb_buffer.ov_vec.v_count[0];
		ret = fi_recvv(ma->ftm_rctx, &iv, fbp->fb_mr.bm_desc, 1, 0,
			       U32_TO_VPTR(fbp->fb_token));
		break;
	}
	
	case M0_NET_QT_MSG_SEND: {
		M0_ASSERT(nb->nb_length <= m0_vec_count(&nb->nb_buffer.ov_vec));
		M0_ASSERT(nb->nb_buffer.ov_vec.v_nr == 1);
		ret = libfab_fab_ep_find(ma, NULL, nb->nb_ep->nep_addr, &ep);
		if (ret != 0)
			break;
		aep = libfab_aep_get(ep);
		fbp->fb_txctx = ep;
		
		if (aep->aep_tx_state != FAB_CONNECTED)
			ret = libfab_conn_init(ep, ma, fbp);
		else {
			ret = libfab_txbuf_list_add(ma, fbp, aep);
			libfab_bufq_process(ma);
		}
		break;
	}

	/* For passive buffers, generate the buffer descriptor. */
	case M0_NET_QT_PASSIVE_BULK_RECV: {
		fbp->fb_length = nb->nb_length;
		if (!libfab_is_verbs(ma)) {
			ret = libfab_bdesc_encode(fbp);
			break;
		}
		/* else
			Intentional fall through */
	}

	case M0_NET_QT_PASSIVE_BULK_SEND: {
		if (m0_net_tm_tlist_is_empty(
				  &ma->ftm_ntm->ntm_q[M0_NET_QT_MSG_RECV]))
			ret = fi_recv(ma->ftm_rctx, fbp->fb_dummy,
				      sizeof(fbp->fb_dummy), NULL, 0,
				      U32_TO_VPTR(fbp->fb_token));
		
		if (ret == 0)
			ret = libfab_bdesc_encode(fbp);
		break;
	}

	/* For active buffers, decode the passive buffer descriptor */
	case M0_NET_QT_ACTIVE_BULK_RECV:
		fbp->fb_length = nb->nb_length;
		/* Intentional fall through */

	case M0_NET_QT_ACTIVE_BULK_SEND: {
		libfab_bdesc_decode(fbp, &epname);
		ret = libfab_fab_ep_find(ma, &epname, NULL, &ep);
		if (ret != 0)
			break;
		fbp->fb_txctx = ep;
		aep = libfab_aep_get(ep);
		if (aep->aep_tx_state != FAB_CONNECTED)
			ret = libfab_conn_init(ep, ma, fbp);
		else {
			ret = libfab_txbuf_list_add(ma, fbp, aep);
			libfab_bufq_process(ma);
		}
		break;
	}

	default:
		M0_IMPOSSIBLE("invalid queue type: %x", nb->nb_qtype);
		break;
	}

	if (ret == 0) {
		fbp->fb_state = FAB_BUF_QUEUED;
		m0_tlink_init(&fab_bufhash_tl, fbp);
		fab_bufhash_htable_add(&ma->ftm_bufhash.bht_hash, fbp);
	}

	return M0_RC(ret);
}

/**
 * Cancels a buffer operation.
 *
 * Used as m0_net_xprt_ops::xo_buf_del().
 *
 * @see m0_net_buffer_del().
 */
static void libfab_buf_del(struct m0_net_buffer *nb)
{
	struct m0_fab__buf *buf = nb->nb_xprt_private;
	struct m0_fab__tm  *ma = libfab_buf_ma(nb);

	M0_PRE(libfab_tm_is_locked(ma) && libfab_tm_invariant(ma) &&
	       libfab_buf_invariant(buf));
	nb->nb_flags |= M0_NET_BUF_CANCELLED;

	libfab_buf_dom_dereg(buf);
	buf->fb_state = FAB_BUF_CANCELED;
	libfab_buf_done(buf, -ECANCELED, false);
}

/**
 * Used as m0_net_xprt_ops::xo_bev_deliver_sync().
 *
 * @see m0_net_bev_deliver_sync().
 */
static int libfab_bev_deliver_sync(struct m0_net_transfer_mc *ma)
{
	return 0;
}

/**
 * Used as m0_net_xprt_ops::xo_bev_deliver_all().
 *
 * @see m0_net_bev_deliver_all().
 */
static void libfab_bev_deliver_all(struct m0_net_transfer_mc *ma)
{

}

/**
 * Used as m0_net_xprt_ops::xo_bev_pending().
 *
 * @see m0_net_bev_pending().
 */
static bool libfab_bev_pending(struct m0_net_transfer_mc *ma)
{
	return false;
}

/**
 * Used as m0_net_xprt_ops::xo_bev_notify().
 *
 * @see m0_net_bev_notify().
 */
static void libfab_bev_notify(struct m0_net_transfer_mc *ma,
			      struct m0_chan *chan)
{

}

/**
 * Maximal number of bytes in a buffer.
 *
 * Used as m0_net_xprt_ops::xo_get_max_buffer_size()
 *
 * @see m0_net_domain_get_max_buffer_size()
 */
static m0_bcount_t libfab_get_max_buf_size(const struct m0_net_domain *dom)
{
	struct m0_fab__ndom *nd = dom->nd_xprt_private;

	return (m0_bcount_t)(nd->fnd_seg_size * nd->fnd_seg_nr);
}

/**
 * Maximal number of bytes in a buffer segment.
 *
 * Used as m0_net_xprt_ops::xo_get_max_buffer_segment_size()
 *
 * @see m0_net_domain_get_max_buffer_segment_size()
 */
static m0_bcount_t libfab_get_max_buf_seg_size(const struct m0_net_domain *dom)
{
	return ((struct m0_fab__ndom *)dom->nd_xprt_private)->fnd_seg_size;
}

/**
 * Maximal number of segments in a buffer
 *
 * Used as m0_net_xprt_ops::xo_get_max_buffer_segments()
 *
 * @see m0_net_domain_get_max_buffer_segments()
 */
static int32_t libfab_get_max_buf_segments(const struct m0_net_domain *dom)
{
	return ((struct m0_fab__ndom *)dom->nd_xprt_private)->fnd_seg_nr;
}
/**
 * Maximal number of bytes in a buffer descriptor.
 *
 * Used as m0_net_xprt_ops::xo_get_max_buffer_desc_size()
 *
 * @see m0_net_domain_get_max_buffer_desc_size()
 */
static m0_bcount_t libfab_get_max_buf_desc_size(const struct m0_net_domain *dom)
{
	struct m0_fab__ndom *nd = dom->nd_xprt_private;
	m0_bcount_t          max_bd_size = sizeof(struct fi_rma_iov);

	max_bd_size = (max_bd_size * nd->fnd_seg_nr) +
		      sizeof(struct m0_fab__bdesc);

	return max_bd_size;
}

/**
 * Maximal segment size for rpc buffer.
 *
 * Used as m0_net_xprt_ops::xo_rpc_max_seg_size()
 *
 * @see m0_net_domain_rpc_max_seg_size()
 */
static m0_bcount_t libfab_rpc_max_seg_size(struct m0_net_domain *ndom)
{
	M0_PRE(ndom != NULL);
	return FAB_MAX_RPC_SEG_SIZE;
}

/**
 * Maximal segment count for rpc buffer.
 *
 * Used as m0_net_xprt_ops::xo_rpc_max_segs_nr()
 *
 * @see m0_net_domain_rpc_max_segs_nr()
 */
static uint32_t libfab_rpc_max_segs_nr(struct m0_net_domain *ndom)
{
	M0_PRE(ndom != NULL);
	return FAB_MAX_RPC_SEG_NR;
}

/**
 * Maximal message size for rpc buffer.
 *
 * Used as m0_net_xprt_ops::xo_rpc_max_msg_size()
 *
 * @see m0_net_domain_rpc_max_msg_size()
 */
static m0_bcount_t libfab_rpc_max_msg_size(struct m0_net_domain *ndom,
					   m0_bcount_t rpc_size)
{
	m0_bcount_t mbs;
	M0_PRE(ndom != NULL);

	mbs = libfab_rpc_max_seg_size(ndom) * libfab_rpc_max_segs_nr(ndom);
	return rpc_size != 0 ? m0_clip64u(M0_SEG_SIZE, mbs, rpc_size) : mbs;
}

/**
 * Maximal number of receive messages in a single rpc buffer.
 *
 * Used as m0_net_xprt_ops::xo_rpc_max_recv_msgs()
 *
 * @see m0_net_domain_rpc_max_recv_msgs()
 */
static uint32_t libfab_rpc_max_recv_msgs(struct m0_net_domain *ndom,
					 m0_bcount_t rpc_size)
{
	M0_PRE(ndom != NULL);
	return FAB_MAX_RPC_RECV_MSG_NR;
}

static const struct m0_net_xprt_ops libfab_xprt_ops = {
	.xo_dom_init                    = &libfab_dom_init,
	.xo_dom_fini                    = &libfab_dom_fini,
	.xo_tm_init                     = &libfab_ma_init,
	.xo_tm_confine                  = &libfab_ma_confine,
	.xo_tm_start                    = &libfab_ma_start,
	.xo_tm_stop                     = &libfab_ma_stop,
	.xo_tm_fini                     = &libfab_ma_fini,
	.xo_end_point_create            = &libfab_end_point_create,
	.xo_buf_register                = &libfab_buf_register,
	.xo_buf_deregister              = &libfab_buf_deregister,
	.xo_buf_add                     = &libfab_buf_add,
	.xo_buf_del                     = &libfab_buf_del,
	.xo_bev_deliver_sync            = &libfab_bev_deliver_sync,
	.xo_bev_deliver_all             = &libfab_bev_deliver_all,
	.xo_bev_pending                 = &libfab_bev_pending,
	.xo_bev_notify                  = &libfab_bev_notify,
	.xo_get_max_buffer_size         = &libfab_get_max_buf_size,
	.xo_get_max_buffer_segment_size = &libfab_get_max_buf_seg_size,
	.xo_get_max_buffer_segments     = &libfab_get_max_buf_segments,
	.xo_get_max_buffer_desc_size    = &libfab_get_max_buf_desc_size,
	.xo_rpc_max_seg_size            = &libfab_rpc_max_seg_size,
	.xo_rpc_max_segs_nr             = &libfab_rpc_max_segs_nr,
	.xo_rpc_max_msg_size            = &libfab_rpc_max_msg_size,
	.xo_rpc_max_recv_msgs           = &libfab_rpc_max_recv_msgs,
};

struct m0_net_xprt m0_net_libfab_xprt = {
	.nx_name = "libfab",
	.nx_ops  = &libfab_xprt_ops
};
M0_EXPORTED(m0_net_libfab_xprt);

#else /* ENABLE_LIBFAB */

/* libfab init and fini() : initialized in motr init */
M0_INTERNAL int m0_net_libfab_init(void)
{
	return M0_RC(0);
}

M0_INTERNAL void m0_net_libfab_fini(void)
{
	
}

#endif /* ENABLE_LIBFAB */

#undef M0_TRACE_SUBSYSTEM

/** @} end of netlibfab group */

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
