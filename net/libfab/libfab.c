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
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_NET
#include "lib/trace.h"          /* M0_ENTRY() */
#include <netinet/in.h>         /* INET_ADDRSTRLEN */
#include <arpa/inet.h>          /* inet_pton, htons */
#include <sched.h>              /* sched_yield */
#include <stdlib.h>             /* atoi */
#include <sys/epoll.h>          /* struct epoll_event */
#include <unistd.h>             /* close */
#include "net/buffer_pool.h"    /* struct m0_net_buffer_pool */
#include "net/net.h"            /* struct m0_net_domain */
#include "lib/errno.h"          /* errno */
#include "lib/hash.h"           /* m0_htable */
#include "lib/memory.h"         /* M0_ALLOC_PTR()*/
#include "libfab_internal.h"
#include "net/net_internal.h"   /* m0_net__buffer_invariant() */

static char     *providers[] = { "verbs", "tcp", "sockets" };
static char     *protf[]  = { "unix", "inet", "inet6" };
static char     *socktype[] = { "stream", "dgram" };
static uint64_t  mr_key_idx = 0;
static uint32_t  buf_token = 0;
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
	return (*(uint32_t *)key & 0xFF);
}

static bool libfab_bht_key_eq(const void *key1, const void *key2)
{
	return ((*(const uint32_t *)key1) == (*(const uint32_t *)key2));
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
			  struct m0_fab__ep_name *epname);
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
static void libfab_buf_complete(struct m0_fab__buf *buf, int32_t status);
static void libfab_buf_done(struct m0_fab__buf *buf, int rc);
static inline struct m0_fab__tm *libfab_buf_tm(struct m0_fab__buf *buf);
static inline struct m0_fab__tm *libfab_buf_ma(struct m0_net_buffer *buf);
static bool libfab_tm_invariant(const struct m0_fab__tm *tm);
static void libfab_buf_del(struct m0_net_buffer *nb);
static inline void libfab_ep_get(struct m0_fab__ep *ep);
static void libfab_ep_release(struct m0_ref *ref);
static uint64_t libfab_mr_keygen(void);
static int libfab_check_for_event(struct fid_eq *eq, uint32_t *ev);
static int libfab_check_for_comp(struct fid_cq *cq, uint32_t *ctx,
				 m0_bindex_t *len, uint64_t *rem_cq_data);
static void libfab_tm_fini(struct m0_net_transfer_mc *tm);
static int libfab_buf_dom_reg(struct m0_net_buffer *nb, struct fid_domain *dp);
static uint64_t libfab_destaddr_get(struct m0_fab__ep_name *epname);
static void libfab_pending_bufs_send(struct m0_fab__ep *ep);
static int libfab_target_notify(struct m0_fab__buf *buf,
				struct m0_fab__active_ep *ep);
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
static int libfab_bulk_op(struct m0_fab__active_ep *ep, struct m0_fab__buf *fb);
static inline bool libfab_is_verbs(struct m0_fab__tm *tm);
static int libfab_bulklist_add(struct m0_fab__tm *tm, struct m0_fab__buf *fb,
				struct m0_fab__active_ep *aep);
static uint32_t libfab_wr_cnt_get(struct m0_fab__buf *fb);
static void libfab_bulk_buf_process(struct m0_fab__tm *tm);

/* libfab init and fini() : initialized in motr init */
M0_INTERNAL int m0_net_libfab_init(void)
{
	m0_net_xprt_register(&m0_net_libfab_xprt);
	m0_net_xprt_default_set(&m0_net_libfab_xprt);
	return M0_RC(0);
}

M0_INTERNAL void m0_net_libfab_fini(void)
{
	m0_net_xprt_deregister(&m0_net_libfab_xprt);
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
		M0_PRE(nodeSize >= ((strlen(fnd->fnd_loc_ip)+1)) );
		memcpy(node, fnd->fnd_loc_ip, (strlen(fnd->fnd_loc_ip)+1));
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
	if (tmid >= 1024 || (portal - 30) >= 32)
		return M0_ERR_INFO(-EPROTO,
			"portal: %u, tmid: %u", portal, tmid);
	*/

	if ( portal < 30)
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
				      size_t portSize)
{
	int   shift = 0;
	int   f;
	int   s;
	char *at;

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
	M0_ASSERT(nodeSize >= (at - ep_name));
	memcpy(node, ep_name, ((at - ep_name)-1));
	return 0;
}

/**
 * Used to decode the ip and port from the given end point
 * ep_name : endpoint address from domain
 * node    : copy ip address from ep_name
 * port    : copy port number from ep_name
 * Example of ep_name IPV4 192.168.0.1:4235
 *                    IPV6 [4002:db1::1]:4235
 */
