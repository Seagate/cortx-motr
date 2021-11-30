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


#include "lib/assert.h"
#include "lib/errno.h"
#include "net/net_internal.h"
#include <arpa/inet.h>          /* inet_pton, htons */
#include <netdb.h>              /* hostent */
#ifndef __KERNEL__
#  include "lib/string.h"       /* atoi, isdigit */
#endif

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_NET
#include "lib/trace.h"

static const char *ip_family[M0_NET_IP_AF_MAX]      = { "inet",
							"inet6",
							"unix" };

static const char *ip_protocol[M0_NET_IP_PROTO_MAX] = { "tcp",
							"verbs",
							"stream",
							"dgram",
							"o2ib",
							"lo" };

/**
 * Bitmap of used transfer machine identifiers. 1 is for used and 0 is for free.
 */
static uint8_t ip_autotm[1024] = {};

/**
 * This function convert the ip to hostname/FQDN format.
 * Here ip is string containing ip address with numbes-and-dot notation.
 */
static void m0_net_ip_to_hostname(char *ip, char *hostname)
{
	struct hostent *he;
	struct in_addr  addr;
	int             rc;

	inet_aton(ip, &addr);
	he = gethostbyaddr(&addr, sizeof(addr), AF_INET);
	M0_LOG(M0_DEBUG, "ip: %s to hostname: %s\n",ip, he->h_name);
	rc = snprintf(hostname, M0_NET_IP_STRLEN_MAX, "%s", he->h_name);

	if (rc >= M0_NET_IP_STRLEN_MAX)
		M0_LOG(M0_ERROR, "Hostname too long. Reqd: %d buf_len: %d", rc,
		       M0_NET_IP_STRLEN_MAX);
}

/**
 * This function decodes the inet format address.
 *  The inet address format is of type
 *    <family>:<type>:<ipaddr/hostname_FQDN>@<port>
 *    for example: "inet:tcp:127.0.0.1@3000",
 *                 "inet:stream:lanl.gov@23",
 *                 "inet6:dgram:FE80::0202:B3FF:FE1E:8329@6663"
 * Return value: 0 in case of success.
 *               < 0 in case of error.
 */
static int m0_net_ip_inet_parse(const char *name, struct m0_net_ip_addr *addr)
{
	uint32_t     portnum;
	int          shift = 0;
	int          f;
	int          s;
	int          rc;
	char         ip[M0_NET_IP_STRLEN_MAX] = {};
	char         port[M0_NET_IP_PORTLEN_MAX] = {};
	char        *at;
	const char  *ep_name = name;
	const char  *tail = name + strlen(name) - 1;

	if (!isdigit(tail[0]))
		return M0_ERR(-EINVAL);

	for (f = 0; f < ARRAY_SIZE(ip_family); ++f) {
		if (ip_family[f] != NULL) {
			shift = strlen(ip_family[f]);
			if (strncmp(ep_name, ip_family[f], shift) == 0)
				break;
		}
	}
	if (f >= ARRAY_SIZE(ip_family) || ep_name[shift] != ':')
		return M0_ERR(-EINVAL);
	ep_name += shift + 1;
	for (s = 0; s < ARRAY_SIZE(ip_protocol); ++s) {
		if (ip_protocol[s] != NULL) {
			shift = strlen(ip_protocol[s]);
			if (strncmp(ep_name, ip_protocol[s], shift) == 0)
				break;
		}
	}
	if (s >= ARRAY_SIZE(ip_protocol) || ep_name[shift] != ':')
		return M0_ERR(-EINVAL);
	ep_name += shift + 1;
	at = strchr(ep_name, '@');
	if (at == NULL)
		return M0_ERR(-EINVAL);
	else {
		at++;
		if (at == NULL || at[0] < '0' || at[0] > '9')
			return M0_ERR(-EINVAL);
		strcpy(port, at);
		portnum = atoi(port);
		M0_ASSERT(portnum < M0_NET_IP_PORT_MAX);
		addr->nia_n.nip_port = (uint16_t)portnum;
	}

	rc = m0_net_hostname_to_ip((char *)ep_name, ip, &addr->nia_n.nip_format);
	if (rc == 0)
		inet_pton(f == M0_NET_IP_AF_INET ? AF_INET : AF_INET6,
			  ip, &addr->nia_n.ip_n.sn[0]);

	M0_ASSERT(strlen(name) < ARRAY_SIZE(addr->nia_p));
	strcpy(addr->nia_p, name);
	addr->nia_n.fmt_pvt.ia.nia_family = f;
	addr->nia_n.fmt_pvt.ia.nia_type = s;

	/* Ignore the error due to gethostbyname() as it will be retried. */
	return rc >= 0 ? M0_RC(0) : M0_ERR(rc);
}

/**
 * This function decodes the lnet format address and extracts the ip address and
 * port number from it.
 * This is also used to allocate unique transfer machine identifiers for LNet
 * network addresses with wildcard transfer machine identifier (like
 * "192.168.96.128@tcp:12345:31:*").
 * Return value: 0 in case of success.
 *               < 0 in case of error.
 */
static int m0_net_ip_lnet_parse(const char *name, struct m0_net_ip_addr *addr)
{
	char        *at = NULL;
	int          nr;
	int          i;
	unsigned     pid;
	unsigned     portal;
	unsigned     portnum;
	unsigned     tmid;
	char         node[M0_NET_IP_STRLEN_MAX] = {};
	char         port[M0_NET_IP_PORTLEN_MAX] = {};
	const char  *ep_name = name;
	uint32_t     nia_n;
	int          shift;
	int          s;
	const char  *tail = name + strlen(name) - 1;
	int32_t      n[4];
	int          rc;

	if (!isdigit(tail[0]) && tail[0] != '*')
		return M0_ERR(-EINVAL);

	at = strchr(ep_name, '@');
	if (strncmp(ep_name, "0@lo", 4) == 0) {
		nia_n = htonl(INADDR_LOOPBACK);
		inet_ntop(AF_INET, &nia_n, node, ARRAY_SIZE(node));
	} else {
		if (at == NULL || at - ep_name >= sizeof node)
			return M0_ERR(-EPROTO);

		M0_PRE(sizeof node >= (at-ep_name)+1);
		memcpy(node, ep_name, at - ep_name);
		//verify ip address
		rc = sscanf(node, "%d.%d.%d.%d", &n[0], &n[1], &n[2], &n[3]);
		if (rc != 4 ||
		    m0_exists(i, ARRAY_SIZE(n), n[i] < 0 || n[i] > 255))
			return M0_RC(-EINVAL); /* invalid IPv4 address */
	}
	at++;

	for (s = 0; s < ARRAY_SIZE(ip_protocol); s++) {
		if (ip_protocol[s] != NULL) {
			shift = strlen(ip_protocol[s]);
			if (strncmp(at, ip_protocol[s], shift) == 0)
			{
				addr->nia_n.fmt_pvt.la.nla_type = s;
				break;
			}
		}
	}

	if (s == ARRAY_SIZE(ip_protocol) || ((at = strchr(at, ':')) == NULL))
		return M0_ERR(-EPROTO);

	nr = sscanf(at + 1, "%u:%u:%u", &pid, &portal, &tmid);
	if (nr != 3) {
		nr = sscanf(at + 1, "%u:%u:*", &pid, &portal);
		if (nr != 2)
			return M0_ERR(-EPROTO);
		for (i = 0; i < ARRAY_SIZE(ip_autotm); ++i) {
			if (ip_autotm[i] == 0) {
				tmid = i;
				/* To handle '*' wildchar as tmid*/
				addr->nia_n.fmt_pvt.la.nla_autotm = true;
				break;
			}
		}
		if (i == ARRAY_SIZE(ip_autotm))
			return M0_ERR(-EADDRNOTAVAIL);
	} else
		addr->nia_n.fmt_pvt.la.nla_autotm = false;

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

	addr->nia_n.fmt_pvt.la.nla_portal =  portal;
	if (portal < 30)
		portal = 30 + portal;

	portnum  = tmid | (1 << 10) | ((portal - 30) << 11);
	M0_ASSERT(portnum < M0_NET_IP_PORT_MAX);
	snprintf(port, ARRAY_SIZE(port), "%d", (int)portnum);
	ip_autotm[tmid] = 1;

	addr->nia_n.nip_format = M0_NET_IP_LNET_FORMAT;
	inet_pton(AF_INET, node, &addr->nia_n.ip_n.sn[0]);
	addr->nia_n.fmt_pvt.la.nla_tmid = tmid;
	addr->nia_n.nip_port = (uint16_t)portnum;
	M0_ASSERT(strlen(name) < ARRAY_SIZE(addr->nia_p));
	strcpy(addr->nia_p, name);

	return M0_RC(0);
}

/**
 * Compare ipv4 address in network byte order.
 */
static bool m0_net_ip_v4_cmp(uint32_t *a1, uint32_t *a2)
{
	return (a1[0] == a2[0]);
}

/**
 * Compare ipv6 address in network byte order.
 */