static int libfab_ep_addr_decode_native(const char *ep_name, char *node,
					size_t nodeSize, char *port,
					size_t portSize)
{
	char   *name;
	char   *cp;
	size_t  n;
	int     rc = 0;

	M0_PRE(ep_name != NULL);

	M0_ENTRY("ep_name=%s", ep_name);

	name = (char *)ep_name + strlen("libfab:");

	if( name[0] == '[' ) {
		/* IPV6 pattern */
		cp = strchr(name, ']');
		if (cp == NULL)
			return M0_ERR(-EINVAL);

		name++;
		n = cp - name;
		if (n == 0 )
			return M0_ERR(-EINVAL);
		cp++;
		if (*cp != ':')
		return M0_ERR(-EINVAL);
		cp++;
	} else {
		/* IPV4 pattern */
		cp = strchr(name, ':');
		if (cp == NULL)
			return M0_ERR(-EINVAL);

		n = cp - name;
		if (n == 0 )
			return M0_ERR(-EINVAL);

		++cp;
	}

	if ((nodeSize < (n+1)) || (portSize < (strlen(cp)+1)))
		return M0_ERR(-EINVAL);

	memcpy(node, name, n);
	node[n] = '\0';

	n=strlen(cp);
	memcpy(port, cp, n);
	port[n] = '\0';

	return M0_RC(rc);
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
 *     - libfab compatible format
 *       for example IPV4 libfab:192.168.0.1:4235
 *                   IPV6 libfab:[4002:db1::1]:4235
 *
 */
static int libfab_ep_addr_decode(struct m0_fab__ep *ep, const char *name,
				 struct m0_fab__ndom *fnd)
{
	char *node = ep->fep_name.fen_addr;
	char *port = ep->fep_name.fen_port;
	size_t nodeSize = ARRAY_SIZE(ep->fep_name.fen_addr);
	size_t portSize = ARRAY_SIZE(ep->fep_name.fen_port);
	int result;

	M0_ENTRY("name=%s", name);
	
	if( name == NULL || name[0] == 0)
		result =  M0_ERR(-EPROTO);
	else if ((strncmp(name, "libfab", 6)) == 0)
		result = libfab_ep_addr_decode_native(name, node, nodeSize, 
						      port, portSize);
	else if (name[0] < '0' || name[0] > '9')
		/* sock format */
		result = libfab_ep_addr_decode_sock(name, node, nodeSize, 
						    port, portSize);
	else
		/* Lnet format. */
		result = libfab_ep_addr_decode_lnet(name, node, nodeSize, 
						    port, portSize, fnd);

	if (result == FI_SUCCESS)
		strcpy(ep->fep_name.fen_str_addr, name);

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
	int                        i;
	m0_time_t                  now = m0_time_now();

	M0_PRE(libfab_tm_is_locked(ftm));
	M0_PRE(libfab_tm_invariant(ftm));

	ftm->ftm_tmout_check = now;
	for (i = 0; i < ARRAY_SIZE(net->ntm_q); ++i) {
		struct m0_net_buffer *nb;

		m0_tl_for(m0_net_tm, &net->ntm_q[i], nb) {
			if (nb->nb_timeout < now) {
				nb->nb_flags |= M0_NET_BUF_TIMED_OUT;
				libfab_buf_done(nb->nb_xprt_private,
						-ETIMEDOUT);
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
	m0_tl_for(fab_buf, &ftm->ftm_done, buffer) {
		fab_buf_tlist_del(buffer);
		libfab_buf_complete(buffer, buffer->fb_status);
		nr++;
	} m0_tl_endfor;

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
	if (cd->fcd_tmid == 0xFFFF)
		sprintf(buf, "%s@%s:12345:%d:*",
			cd->fcd_iface == FAB_LO ? "0" : en->fen_addr,
			cd->fcd_iface == FAB_LO ? "lo" : 
				((cd->fcd_iface == FAB_TCP) ? "tcp" : "o2ib"),
			cd->fcd_portal);
	else
		sprintf(buf, "%s@%s:12345:%d:%d",
			cd->fcd_iface == FAB_LO ? "0" : en->fen_addr,
			cd->fcd_iface == FAB_LO ? "lo" : 
			((cd->fcd_iface == FAB_TCP) ? "tcp" : "o2ib"),
			cd->fcd_portal, cd->fcd_tmid);

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
	struct m0_fab__ep_name    en;
	struct fid_eq            *eq;
	struct fi_eq_err_entry    eq_err;
	struct fi_eq_cm_entry     entry;
	uint32_t                  event;
	int                       rc;
	char                      straddr[LIBFAB_ADDR_STRLEN_MAX] = { '\0' };

	eq = tm->ftm_pep->fep_listen->pep_res.fpr_eq;
	rc = fi_eq_read(eq, &event, &entry,
			(sizeof(entry) + sizeof(struct m0_fab__conn_data)), 0);
	if (rc >= (int)sizeof(entry) && event == FI_CONNREQ) {
		memset(&en, 0, sizeof(en));
		cd = (struct m0_fab__conn_data*)entry.data;
		libfab_straddr_gen(cd, straddr, sizeof(straddr), &en);
		rc = libfab_fab_ep_find(tm, &en, straddr, &ep);
		if (rc == FI_SUCCESS) {
			rc = libfab_conn_accept(ep, tm, entry.info);
			if (rc != FI_SUCCESS)
				M0_LOG(M0_ERROR, "Conn accept failed %d", rc);
		}
		fi_freeinfo(entry.info);
	} else if (rc == -FI_EAVAIL) {
		memset(&eq_err, 0, sizeof(eq_err));
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
	}
	/* For all other events, there is no error info available.
	   Hence, all such events can be ignored.
	*/
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
		m0_tl_for(fab_sndbuf, &txep->fep_sndbuf, fbp) {
			fab_sndbuf_tlist_del(fbp);
			libfab_buf_done(fbp, rc);
		} m0_tl_endfor;
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

	memset(&len, 0, sizeof(len));
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
				libfab_buf_done(fb, 0);
			}
			if (data[i] != 0) {
				rem_token = (uint32_t)data[i];
				fb = fab_bufhash_htable_lookup(
					&tm->ftm_bufhash.bht_hash,
					&rem_token);
				if (fb != NULL)
					libfab_buf_done(fb, 0);
			}
		}
	}
}

/**
 * Check for completion events on the completion queue for the transmit endpoint
 */
static void libfab_txep_comp_read(struct fid_cq *cq,
				  struct m0_fab__tm *tm)
{
	struct m0_fab__active_ep *aep;
	struct m0_fab__buf       *fb = NULL;
	uint32_t                  token[FAB_MAX_COMP_READ];
	int                       i;
	int                       cnt;

	cnt = libfab_check_for_comp(cq, token, NULL, NULL);
	for (i = 0; i < cnt; i++) {
		if ( token[i] != 0)
			fb = fab_bufhash_htable_lookup(
				&tm->ftm_bufhash.bht_hash,
				&token[i]);
		if (fb != NULL) {
			aep = libfab_aep_get(fb->fb_txctx);
			if (M0_IN(fb->fb_nb->nb_qtype,
				  (M0_NET_QT_ACTIVE_BULK_RECV,
				   M0_NET_QT_ACTIVE_BULK_SEND)))
				aep->aep_bulk_cnt -= fb->fb_wr_cnt;

			fb->fb_wr_comp_cnt = fb->fb_wr_cnt + 1;
			if (fb->fb_wr_comp_cnt >= fb->fb_wr_cnt) {
				libfab_target_notify(fb, aep);
				libfab_buf_done(fb, 0);
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

	/* Wait for the transfer machine to get started */
	while (tm->ftm_state != FAB_TM_STARTED)
		/* No-op */;
	
	while (tm->ftm_state != FAB_TM_SHUTDOWN) {
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
				/* Got tm lock. 
				   Let's continue processing events. */
				break;
			}
		}
		
		M0_ASSERT(libfab_tm_is_locked(tm) && libfab_tm_invariant(tm));

		if (ev_cnt > 0) {
			ctx = ev.data.ptr;
			if (ctx->evctx_type == FAB_COMMON_Q_EVENT) {
				/* Check the common queue of the
				   transfer machine for events */
				libfab_handle_connect_request_events(tm);
				libfab_txep_comp_read(tm->ftm_tx_cq, tm);
			} else {
				/* Check the private queue of the
				   endpoint for events */
				xep = ctx->evctx_ep;
				aep = libfab_aep_get(xep);
				libfab_txep_event_check(xep, aep, tm);
				cq = aep->aep_rx_res.frr_cq;
				libfab_rxep_comp_read(cq, xep, tm);
			}
		}

		libfab_bulk_buf_process(tm);
		if (m0_time_sub(m0_time_now(), tm->ftm_tmout_check) >=
								 m0_time(1,0))
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
			  struct m0_fab__ep_name *epname)
{
	bool ret = false;
	M0_PRE(ep != NULL);
	if (name == NULL) {
		M0_ASSERT(epname != NULL);
		if ((strcmp(ep->fep_name.fen_addr, epname->fen_addr) == 0) &&
		    (strcmp(ep->fep_name.fen_port, epname->fen_port) == 0))
			ret = true;
	} else {
		if (strcmp(ep->fep_name.fen_str_addr, name) == 0)
			ret = true;
	}
	return ret;
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
	struct m0_fab__active_ep *aep;
	struct m0_fab__tm        *ma;
	char                      ep_str[LIBFAB_ADDR_STRLEN_MAX + 9] = {'\0'};
	char                     *wc = NULL;
	int                       rc = 0;

	M0_ASSERT(libfab_tm_is_locked(tm->ntm_xprt_private));
	net = m0_tl_find(m0_nep, net, &tm->ntm_end_points,
			 libfab_ep_cmp(libfab_ep(net), name, epn));

	if (net == NULL) {
		if (name != NULL)
			rc = libfab_ep_create(tm, name, epn, epp);
		else {
			M0_ASSERT(epn != NULL);
			M0_ASSERT((strlen(epn->fen_addr) + strlen(epn->fen_port)
				  + 8) < LIBFAB_ADDR_STRLEN_MAX);
			sprintf(ep_str, "libfab:%s:%s", epn->fen_addr,
				epn->fen_port);
			rc = libfab_ep_create(tm, ep_str, epn, epp);
		}
	} else {
		ep = libfab_ep(net);
		*epp = &ep->fep_nep;
		if (name != NULL && epn != NULL) {
			wc = strchr(name,'*');
			if (wc != NULL && 
			    strcmp(ep->fep_name.fen_port, epn->fen_port) != 0) {
				strcpy(ep->fep_name.fen_addr, epn->fen_addr);
				strcpy(ep->fep_name.fen_port, epn->fen_port);
				aep = libfab_aep_get(ep);
				ma = tm->ntm_xprt_private;
				if (aep->aep_tx_state == FAB_CONNECTED)
					rc = libfab_txep_init(aep, ma, ep);
			}
		}
		if (rc == FI_SUCCESS)
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
	if (rc != FI_SUCCESS) {
		libfab_aep_param_free(ep->fep_aep, ma);
		ep->fep_aep = NULL;
		m0_free(ep);
		return M0_RC(rc);
	}

	wc = strchr(name, '*');
	if (epn != NULL && wc != NULL) {
		strcpy(ep->fep_name.fen_addr, epn->fen_addr);
		strcpy(ep->fep_name.fen_port, epn->fen_port);
	}

	rc = libfab_active_ep_create(ep, ma);
	if (rc != FI_SUCCESS) {
		libfab_aep_param_free(ep->fep_aep, ma);
		ep->fep_aep = NULL;
		m0_free(ep);
		return M0_RC(rc);
	}

	fab_sndbuf_tlist_init(&ep->fep_sndbuf);
	*epp = &ep->fep_nep;
	return M0_RC(rc);
}

/**
 * Init resources for a transfer machine
 */
static int libfab_tm_res_init(struct m0_fab__tm *tm)
{
	struct m0_fab__fab        *fab;
	struct m0_fab__passive_ep *pep;
	struct fi_cq_attr          cq_attr = { 0 };
	int                        rc = 0;
	
	M0_PRE(tm != NULL);

	pep = tm->ftm_pep->fep_listen;
	fab = tm->ftm_fab;
	/* Initialise completion queues for tx */
	cq_attr.wait_obj = FI_WAIT_FD;
	cq_attr.format = FI_CQ_FORMAT_DATA;
	cq_attr.size = FAB_MAX_TX_CQ_EV;
	rc = fi_cq_open(fab->fab_dom, &cq_attr, &tm->ftm_tx_cq, NULL);
	if (rc != FI_SUCCESS)
		return M0_ERR(rc);

	/* Initialize and bind resources to tx ep */
	tm->ftm_txcq_ctx.evctx_type = FAB_COMMON_Q_EVENT;
	tm->ftm_txcq_ctx.evctx_ep = NULL;
	rc = libfab_waitfd_bind(&tm->ftm_tx_cq->fid, tm, &tm->ftm_txcq_ctx);
	if (rc != FI_SUCCESS)
		return M0_ERR(rc);

	rc = libfab_txep_init(pep->pep_aep, tm, tm->ftm_pep);

	return M0_RC(rc);
}

/**
 * Initialize transmit endpoint resources and associate
 * it to the active transmit endpoint.
 */
static int libfab_ep_txres_init(struct m0_fab__active_ep *aep,
				struct m0_fab__tm *tm, void *ctx)
{
	struct fi_eq_attr   eq_attr = { 0 };
	struct m0_fab__fab *fab;
	int                 rc = 0;

	fab = tm->ftm_fab;

	/* Bind the ep to tx completion queue */
	rc = fi_ep_bind(aep->aep_txep, &tm->ftm_tx_cq->fid,
			(FI_TRANSMIT | FI_RECV | FI_SELECTIVE_COMPLETION));
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	/* Initialise and bind event queue */
	eq_attr.wait_obj = FI_WAIT_FD;
	eq_attr.size = FAB_MAX_AEP_EQ_EV;
	rc = fi_eq_open(fab->fab_fab, &eq_attr, &aep->aep_tx_res.ftr_eq, NULL);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	aep->aep_tx_res.ftr_ctx.evctx_type = FAB_PRIVATE_Q_EVENT;
	aep->aep_tx_res.ftr_ctx.evctx_ep = ctx;
	rc = libfab_waitfd_bind(&aep->aep_tx_res.ftr_eq->fid, tm,
				&aep->aep_tx_res.ftr_ctx);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	rc = fi_ep_bind(aep->aep_txep, &aep->aep_tx_res.ftr_eq->fid, 0);

	return M0_RC(rc);
}

/**
 * Initialize receive endpoint resources and associate
 * it to the active receive endpoint.
 */
static int libfab_ep_rxres_init(struct m0_fab__active_ep *aep,
				struct m0_fab__tm *tm, void *ctx)
{
	struct fi_cq_attr   cq_attr = { 0 };
	struct fi_eq_attr   eq_attr = { 0 };
	struct m0_fab__fab *fab;
	int                 rc = 0;

	fab = tm->ftm_fab;

	/* Initialise and bind completion queues for rx */
	cq_attr.wait_obj = FI_WAIT_FD;
	cq_attr.wait_cond = FI_CQ_COND_NONE;
	cq_attr.format = FI_CQ_FORMAT_DATA;
	cq_attr.size = FAB_MAX_RX_CQ_EV;
	rc = fi_cq_open(fab->fab_dom, &cq_attr, &aep->aep_rx_res.frr_cq, NULL);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	aep->aep_rx_res.frr_ctx.evctx_type = FAB_PRIVATE_Q_EVENT;
	aep->aep_rx_res.frr_ctx.evctx_ep = ctx;
	rc = libfab_waitfd_bind(&aep->aep_rx_res.frr_cq->fid, tm,
				&aep->aep_rx_res.frr_ctx);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);
	
	rc = fi_ep_bind(aep->aep_rxep, &tm->ftm_tx_cq->fid,
			FI_TRANSMIT | FI_SELECTIVE_COMPLETION);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);
	rc = fi_ep_bind(aep->aep_rxep, &aep->aep_rx_res.frr_cq->fid, FI_RECV);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	/* Initialise and bind event queue */
	eq_attr.wait_obj = FI_WAIT_FD;
	eq_attr.size = FAB_MAX_AEP_EQ_EV;
	rc = fi_eq_open(fab->fab_fab, &eq_attr, &aep->aep_rx_res.frr_eq, NULL);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	rc = libfab_waitfd_bind(&aep->aep_rx_res.frr_eq->fid, tm,
				&aep->aep_rx_res.frr_ctx);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	rc = fi_ep_bind(aep->aep_rxep, &aep->aep_rx_res.frr_eq->fid, 0);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	/* Bind shared recv context */
	rc = fi_ep_bind(aep->aep_rxep, &tm->ftm_rctx->fid, 0);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	return M0_RC(rc);
}

/**
 * Initialize passive endpoint resources and associate
 * it to the passive endpoint.
 */
static int libfab_pep_res_init(struct m0_fab__passive_ep *pep,
			       struct m0_fab__tm *tm, void *ctx)
{
	struct fi_eq_attr eq_attr = { 0 };
	int               rc = 0;

	/* Initialise and bind event queue */
	eq_attr.wait_obj = FI_WAIT_FD;
	eq_attr.size = FAB_MAX_PEP_EQ_EV;
	rc = fi_eq_open(tm->ftm_fab->fab_fab, &eq_attr, &pep->pep_res.fpr_eq,
			NULL);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);
	
	pep->pep_res.fpr_ctx.evctx_type = FAB_COMMON_Q_EVENT;
	pep->pep_res.fpr_ctx.evctx_ep = ctx;
	rc = libfab_waitfd_bind(&pep->pep_res.fpr_eq->fid, tm,
				&pep->pep_res.fpr_ctx);
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	rc = fi_pep_bind(pep->pep_pep, &pep->pep_res.fpr_eq->fid, 0);

	return M0_RC(rc);
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

	M0_ENTRY("from ep=%s -> tm = %s", (char*)ep->fep_name.fen_str_addr,
		 (char*)tm->ftm_pep->fep_name.fen_str_addr);

	aep = libfab_aep_get(ep);
	dp = tm->ftm_fab->fab_dom;
	
	if (aep->aep_rxep != NULL) {
		rc = fi_close(&aep->aep_rxep->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "ep close = %d",rc);
		libfab_ep_rxres_free(&aep->aep_rx_res, tm);
	}
	aep->aep_rx_state = FAB_NOT_CONNECTED;
	ep->fep_connlink = FAB_CONNLINK_DOWN;


	rc = fi_endpoint(dp, info, &aep->aep_rxep, NULL);
	if (rc != FI_SUCCESS) {
		libfab_aep_param_free(aep, tm);
		aep = NULL;
		return M0_RC(rc);
	}

	rc = libfab_ep_rxres_init(aep, tm, ep);
	if (rc != FI_SUCCESS) {
		libfab_aep_param_free(aep, tm);
		aep = NULL;
		return M0_RC(rc);
	}

	rc = fi_enable(aep->aep_rxep);
	if (rc != FI_SUCCESS) {
		libfab_aep_param_free(aep, tm);
		aep = NULL;
		return M0_RC(rc);
	}

	/* Accept incoming request */
	rc = fi_accept(aep->aep_rxep, NULL, 0);
	if (rc != FI_SUCCESS) {
		libfab_aep_param_free(aep, tm);
		aep = NULL;
		return M0_RC(rc);
	}

	aep->aep_rx_state = FAB_CONNECTING;

	return M0_RC(rc);
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
	if (rc != FI_SUCCESS) {
		libfab_aep_param_free(aep, tm);
		aep = NULL;
		return M0_RC(rc);
	}

	net = &ep->fep_nep;
	net->nep_tm = tm->ftm_ntm;
	m0_nep_tlink_init_at_tail(net, &tm->ftm_ntm->ntm_end_points);
	net->nep_addr = (const char *)(&ep->fep_name.fen_str_addr);
	m0_ref_init(&ep->fep_nep.nep_ref, 1, &libfab_ep_release);
	
	return M0_RC(rc);
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
	int                        i;
	int                        rc;
	char                      *addr = NULL;
	char                      *port = NULL;

	M0_ENTRY("ep=%s addr=%s port=%s", (char*)ep->fep_name.fen_str_addr,
		 (char*)ep->fep_name.fen_addr, (char*)ep->fep_name.fen_port);

	M0_ALLOC_PTR(ep->fep_listen);
	if (ep->fep_listen == NULL)
		return M0_RC(-ENOMEM);
	M0_ALLOC_PTR(ep->fep_listen->pep_aep);
	if (ep->fep_listen->pep_aep == NULL) {
		m0_free(ep->fep_listen);
		return M0_RC(-ENOMEM);
	}

	pep = ep->fep_listen;
	ep->fep_listen->pep_aep->aep_rxep = NULL;
	ep->fep_listen->pep_aep->aep_txep = NULL;

	if (strlen(ep->fep_name.fen_port) != 0) {
		addr = ep->fep_name.fen_addr;
		port = ep->fep_name.fen_port;
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

	for (i = 0; i < ARRAY_SIZE(providers); i++) {
		hints->fabric_attr->prov_name = providers[i];
		rc = fi_getinfo(LIBFAB_VERSION, addr, port, FI_SOURCE, hints,
				&fi);
		if (rc == FI_SUCCESS)
			break;
	}
	M0_ASSERT(i < ARRAY_SIZE(providers));

	M0_LOG(M0_DEBUG, "tm = %s Provider selected %s",
	       (char*)ep->fep_name.fen_str_addr, fi->fabric_attr->prov_name);
	hints->fabric_attr->prov_name = NULL;
	tm->ftm_fab->fab_fi = fi;
	fi_freeinfo(hints);
	
	rc = fi_fabric(tm->ftm_fab->fab_fi->fabric_attr, &tm->ftm_fab->fab_fab,
		       NULL);
	if (rc != FI_SUCCESS) {
		libfab_pep_param_free(pep, tm);
		pep = NULL;
		return M0_RC(rc);
	}

	rc = libfab_waitfd_init(tm);
	if (rc != FI_SUCCESS) {
		libfab_pep_param_free(pep, tm);
		pep = NULL;
		return M0_RC(rc);
	}

	rc = fi_passive_ep(tm->ftm_fab->fab_fab, tm->ftm_fab->fab_fi,
			   &pep->pep_pep, NULL);
	if (rc != FI_SUCCESS) {
		libfab_pep_param_free(pep, tm);
		pep = NULL;
		return M0_RC(rc);
	}

	rc = libfab_pep_res_init(pep, tm, ep);
	if (rc != FI_SUCCESS) {
		libfab_pep_param_free(pep, tm);
		pep = NULL;
		return M0_RC(rc);
	}

	rc = fi_listen(pep->pep_pep);
	if (rc != FI_SUCCESS) {
		libfab_pep_param_free(pep, tm);
		pep = NULL;
		return M0_RC(rc);
	}

	rc = fi_domain(tm->ftm_fab->fab_fab, tm->ftm_fab->fab_fi,
		       &tm->ftm_fab->fab_dom, NULL);
	if (rc != FI_SUCCESS) {
		M0_LOG(M0_ERROR," \n fi_domain = %d \n ", rc);
		libfab_pep_param_free(pep, tm);
		pep = NULL;
		return M0_RC(rc);
	}

	rc = fi_srx_context(tm->ftm_fab->fab_dom, tm->ftm_fab->fab_fi->rx_attr,
			    &tm->ftm_rctx, NULL);
	if (rc != FI_SUCCESS) {
		M0_LOG(M0_ERROR," \n fi_srx_context = %d \n ", rc);
		libfab_pep_param_free(pep, tm);
		pep = NULL;
		return M0_RC(rc);
	}

	rc = libfab_tm_res_init(tm);
	if(rc != FI_SUCCESS){
		M0_LOG(M0_ERROR," \n libfab_tm_res_init = %d \n ", rc);
		libfab_pep_param_free(pep, tm);
		pep = NULL;
		return M0_RC(rc);
	}

	fab_sndbuf_tlist_init(&ep->fep_sndbuf);

	return M0_RC(rc);
}

/**
 * Used to free the resources attached to an passive endpoint
 */
static int libfab_pep_res_free(struct m0_fab__pep_res *pep_res,
			       struct m0_fab__tm *tm)
{
	int rc = 0;

	if (pep_res->fpr_eq != NULL) {
		rc = fi_close(&(pep_res->fpr_eq)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fpr_eq fi_close ret=%d fid=%d",
			       rc, (int)(pep_res->fpr_eq)->fid.fclass);
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
		rc = fi_close(&(tx_res->ftr_eq)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "ftr_eq fi_close ret=%d fid=%d",
			       rc, (int)(tx_res->ftr_eq)->fid.fclass);
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
		rc = fi_close(&(rx_res->frr_eq)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "frr_eq fi_close ret=%d fid=%d",
			       rc, (int)(rx_res->frr_eq)->fid.fclass);
		rx_res->frr_eq = NULL;
	}

	if (rx_res->frr_cq != NULL) {
		rc = fi_close(&(rx_res->frr_cq)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "frr_cq fi_close ret=%d fid=%d",
			       rc, (int)(rx_res->frr_cq)->fid.fclass);
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
		rc = fi_close(&(aep->aep_txep)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "aep_txep fi_close ret=%d fid=%d",
			       rc, (int)(aep->aep_txep)->fid.fclass);
		aep->aep_txep = NULL;
	}

	if (aep->aep_rxep != NULL) {
		rc = fi_close(&(aep->aep_rxep)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "aep_rxep fi_close ret=%d fid=%d",
			       rc, (int)(aep->aep_rxep)->fid.fclass);
		aep->aep_rxep = NULL;
	}

	rc = libfab_ep_txres_free(&aep->aep_tx_res, tm);
	if (rc != FI_SUCCESS)
		M0_LOG(M0_ERROR, "ep_txres_free failed %d", rc);
	
	rc = libfab_ep_rxres_free(&aep->aep_rx_res, tm);
	if (rc != FI_SUCCESS)
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
		rc = fi_close(&(pep->pep_pep)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "fep_pep fi_close ret=%d fid=%d",
			       rc, (int)(pep->pep_pep)->fid.fclass);
		pep->pep_pep = NULL;
	}
	
	rc = libfab_aep_param_free(pep->pep_aep, tm);
	pep->pep_aep = NULL;
	if (rc != FI_SUCCESS)
		M0_LOG(M0_ERROR, "aep_param_free failed %d", rc);

	rc = libfab_pep_res_free(&pep->pep_res, tm);
	if (rc != FI_SUCCESS)
		M0_LOG(M0_ERROR, "pep_res_free failed %d", rc);
	
	m0_free(pep);

	return M0_RC(rc);
}

/**
 * Used to free the endpoint and its resources.
 */
static int libfab_ep_param_free(struct m0_fab__ep *ep, struct m0_fab__tm *tm)
{
	int rc = 0;

	if (ep == NULL)
		return M0_RC(0);

	rc = libfab_pep_param_free(ep->fep_listen, tm);
	ep->fep_listen = NULL;
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	rc = libfab_aep_param_free(ep->fep_aep, tm);
	ep->fep_aep = NULL;
	if (rc != FI_SUCCESS)
		return M0_RC(rc);

	memset(&ep->fep_name, 0, sizeof(ep->fep_name));

	m0_free(ep);
	return M0_RC(rc);
}

/**
 * Used to free the transfer machine parameters
 */
static int libfab_tm_param_free(struct m0_fab__tm *tm)
{
	struct m0_fab__bulk_op  *op;
	struct m0_net_end_point *net;
	struct m0_fab__ep       *xep;
	int                      rc = 0;

	if (tm == NULL)
		return M0_RC(0);

	if (tm->ftm_poller.t_func != NULL) {
		m0_thread_join(&tm->ftm_poller);
		m0_thread_fini(&tm->ftm_poller);
	}

	M0_ASSERT(libfab_tm_is_locked(tm));
	m0_tl_for(m0_nep, &tm->ftm_ntm->ntm_end_points, net) {
		xep = libfab_ep(net);
		m0_nep_tlist_del(net);
		rc = libfab_ep_param_free(xep, tm);
	} m0_tl_endfor;
	M0_ASSERT(m0_nep_tlist_is_empty(&tm->ftm_ntm->ntm_end_points));
	tm->ftm_ntm->ntm_ep = NULL;
	
	if (tm->ftm_rctx != NULL) {
		rc = fi_close(&(tm->ftm_rctx)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "ftm_rctx fi_close ret=%d fid=%d",
			       rc, (int)(tm->ftm_rctx)->fid.fclass);
		tm->ftm_rctx = NULL;
	}

	if (tm->ftm_tx_cq != NULL) {
		rc = fi_close(&(tm->ftm_tx_cq)->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR, "tx_cq fi_close ret=%d fid=%d",
			       rc, (int)(tm->ftm_tx_cq)->fid.fclass);
		tm->ftm_tx_cq = NULL;
	}
	
	close(tm->ftm_epfd);

	m0_tl_for(fab_bulk, &tm->ftm_bulk, op) {
		fab_bulk_tlist_del(op);
		m0_free(op);
	} m0_tl_endfor;
	M0_ASSERT(fab_bulk_tlist_is_empty(&tm->ftm_bulk));
	fab_bulk_tlist_fini(&tm->ftm_bulk);

	return M0_RC(rc);
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
	return (buf->nb_tm != NULL) ? buf->nb_tm->ntm_xprt_private : NULL;
}

/**
 * Used to clean up the libfabric buffer structure.
 */
static void libfab_buf_fini(struct m0_fab__buf *buf)
{
	struct m0_fab__buf *fbp = NULL;
	fab_buf_tlink_fini(buf);
	if (buf->fb_ev_ep != NULL)
		buf->fb_ev_ep = NULL;
	if (buf->fb_bulk_op != NULL && fab_bulk_tlink_is_in(buf->fb_bulk_op)) {
		fab_bulk_tlist_del(buf->fb_bulk_op);
		m0_free(buf->fb_bulk_op);
		buf->fb_bulk_op = NULL;
	}
	if(buf->fb_txctx != NULL)
	{
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
	buf->fb_state = FAB_BUF_REGISTERED;

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
static void libfab_buf_complete(struct m0_fab__buf *buf, int32_t status)
{
	struct m0_fab__tm    *ma = libfab_buf_tm(buf);
	struct m0_net_buffer *nb = buf->fb_nb;
	struct m0_net_buffer_event ev = {
		.nbe_buffer = nb,
		.nbe_status = status,
		.nbe_time   = m0_time_now()
	};

	M0_ENTRY("b=%p q=%d rc=%d", buf, buf->fb_nb->nb_qtype, status);
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
 * Completes the buffer operation.
 */
static void libfab_buf_done(struct m0_fab__buf *buf, int rc)
{
	struct m0_fab__tm    *ma = libfab_buf_tm(buf);
	struct m0_net_buffer *nb = buf->fb_nb;
	struct m0_fab__buf   *pas_buf;
	struct iovec          iv;
	uint32_t             *ptr;
	int                   ret;

	M0_ENTRY("b=%p q=%d len=%d rc=%d", buf, nb->nb_qtype,
		 (int)buf->fb_length, rc);
	M0_PRE(libfab_tm_is_locked(ma));
	/*
	 * Multiple libfab_buf_done() calls on the same buffer are possible if
	 * the buffer is cancelled.
	 */
	if (!fab_buf_tlink_is_in(buf)) {
		buf->fb_status = buf->fb_status == 0 ? rc : buf->fb_status;
		/* Try to finalise. */
		if (m0_thread_self() == &ma->ftm_poller) {
			if (buf->fb_length == (sizeof(uint32_t) * 2)) {
				ptr = (uint32_t *)nb->nb_buffer.ov_buf[0];
				if (*ptr == FAB_DUMMY_DATA) {
					uint32_t token;
					ptr++;
					token = *ptr;
					pas_buf = fab_bufhash_htable_lookup(
						&ma->ftm_bufhash.bht_hash,
						&token);
					if (pas_buf != NULL)
						libfab_buf_complete(pas_buf, 0);

				/* Repost this buffer to the receive queue
				   without generating a callback as it contains
				   only dummy data */
					buf->fb_length = nb->nb_length;
					iv.iov_base = nb->nb_buffer.ov_buf[0];
					iv.iov_len = 
						nb->nb_buffer.ov_vec.v_count[0];
					ret = fi_recvv(ma->ftm_rctx, &iv,
						       buf->fb_mr.bm_desc, 1, 0,
						       &buf->fb_token);
					M0_ASSERT(ret == FI_SUCCESS);
				} else
					libfab_buf_complete(buf,
							    buf->fb_status);
			}
			else
				libfab_buf_complete(buf, buf->fb_status);
		}
		else {
			/* Otherwise, postpone finalisation to
			* libfab_tm_buf_done(). */
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
	M0_LOG(M0_DEBUG, "free endpoint %s", (char*)ep->fep_name.fen_str_addr);

	m0_nep_tlist_del(nep);
	libfab_ep_param_free(ep, tm);
}

/**
 * Generate unique key for memory registration.
 */
static uint64_t libfab_mr_keygen(void)
{
	uint64_t key = FAB_MR_KEY + mr_key_idx;
	mr_key_idx++;
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
				*(uint32_t *)entry[i].op_context;
			if (len != NULL)
				len[i] = entry[i].len;
			if (data != NULL)
				data[i] = ((entry[i].flags & wr_cqdata)) ? 
					  entry[i].data : 0;
		}
	}
	else if (ret != -FI_EAGAIN) {
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
		if (rc != FI_SUCCESS)
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
	libfab_ep_pton(&tm->ftm_pep->fep_name, &fbd->fbd_netaddr);
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
	struct m0_net_buffer   *nb = fb->fb_nb;
	struct m0_fab__ndom    *ndom = nb->nb_dom->nd_xprt_private;

	fb->fb_rbd = (struct m0_fab__bdesc *)(nb->nb_desc.nbd_data);
	fb->fb_riov = (struct fi_rma_iov *)(nb->nb_desc.nbd_data + 
					    sizeof(struct m0_fab__bdesc));
	libfab_ep_ntop(fb->fb_rbd->fbd_netaddr, epname);
	M0_ASSERT(fb->fb_rbd->fbd_iov_cnt <= ndom->fnd_seg_nr);
}

/**
 * Register the buffer with the appropriate access to the domain
 */
static int libfab_buf_dom_reg(struct m0_net_buffer *nb, struct fid_domain *dp)
{
	struct m0_fab__buf    *fbp = nb->nb_xprt_private;
	struct m0_fab__buf_mr *mr;
	uint64_t               key;
	int                    seg_nr = nb->nb_buffer.ov_vec.v_nr;
	int                    i;
	int                    ret = FI_SUCCESS;
	uint32_t               retry_cnt;
	struct m0_fab__ndom   *ndom = nb->nb_dom->nd_xprt_private;

	M0_PRE(fbp != NULL && dp != NULL);
	M0_PRE(seg_nr <= ndom->fnd_seg_nr);

	mr = &fbp->fb_mr;
	if (fbp->fb_dp == dp)
		return M0_RC(ret);

	if (fbp->fb_state == FAB_BUF_REGISTERED)
		M0_LOG(M0_ERROR,"Re-registeration of buffer");

	for (i = 0; i < seg_nr; i++) {
		retry_cnt = 20;
		key = libfab_mr_keygen();
		
		ret = fi_mr_reg(dp, nb->nb_buffer.ov_buf[i],
				nb->nb_buffer.ov_vec.v_count[i],
				FAB_MR_ACCESS, FAB_MR_OFFSET, key,
				FAB_MR_FLAG, &mr->bm_mr[i], NULL);

		if (ret != FI_SUCCESS) {
			M0_LOG(M0_ERROR, "fi_mr_reg = %d i =%d key = %"PRIu64,
			       ret, i, key);
			while (ret != FI_SUCCESS && retry_cnt > 0) {
				key = libfab_mr_keygen();
				ret = fi_mr_reg(dp, nb->nb_buffer.ov_buf[i],
						nb->nb_buffer.ov_vec.v_count[i],
						FAB_MR_ACCESS, FAB_MR_OFFSET,
						key, FAB_MR_FLAG, &mr->bm_mr[i],
						NULL);
				retry_cnt--;
			}
			M0_ASSERT(ret == FI_SUCCESS);
		}

		mr->bm_desc[i] = fi_mr_desc(mr->bm_mr[i]);
	}

	fbp->fb_dp = dp;
	fbp->fb_state = FAB_BUF_REGISTERED;

	return M0_RC(ret);
}

/**
 * Get the destination address from the endpoint name
 */
static uint64_t libfab_destaddr_get(struct m0_fab__ep_name *epname)
{
	uint64_t dst;

	libfab_ep_pton(epname, &dst);
	dst |= 0x02;	/* FI_SOCKADDR_IN */
	return dst;
}

/**
 * Buffers are queued before connection establishment, send those buffers
 * after connection establishment
 */
static void libfab_pending_bufs_send(struct m0_fab__ep *ep)
{
	struct m0_fab__active_ep *aep;
	struct m0_fab__buf       *fbp;
	struct m0_net_buffer     *nb;
	struct iovec              iv;
	struct fi_msg             op_msg;
	int                       ret = 0;

	aep = libfab_aep_get(ep);
	m0_tl_for(fab_sndbuf, &ep->fep_sndbuf, fbp) {
		nb = fbp->fb_nb;
		fbp->fb_txctx = ep;
		iv.iov_base = nb->nb_buffer.ov_buf[0];
		iv.iov_len = nb->nb_buffer.ov_vec.v_count[0];
		switch (nb->nb_qtype) {
			case M0_NET_QT_MSG_SEND:
				op_msg.msg_iov = &iv;
				op_msg.desc = fbp->fb_mr.bm_desc;
				op_msg.iov_count = 1;
				op_msg.addr = 0;
				op_msg.context = &fbp->fb_token;
				op_msg.data = 0;
				ret = fi_sendmsg(aep->aep_txep, &op_msg,
						 FI_COMPLETION);
				break;
			case M0_NET_QT_ACTIVE_BULK_RECV:
			case M0_NET_QT_ACTIVE_BULK_SEND:
				fbp->fb_wr_cnt = libfab_wr_cnt_get(fbp);
				ret = libfab_bulklist_add(libfab_buf_ma(nb),
							  fbp, aep);
				libfab_bulk_buf_process(libfab_buf_ma(nb));
				break;
			default:
				M0_LOG(M0_ERROR, "Invalid qtype=%d",
					nb->nb_qtype);
				break;
		}
		fab_sndbuf_tlist_del(fbp);
		if (ret != FI_SUCCESS) {
			libfab_buf_done(fbp, ret);
		}
	} m0_tl_endfor;
	M0_ASSERT(fab_sndbuf_tlist_is_empty(&ep->fep_sndbuf));
}

/**
 * Notify target endpoint about RDMA read completion,
 * so that buffer on remote endpoint shall be released.
 */
static int libfab_target_notify(struct m0_fab__buf *buf,
				struct m0_fab__active_ep *aep)
{
	uint32_t dummy[2]; /* Dummy data */
	int      ret = 0;
	
	if (buf->fb_nb->nb_qtype == M0_NET_QT_ACTIVE_BULK_RECV) {
		dummy[0] = FAB_DUMMY_DATA;
		dummy[1] = buf->fb_rbd->fbd_buftoken;
		
		ret = fi_send(aep->aep_txep, &dummy, sizeof(dummy), NULL,
			      0, NULL);
		M0_ASSERT(ret == FI_SUCCESS);
	}

	return M0_RC(ret);
}

/**
 * Allocate new fabric pointer and add it into list.
 */
static struct m0_fab__fab *libfab_newfab_init(struct m0_fab__ndom *fnd)
{
	struct m0_fab__fab *fab = NULL;

	// M0_PRE(m0_mutex_is_locked(&fnd->fnd_ndom->nd_mutex));
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
	char *h_ptr = tm->ftm_pep->fep_name.fen_str_addr;
	char *t_ptr;
	char  str_portal[10]={'\0'};
	int   len;

	libfab_ep_pton(&tm->ftm_pep->fep_name, &cd->fcd_netaddr);
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
	h_ptr = strchr(h_ptr+1, ':');	/* Skip the pid "12345" */
	t_ptr = strchr(h_ptr+1, ':');
	len = t_ptr - (h_ptr+1);
	strncpy(str_portal, h_ptr+1, len);
	cd->fcd_portal = atoi(str_portal);
	if(*(t_ptr+1) == '*')
		cd->fcd_tmid = 0xFFFF;
	else
		cd->fcd_tmid = atoi(t_ptr+1);
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
		dst = libfab_destaddr_get(&ep->fep_name);
		libfab_conn_data_fill(&cd, ma);

		ret = fi_getopt(&aep->aep_txep->fid, FI_OPT_ENDPOINT,
				FI_OPT_CM_DATA_SIZE,
				&cm_max_size, &opt_size);
		M0_ASSERT((ret == FI_SUCCESS) && (sizeof(cd) < cm_max_size));

		ret = fi_connect(aep->aep_txep, &dst, &cd, sizeof(cd));
		if (ret == FI_SUCCESS)
			aep->aep_tx_state = FAB_CONNECTING;
		else
			M0_LOG(M0_DEBUG, " Conn req failed ret=%d dst=%"PRIx64,
			       ret, dst);
	}
	
	if (ret == 0)
		fab_sndbuf_tlink_init_at_tail(fbp, &ep->fep_sndbuf);

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
	if (ret == FI_SUCCESS)
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
	struct m0_fab__ep_name *en = &ep->fep_name;
	struct m0_fab__fab     *fab = tm->ftm_fab;
	struct fi_info         *info;
	struct fi_info         *hints = NULL;
	int                     rc;
	bool                    isVerbs = libfab_is_verbs(tm);
	
	if (aep->aep_txep != NULL) {
		rc = fi_close(&aep->aep_txep->fid);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR,"aep_txep close failed %d",rc);
		
		rc = libfab_ep_txres_free(&aep->aep_tx_res, tm);
		if (rc != FI_SUCCESS)
			M0_LOG(M0_ERROR,"ep_txres_free failed %d",rc);
	}
	aep->aep_tx_state = FAB_NOT_CONNECTED;
	ep->fep_connlink = FAB_CONNLINK_DOWN;

	if (isVerbs) {
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
		M0_ASSERT(rc == FI_SUCCESS);
	} else
		info = tm->ftm_fab->fab_fi;

	rc = fi_endpoint(fab->fab_dom, info, &aep->aep_txep, NULL);
	if (rc != FI_SUCCESS) {
		M0_LOG(M0_ERROR,"fi_endpoint failed %d",rc);
		return M0_RC(rc);
	}
	
	rc = libfab_ep_txres_init(aep, tm, ctx);
	if (rc != FI_SUCCESS) {
		M0_LOG(M0_ERROR,"ep_txres_init failed %d",rc);
		return M0_RC(rc);
	}
	
	rc = fi_enable(aep->aep_txep);
	if (rc != FI_SUCCESS)
		M0_LOG(M0_ERROR,"fi_enable failed %d",rc);
	

	if (isVerbs) {
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
	if (rc != FI_SUCCESS)
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
	return (ep->fep_listen == NULL) ? ep->fep_aep : ep->fep_listen->pep_aep;
}

/**
 * Returns true if verbs provider is selected else false.
 */
static inline bool libfab_is_verbs(struct m0_fab__tm *tm)
{
	return (!strcmp(tm->ftm_fab->fab_fi->fabric_attr->prov_name, "verbs"));
}

/**
 * Add bulk operation into transfer machine bulk operation list.
 */
static int libfab_bulklist_add(struct m0_fab__tm *tm, struct m0_fab__buf *fb,
				struct m0_fab__active_ep *aep)
{
	struct m0_fab__bulk_op *op;

	M0_ALLOC_PTR(op);
	if (op == NULL)
		return -ENOMEM;
	op->fbl_aep = aep;
	op->fbl_buf = fb;
	fb->fb_bulk_op = op;
	fab_bulk_tlink_init_at_tail(op, &tm->ftm_bulk);

	return M0_RC(0);
}

/** 
 * This function will calculate the number of work requests that will be 
 * generated the for the bulk transfer operation (read/write) on the
 * net-buffer.
 * The wr_cnt is returned
 */
static uint32_t libfab_wr_cnt_get(struct m0_fab__buf *fb)
{
	struct fi_rma_iov *r_iov;
	m0_bcount_t       *v_cnt = fb->fb_nb->nb_buffer.ov_vec.v_count;
	m0_bcount_t        xfer_len = 0;
	m0_bcount_t        chunk;
	uint32_t           loc_sidx = 0;
	uint32_t           rem_sidx = 0;
	uint32_t           loc_soff = 0;
	uint32_t           rem_soff = 0;
	uint32_t           loc_slen;
	uint32_t           rem_slen;
	uint32_t           wr_cnt = 0;

	M0_ENTRY("loc_buf=%p q=%d loc_seg=%d rem_buf=%d rem_seg=%d",
		 fb, fb->fb_nb->nb_qtype, fb->fb_nb->nb_buffer.ov_vec.v_nr, 
		 (int)fb->fb_rbd->fbd_buftoken, (int)fb->fb_rbd->fbd_iov_cnt);
	M0_PRE(fb->fb_rbd != NULL);

	r_iov = fb->fb_riov;

	while(xfer_len < fb->fb_nb->nb_length) {
		M0_ASSERT(rem_sidx <= fb->fb_rbd->fbd_iov_cnt);
		loc_slen = v_cnt[loc_sidx] - loc_soff;
		rem_slen = r_iov[rem_sidx].len - rem_soff;
		
		chunk = min64u(loc_slen, rem_slen);

		if (loc_slen > rem_slen) {
			rem_sidx++;
			rem_soff = 0;
			loc_soff += chunk;
		} else {
			loc_sidx++;
			loc_soff = 0;
			rem_soff += chunk;
			if(rem_soff >= r_iov[rem_sidx].len) {
				rem_sidx++;
				rem_soff = 0;
			}
		}
		wr_cnt++;
		xfer_len += chunk;
	}
	return wr_cnt;
}

/**
 * Process bulk operation from transfer machine bulk operation list.
 */
static void libfab_bulk_buf_process(struct m0_fab__tm *tm)
{
	struct m0_fab__bulk_op *op;
	uint32_t                available;
	uint32_t                total = libfab_is_verbs(tm) ? 256 : 1024;

	m0_tl_for(fab_bulk, &tm->ftm_bulk, op) {
		/* Keeping 64 entries on CQ for rpc messages */
		available = (total - 64) - op->fbl_aep->aep_bulk_cnt;

		/* Only post the bulk buffer if there is enough space on the
		   libfabric queue and the endpoint is in connected state. */
		if (op->fbl_buf->fb_wr_cnt <= available &&
		    op->fbl_aep->aep_tx_state == FAB_CONNECTED) {
			libfab_bulk_op(op->fbl_aep, op->fbl_buf);
			fab_bulk_tlist_del(op);
			op->fbl_buf->fb_bulk_op = NULL;
			m0_free(op);
		}
	} m0_tl_endfor;
}

/** 
 * This function will call the bulk transfer operation (read/write) on the
 * net-buffer.
 */
static int libfab_bulk_op(struct m0_fab__active_ep *aep, struct m0_fab__buf *fb)
{
	struct fi_msg_rma  op_msg;
	struct fi_rma_iov *r_iov;
	struct fi_rma_iov  remote;
	m0_bcount_t       *v_cnt = fb->fb_nb->nb_buffer.ov_vec.v_count;
	m0_bcount_t        xfer_len = 0;
	struct iovec       iv;
	uint64_t           op_flag;
	uint32_t           loc_sidx = 0;
	uint32_t           rem_sidx = 0;
	uint32_t           loc_soff = 0;
	uint32_t           rem_soff = 0;
	uint32_t           loc_slen;
	uint32_t           rem_slen;
	uint32_t           wr_cnt = 0;
	bool               isread;
	int                ret = 0;
	bool               last_seg = false;

	M0_ENTRY("loc_buf=%p q=%d loc_seg=%d rem_buf=%d rem_seg=%d",
		 fb, fb->fb_nb->nb_qtype, fb->fb_nb->nb_buffer.ov_vec.v_nr, 
		 (int)fb->fb_rbd->fbd_buftoken, (int)fb->fb_rbd->fbd_iov_cnt);
	M0_PRE(fb->fb_rbd != NULL);

	r_iov = fb->fb_riov;
	isread = (fb->fb_nb->nb_qtype == M0_NET_QT_ACTIVE_BULK_RECV);
	fb->fb_wr_comp_cnt = 0;

	while(xfer_len < fb->fb_nb->nb_length) {
		M0_ASSERT(rem_sidx <= fb->fb_rbd->fbd_iov_cnt);
		loc_slen = v_cnt[loc_sidx] - loc_soff;
		rem_slen = r_iov[rem_sidx].len - rem_soff;
		
		iv.iov_base = fb->fb_nb->nb_buffer.ov_buf[loc_sidx] + loc_soff;
		iv.iov_len = min64u(loc_slen, rem_slen);

		if (xfer_len + iv.iov_len >= fb->fb_nb->nb_length)
			last_seg = true;
		
		remote = r_iov[rem_sidx];
		remote.addr += rem_soff;
		remote.len -= rem_soff;
		
		op_msg.msg_iov       = &iv;
		op_msg.desc          = &fb->fb_mr.bm_desc[loc_sidx];
		op_msg.iov_count     = 1;
		op_msg.addr          = rem_soff;
		op_msg.rma_iov       = &remote;
		op_msg.rma_iov_count = 1;
		op_msg.context       = &fb->fb_token;
		
		op_msg.data = (isread || (!last_seg)) ? 0 :
			      fb->fb_rbd->fbd_buftoken;
		op_flag = (isread || (!last_seg)) ? 0 : FI_REMOTE_CQ_DATA;
		op_flag |= last_seg ? FI_COMPLETION : 0;
		
		if (isread)
			ret = fi_readmsg(aep->aep_txep, &op_msg, op_flag);
		else
			ret = fi_writemsg(aep->aep_txep, &op_msg, op_flag);

		if (ret != FI_SUCCESS) {
			M0_LOG(M0_ERROR,"bulk-op failed %d b=%p q=%d l_seg=%d",
			       ret, fb, fb->fb_nb->nb_qtype, loc_sidx);
			break;
		}

		if (loc_slen > rem_slen) {
			rem_sidx++;
			rem_soff = 0;
			loc_soff += iv.iov_len;
		} else {
			loc_sidx++;
			loc_soff = 0;
			rem_soff += iv.iov_len;
			if(rem_soff >= r_iov[rem_sidx].len) {
				rem_sidx++;
				rem_soff = 0;
			}
		}
		wr_cnt++;
		aep->aep_bulk_cnt++;
		xfer_len += iv.iov_len;
	}
	
	if (ret == FI_SUCCESS)
		M0_ASSERT(wr_cnt == fb->fb_wr_cnt);

	return M0_RC(ret);
}

/** 
 * This function returns a unique token number.
 */
static uint32_t libfab_buf_token_get(struct m0_fab__buf *fb)
{
	uint32_t ret = (fb->fb_nb->nb_qtype);
	++buf_token;
	if (buf_token >= (1 << 24))
		buf_token = 1;
	ret |= (buf_token << 8);
	return ret;
}

static int libfab_domain_params_get(struct m0_fab__ndom *fab_ndom)
{
	struct fi_info *hints;
	struct fi_info *fi;
	int             result = 0;

	hints = fi_allocinfo();
	if( hints == NULL)
		return M0_ERR(-ENOMEM);
	hints->fabric_attr->prov_name = providers[0];
	result = fi_getinfo(FI_VERSION(1,11), NULL, NULL, 0, hints, &fi);
	if(result == FI_SUCCESS) {
		/* For Verbs provider */
		inet_ntop(AF_INET,
			  &((struct sockaddr_in *)fi->src_addr)->sin_addr,
			  fab_ndom->fnd_loc_ip, 16);
		fab_ndom->fnd_seg_nr = FAB_VERBS_IOV_MAX;
		fab_ndom->fnd_seg_size = FAB_VERBS_MAX_BULK_SEG_SIZE;
	} else {
		/* For TCP/Socket provider */
		strcpy(fab_ndom->fnd_loc_ip, "127.0.0.1");
		fab_ndom->fnd_seg_nr = FAB_TCP_SOCK_IOV_MAX;
		fab_ndom->fnd_seg_size = FAB_TCP_SOCK_MAX_BULK_SEG_SIZE;
	}

	hints->fabric_attr->prov_name = NULL;
	fi_freeinfo(hints);
	fi_freeinfo(fi);
	return M0_RC(0);
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
	if (ret != FI_SUCCESS)
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
	struct m0_fab__ndom     *fnd;
	struct m0_fab__fab      *fab;
	int                      rc;
	
	M0_ENTRY();
	libfab_dom_invariant(dom);
	fnd = dom->nd_xprt_private;
	m0_tl_for(fab_fabs, &fnd->fnd_fabrics, fab) {
		if (fab->fab_dom != NULL) {
			rc = fi_close(&(fab->fab_dom)->fid);
			if (rc != FI_SUCCESS)
				M0_LOG(M0_ERROR, "fab_dom fi_close ret=%d", rc);
			fab->fab_dom = NULL;
		}
		
		if (fab->fab_fab != NULL) {
			rc = fi_close(&(fab->fab_fab)->fid);
			if (rc != FI_SUCCESS)
				M0_LOG(M0_ERROR, "fab_fabric fi_close ret=%d",
				       rc);
			fab->fab_fab = NULL;
		}

		if (fab->fab_fi != NULL) {
			fi_freeinfo(fab->fab_fi);
			fab->fab_fi = NULL;
		}

		fab_fabs_tlist_del(fab);
		m0_free(fab);
	} m0_tl_endfor;
	M0_ASSERT(fab_fabs_tlist_is_empty(&fnd->fnd_fabrics));
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
	
	M0_ASSERT(fab_bufhash_htable_is_empty(&ma->ftm_bufhash.bht_hash));
	fab_bufhash_htable_fini(&ma->ftm_bufhash.bht_hash);
	
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
		ftm->ftm_tmout_check = m0_time_now();
		fab_buf_tlist_init(&ftm->ftm_done);
		fab_bulk_tlist_init(&ftm->ftm_bulk);
		ftm->ftm_bufhash.bht_magic = M0_NET_LIBFAB_BUF_HT_HEAD_MAGIC;
		rc = fab_bufhash_htable_init(&ftm->ftm_bufhash.bht_hash,
					     M0_NET_QT_NR);
	} else
		rc = M0_ERR(-ENOMEM);

	if (rc != 0)
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
		rc = libfab_passive_ep_create(ftm->ftm_pep, ftm);
		if (rc != FI_SUCCESS)
			return M0_RC(rc);

		m0_ref_init(&ftm->ftm_pep->fep_nep.nep_ref, 1, 
				&libfab_ep_release);
		libfab_ep_get(ftm->ftm_pep);
		nep = &ftm->ftm_pep->fep_nep;
		nep->nep_tm = ntm;

		m0_nep_tlink_init_at_tail(nep, &ntm->ntm_end_points);
		ftm->ftm_pep->fep_nep.nep_addr = 
					ftm->ftm_pep->fep_name.fen_str_addr;

		m0_mutex_init(&ftm->ftm_endlock);
		m0_mutex_init(&ftm->ftm_evpost);

		if (rc == FI_SUCCESS)
			rc = M0_THREAD_INIT(&ftm->ftm_poller,
					    struct m0_fab__tm *, NULL,
					    &libfab_poller, ftm,
					    "libfab_tm");
	} else
		rc = M0_ERR(-ENOMEM);

	libfab_tm_evpost_lock(ftm);
	libfab_tm_unlock(ftm);
	libfab_tm_event_post(ftm, M0_NET_TM_STARTED);
	libfab_tm_evpost_unlock(ftm);
	libfab_tm_lock(ftm);
	ftm->ftm_state = FAB_TM_STARTED;

	return M0_RC(0);
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
	return -ENOSYS;
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
	int rc = 0;

	M0_ENTRY("name=%s", name);

	rc = libfab_ep_find(tm, name, NULL, epp);
	if (rc != 0)
		return M0_RC(rc);

	return M0_RC(rc);
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
	struct m0_fab__ndom *nd = nb->nb_dom->nd_xprt_private;
	int                  i;
	int                  ret;

	M0_ENTRY("Deregister and free buf = %p", fb);
	M0_PRE(nb->nb_flags == M0_NET_BUF_REGISTERED &&
	       libfab_buf_invariant(fb));

	for (i = 0; i < nd->fnd_seg_nr; i++) {
		if (fb->fb_mr.bm_mr[i] != NULL) {
			ret = fi_close(&fb->fb_mr.bm_mr[i]->fid);
			M0_ASSERT(ret == FI_SUCCESS);
			fb->fb_mr.bm_mr[i] = NULL;
		}
	}

	libfab_buf_fini(fb);
	fb->fb_state = FAB_BUF_DEREGISTERED;
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
	int                  ret = 0;

	M0_PRE(nb->nb_xprt_private == NULL);
	M0_PRE(nb->nb_dom != NULL);

	M0_ALLOC_PTR(fb);
	if (fb == NULL)
		return M0_ERR(-ENOMEM);

	M0_ALLOC_ARR(fb->fb_mr.bm_desc, nd->fnd_seg_nr);
	if (fb->fb_mr.bm_desc == NULL) {
		m0_free(fb);
		return M0_ERR(-ENOMEM);
	}

	M0_ALLOC_ARR(fb->fb_mr.bm_mr, nd->fnd_seg_nr);
	if (fb->fb_mr.bm_mr == NULL) {
		m0_free(fb->fb_mr.bm_desc);
		m0_free(fb);
		return M0_ERR(-ENOMEM);
	}

	fab_buf_tlink_init(fb);
	nb->nb_xprt_private = fb;
	fb->fb_nb = nb;
	fb->fb_state = FAB_BUF_INITIALIZED;

	return M0_RC(ret);
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
	struct m0_fab__ep_name    epname;
	struct fi_msg             op_msg;
	int                       ret = 0;

	M0_ENTRY("b=%p q=%d l=%"PRIu64, nb, nb->nb_qtype, nb->nb_length);

	M0_PRE(libfab_tm_is_locked(ma) && libfab_tm_invariant(ma) &&
	       libfab_buf_invariant(fbp));
	M0_PRE(nb->nb_offset == 0); /* Do not support an offset during add. */
	M0_PRE((nb->nb_flags & M0_NET_BUF_RETAIN) == 0);

	fbp->fb_token = libfab_buf_token_get(fbp);
	libfab_buf_dom_reg(nb, ma->ftm_fab->fab_dom);
	fbp->fb_status = 0;

	switch (nb->nb_qtype) {
	case M0_NET_QT_MSG_RECV: {
		M0_ASSERT(nb->nb_buffer.ov_vec.v_nr == 1);
		fbp->fb_length = nb->nb_length;
		iv.iov_base = nb->nb_buffer.ov_buf[0];
		iv.iov_len = nb->nb_buffer.ov_vec.v_count[0];
		ret = fi_recvv(ma->ftm_rctx, &iv, fbp->fb_mr.bm_desc, 1, 0,
			       &fbp->fb_token);
		break;
	}
	
	case M0_NET_QT_MSG_SEND: {
		M0_ASSERT(nb->nb_length <= m0_vec_count(&nb->nb_buffer.ov_vec));
		M0_ASSERT(nb->nb_buffer.ov_vec.v_nr == 1);
		ret = libfab_fab_ep_find(ma, NULL, nb->nb_ep->nep_addr, &ep);
		if (ret != FI_SUCCESS)
			break;
		aep = libfab_aep_get(ep);
		fbp->fb_txctx = ep;
		
		if (aep->aep_tx_state != FAB_CONNECTED)
			ret = libfab_conn_init(ep, ma, fbp);
		else {
			iv.iov_base = nb->nb_buffer.ov_buf[0];
			iv.iov_len = nb->nb_buffer.ov_vec.v_count[0];
			op_msg.msg_iov = &iv;
			op_msg.desc = fbp->fb_mr.bm_desc;
			op_msg.iov_count = 1;
			op_msg.addr = 0;
			op_msg.context = &fbp->fb_token;
			op_msg.data = 0;
			ret = fi_sendmsg(aep->aep_txep, &op_msg, FI_COMPLETION);
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
				      &fbp->fb_token);
		
		if (ret == FI_SUCCESS)
			ret = libfab_bdesc_encode(fbp);
		break;
	}

	/* For active buffers, decode the passive buffer descriptor */
	case M0_NET_QT_ACTIVE_BULK_RECV:
		fbp->fb_length = nb->nb_length;
		/* Intentional fall through */

	case M0_NET_QT_ACTIVE_BULK_SEND: {
		memset(&epname, 0, sizeof(epname));
		libfab_bdesc_decode(fbp, &epname);
		ret = libfab_fab_ep_find(ma, &epname, NULL, &ep);
		if (ret != FI_SUCCESS)
			break;
		fbp->fb_txctx = ep;
		aep = libfab_aep_get(ep);
		if (aep->aep_tx_state != FAB_CONNECTED)
			ret = libfab_conn_init(ep, ma, fbp);
		else {
			fbp->fb_wr_cnt = libfab_wr_cnt_get(fbp);
			ret = libfab_bulklist_add(ma, fbp, aep);
			libfab_bulk_buf_process(ma);
		}
		break;
	}

	default:
		M0_IMPOSSIBLE("invalid queue type: %x", nb->nb_qtype);
		break;
	}

	if (ret == FI_SUCCESS) {
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

	if (buf->fb_state != FAB_BUF_QUEUED)
		buf->fb_state = FAB_BUF_QUEUED;

	if (M0_IN(buf->fb_nb->nb_qtype, (M0_NET_QT_MSG_SEND,
					 M0_NET_QT_ACTIVE_BULK_SEND,
					 M0_NET_QT_ACTIVE_BULK_RECV)))
		buf->fb_status = -ECANCELED;
	else
		libfab_buf_done(buf, -ECANCELED);
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

	return (m0_bcount_t)(sizeof(struct m0_fab__bdesc) + 
		             (sizeof(struct fi_rma_iov) * nd->fnd_seg_nr));
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