static bool m0_net_ip_v6_cmp(uint64_t *a1, uint64_t *a2)
{
	return (a1[0] == a2[0] && a1[1] == a2[1]);
}

/**
 * Compare lnet address format specific parameters.
 */
static bool m0_net_ip_la_cmp(struct m0_net_ip_addr *a1,
			     struct m0_net_ip_addr *a2)
{
	struct m0_net_ip_lnet_addr *la1;
	struct m0_net_ip_lnet_addr *la2;

	M0_PRE(a1 != NULL && a2 != NULL);
	la1 = &a1->nia_n.fmt_pvt.la;
	la2 = &a2->nia_n.fmt_pvt.la;

	return (la1->nla_type   == la2->nla_type   &&
		la1->nla_portal == la2->nla_portal &&
		la1->nla_tmid   == la2->nla_tmid   &&
		la1->nla_autotm == la2->nla_autotm &&
		m0_net_ip_v4_cmp(&a1->nia_n.ip_n.sn[0], &a2->nia_n.ip_n.sn[0]));
}

/**
 * Compare inet address format specific parameters.
 */
static bool m0_net_ip_ia_cmp(struct m0_net_ip_addr *a1,
			     struct m0_net_ip_addr *a2)
{
	struct m0_net_ip_inet_addr *ia1;
	struct m0_net_ip_inet_addr *ia2;

	M0_PRE(a1 != NULL && a2 != NULL);
	ia1 = &a1->nia_n.fmt_pvt.ia;
	ia2 = &a2->nia_n.fmt_pvt.ia;

	return (ia1->nia_family == ia2->nia_family &&
		ia1->nia_type   == ia2->nia_type &&
		ia1->nia_family == M0_NET_IP_AF_INET ?
		m0_net_ip_v4_cmp(&a1->nia_n.ip_n.sn[0], &a2->nia_n.ip_n.sn[0]) :
		m0_net_ip_v6_cmp(&a1->nia_n.ip_n.ln[0], &a2->nia_n.ip_n.ln[0]));
}

M0_INTERNAL int m0_net_ip_parse(const char *name, struct m0_net_ip_addr *addr)
{
	return (name[0] >= '0' && name[0] <= '9') ?
		m0_net_ip_lnet_parse(name, addr) :
		m0_net_ip_inet_parse(name, addr);
}

M0_INTERNAL int m0_net_ip_print(const struct m0_net_ip_addr *nia)
{
	char  ip_p[INET6_ADDRSTRLEN] = {};
	char  hostname[M0_NET_IP_STRLEN_MAX] = {};
	char  tmid[6] = {};
	char *buf = (char *)nia->nia_p;
	const struct m0_net_ip_params *na = &nia->nia_n;
	int   rc = 0;

	M0_ENTRY("frmt=%d ip_n=[0x%"PRIx64",0x%"PRIx64"] port=%d",
		 (int)na->nip_format, na->ip_n.ln[0], na->ip_n.ln[1],
		 (int)na->nip_port);

	if (na->nip_format == M0_NET_IP_LNET_FORMAT)
		M0_LOG(M0_DEBUG, "type=%d portal=%d tmid=%d autotm=%s",
		       (int)na->fmt_pvt.la.nla_type,
		       (int)na->fmt_pvt.la.nla_portal,
		       (int)na->fmt_pvt.la.nla_tmid,
		       na->fmt_pvt.la.nla_autotm ? "true" : "false");
	else
		M0_LOG(M0_DEBUG, "family=%d type=%d",
		       (int)na->fmt_pvt.ia.nia_family,
		       (int)na->fmt_pvt.ia.nia_type);

	if (na->nip_format == M0_NET_IP_LNET_FORMAT) {
		rc = na->fmt_pvt.la.nla_autotm ?
		     snprintf(tmid, ARRAY_SIZE(tmid), "*") :
		     snprintf(tmid, ARRAY_SIZE(tmid), "%d",
			      na->fmt_pvt.la.nla_tmid);
		M0_ASSERT(rc < ARRAY_SIZE(tmid));
		inet_ntop(AF_INET, &na->ip_n.sn[0], ip_p, ARRAY_SIZE(ip_p));
		rc = snprintf(buf, M0_NET_IP_STRLEN_MAX,
			      "%s@%s:12345:%d:%s",
			      na->fmt_pvt.la.nla_type == M0_NET_IP_PROTO_LO ?
			      "0" : ip_p,
			      na->fmt_pvt.la.nla_type ==
			      M0_NET_IP_PROTO_LO ? "lo" :
			      ((na->fmt_pvt.la.nla_type ==
			      M0_NET_IP_PROTO_TCP) ? "tcp": "o2ib"),
			      na->fmt_pvt.la.nla_portal, tmid);
	} else if (na->nip_format == M0_NET_IP_INET_IP_FORMAT) {
		if (na->fmt_pvt.ia.nia_family != M0_NET_IP_AF_UNIX) {
			inet_ntop(na->fmt_pvt.ia.nia_family ==
				  M0_NET_IP_AF_INET ? AF_INET : AF_INET6,
				  &na->ip_n.sn[0], ip_p, ARRAY_SIZE(ip_p));
			rc = snprintf(buf, M0_NET_IP_STRLEN_MAX, "%s:%s:%s@%d",
				      na->fmt_pvt.ia.nia_family ==
				      M0_NET_IP_AF_INET ? "inet" : "inet6",
				      ip_protocol[na->fmt_pvt.ia.nia_type],
				      ip_p, na->nip_port);
		} else
			M0_LOG(M0_ERROR, "Format is currently not supported");
	} else if (na->nip_format == M0_NET_IP_INET_HOSTNAME_FORMAT) {
		inet_ntop(AF_INET, &na->ip_n.sn[0], ip_p, ARRAY_SIZE(ip_p));
		m0_net_ip_to_hostname(ip_p, hostname);
		rc = snprintf(buf, M0_NET_IP_STRLEN_MAX, "inet:%s:%s@%d", 
			      ip_protocol[na->fmt_pvt.ia.nia_type], hostname,
			      na->nip_port);
	}
	if (rc >= M0_NET_IP_STRLEN_MAX)
		M0_LOG(M0_ERROR, "Name too long. Reqd: %d buf_len: %d", rc,
		       M0_NET_IP_STRLEN_MAX);
	M0_LOG(M0_DEBUG, "Address constructed: %s", buf);

	return 0;
}

M0_INTERNAL int m0_net_hostname_to_ip(char *hostname, char *ip,
				      enum m0_net_ip_format *fmt)
{
	struct hostent  *hname;
	struct in_addr **addr;
	uint32_t         ip_n[4];
	int              i;
	int              n;
	char            *cp;
	char             name[M0_NET_IP_STRLEN_MAX] = {};

	M0_ENTRY("Hostname=%s", (char*)hostname);
	cp = strchr(hostname, '@');
	if (cp == NULL)
		return M0_ERR(-EINVAL);

	n = cp - hostname;
	name[n] = '\0';
	strncat(name, hostname, n);

	if (inet_pton(AF_INET, name, &ip_n[0]) == 1 ||
	    inet_pton(AF_INET6, name, &ip_n[0]) == 1) {
		/* Copy ip address as it is. */
		*fmt = M0_NET_IP_INET_IP_FORMAT;
		strcpy(ip, name);
	} else {
		*fmt = M0_NET_IP_INET_HOSTNAME_FORMAT;
		if ((hname = gethostbyname(name)) == NULL) {
			M0_LOG(M0_ERROR, "gethostbyname err=%d for %s",
			       h_errno, (char*)name);
			/* Return error code for gethostbyname failure */
			return M0_ERR(h_errno);
		}
		addr = (struct in_addr **)hname->h_addr_list;
		for(i = 0; addr[i] != NULL; i++) {
			/* Return the first one. */
			strcpy(ip, inet_ntoa(*addr[i]));
			M0_LOG(M0_DEBUG, "fqdn=%s ip=%s", (char*)name, ip);
			return M0_RC(0);
		}

		/* If no valid addr structure found, then return error */
		return M0_ERR(-errno);
	}

	return M0_RC(0);
}

M0_INTERNAL bool m0_net_ip_addr_cmp(struct m0_net_ip_addr *addr1,
				    struct m0_net_ip_addr *addr2, bool is_ncmp)
{
	M0_PRE(addr1 != NULL && addr2 != NULL);
	if (!is_ncmp)
		return (strcmp(addr1->nia_p, addr2->nia_p) == 0);
	else
		return (addr1->nia_n.nip_format == addr2->nia_n.nip_format &&
			addr1->nia_n.nip_port == addr2->nia_n.nip_port     &&
			/* For lnet address compare using m0_net_ip_la_cmp(). */
			((addr1->nia_n.nip_format == M0_NET_IP_LNET_FORMAT &&
			m0_net_ip_la_cmp(addr1, addr2))                    ||
			/* For inet address compare using m0_net_ip_ia_cmp(). */
			(addr1->nia_n.nip_format != M0_NET_IP_LNET_FORMAT  &&
			m0_net_ip_ia_cmp(addr1, addr2))));
}

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
