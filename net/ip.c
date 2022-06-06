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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_NET
#include "lib/trace.h"

#ifndef __KERNEL__

#include "net/net_internal.h"
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/string.h"
#include <arpa/inet.h>          /* inet_pton, htons */
#include <netdb.h>              /* hostent */
#include <stdlib.h>             /* atoi */

static const char *ip_family[M0_NET_IP_AF_NR] = {
						[M0_NET_IP_AF_INET]  = "inet",
						[M0_NET_IP_AF_INET6] = "inet6",
						[M0_NET_IP_AF_UNIX]  = "unix" };

static const char *ip_protocol[M0_NET_IP_PROTO_NR] = {
					    [M0_NET_IP_PROTO_TCP]    = "tcp",
					    [M0_NET_IP_PROTO_UDP]    = "udp",
					    [M0_NET_IP_PROTO_VERBS]  = "verbs",
					    [M0_NET_IP_PROTO_O2IB]   = "o2ib" };

/* This is the max strlen of members of ip_family and ip_protocol */
#define MAX_PREFIX_STRLEN    10

/**
 * Bitmap of used transfer machine identifiers. 1 is for used and 0 is for free.
 */
static uint8_t ip_autotm[1024] = {};

enum { IP_AUTOTM_NR = ARRAY_SIZE(ip_autotm) };

/**
 * Lock used while parsing lnet address.
 */
static struct m0_mutex autotm_lock = {};

/**
 * This function convert the ip to hostname/FQDN format.
 * Here ip is string containing ip address with numbes-and-dot notation.
 */
static int m0_net_ip_to_hostname(const char *ip, char *hostname)
{
	struct hostent *he;
	struct in_addr  addr;
	int             rc;

	inet_aton(ip, &addr);
	he = gethostbyaddr(&addr, sizeof(addr), AF_INET);
	if (he == NULL) {
		M0_LOG(M0_ERROR, "gethostbyaddr err=%d for %s",
		       h_errno, (char*)ip);
		/* Return error code for gethostbyaddr failure */
		return M0_ERR(-h_errno);
	}
	M0_LOG(M0_DEBUG, "ip: %s to hostname: %s\n",ip, he->h_name);
	rc = snprintf(hostname, M0_NET_IP_STRLEN_MAX, "%s", he->h_name);

	if (rc <= 0 || rc >= M0_NET_IP_STRLEN_MAX)
		return M0_ERR(-EINVAL);

	return M0_RC(0);
}


static int parse_prefix(const char *ep_name, const char **prefixes,
			 int nr_prefixes, int *index, int *shift)
{
	int i;

	for (i = 0; i < nr_prefixes; ++i) {
		if (prefixes[i] != NULL) {
			*shift = strnlen(prefixes[i], MAX_PREFIX_STRLEN);
			if (strncmp(ep_name, prefixes[i], *shift) == 0) {
				*index = i;
				break;
			}
		}
	}

	if (i >= nr_prefixes)
		return M0_ERR(-EINVAL);

	return 0;

}

/**
 * This function decodes the inet format address.
 *  The inet address format is of type
 *    <family>:<type>:<ipaddr/hostname_FQDN>@<port>
 *    for example: "inet:tcp:127.0.0.1@3000",
 *                 "inet:verbs:lanl.gov@23",
 *                 "inet6:o2ib:FE80::0202:B3FF:FE1E:8329@6663"
 * Return value: 0 in case of success.
 *               < 0 in case of error.
 */
static int m0_net_ip_inet_parse(const char *name, struct m0_net_ip_addr *addr)
{
	long int     portnum;
	int          shift = 0;
	int          family = 0;
	int          type = 0;
	int          rc;
	char         ip[M0_NET_IP_STRLEN_MAX] = {};
	char         port[M0_NET_IP_PORTLEN_MAX] = {};
	char        *at;
	const char  *ep_name = name;
	char        *end;

	rc = parse_prefix(ep_name, ip_family, ARRAY_SIZE(ip_family), &family,
			  &shift);
	if (rc != 0 || ep_name[shift] != ':')
		return M0_ERR(-EINVAL);
	ep_name += shift + 1;

	rc = parse_prefix(ep_name, ip_protocol, ARRAY_SIZE(ip_protocol), &type,
		     &shift);
	if (rc != 0 || ep_name[shift] != ':')
		return M0_ERR(-EINVAL);
	ep_name += shift + 1;

	at = strchr(ep_name, '@');
	if (at == NULL)
		return M0_ERR(-EINVAL);
	else {
		at++;
		if (at == NULL || !isdigit(at[0]))
			return M0_ERR(-EINVAL);
		strcpy(port, at);
		portnum = strtol(port, &end, 10);
		if (portnum > M0_NET_IP_PORT_MAX)
			return M0_ERR(-EINVAL);
		addr->nia_n.nip_port = (uint16_t)portnum;
	}

	rc = m0_net_hostname_to_ip((char *)ep_name, ip,
				   &addr->nia_n.nip_format);
	if (rc == 0)
		inet_pton(family == M0_NET_IP_AF_INET ? AF_INET : AF_INET6,
			  ip, &addr->nia_n.nip_ip_n.sn[0]);

	/*
	 * To fix codacy warning for strlen, check only if the strnlen exceeds
	 * the array size of addr->nia_p.
	 */
	M0_ASSERT(strnlen(name, ARRAY_SIZE(addr->nia_p) + 1 ) <
		  ARRAY_SIZE(addr->nia_p));
	strcpy(addr->nia_p, name);
	addr->nia_n.nip_fmt_pvt.ia.nia_family = family;
	addr->nia_n.nip_fmt_pvt.ia.nia_type = type;

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
	char            *at = NULL;
	int              nr;
	int              i;
	int              pid;
	int              portal;
	int              portnum;
	int              tmid;
	char             node[M0_NET_IP_STRLEN_MAX] = {};
	char             port[M0_NET_IP_PORTLEN_MAX] = {};
	const char      *ep_name = name;
	uint32_t         nia_n;
	int              shift;
	int              type = 0;
	int              rc;
	bool             is_localhost = false;


	at = strchr(ep_name, '@');
	if (strncmp(ep_name, "0@lo", 4) == 0) {
		nia_n = htonl(INADDR_LOOPBACK);
		inet_ntop(AF_INET, &nia_n, node, ARRAY_SIZE(node));
		is_localhost = true;
	} else {
		if (at == NULL || at - ep_name >= sizeof node)
			return M0_ERR(-EPROTO);

		M0_PRE(sizeof node >= (at-ep_name)+1);
		memcpy(node, ep_name, at - ep_name);
		at++;
		rc = parse_prefix(at, ip_protocol, ARRAY_SIZE(ip_protocol),
				  &type, &shift);
		if (rc != 0)
			return M0_ERR(rc);
	}

	if (at == NULL || (at = strchr(at, ':')) == NULL)
		return M0_ERR(-EPROTO);

	nr = sscanf(at + 1, "%d:%d:%d", &pid, &portal, &tmid);
	if (nr != 3) {
		nr = sscanf(at + 1, "%d:%d:*", &pid, &portal);
		if (nr != 2)
			return M0_ERR(-EPROTO);
		m0_mutex_lock(&autotm_lock);
		for (i = 0; i < IP_AUTOTM_NR; ++i) {
			/*
			 * Start assigning auto-tm indices from the middle of
			 * the bitmap to avoid clashes within UT-s.
			 */
			int probe = (i + IP_AUTOTM_NR / 2) % IP_AUTOTM_NR;
			if (ip_autotm[probe] == 0) {
				tmid = probe;
				/* To handle '*' wildchar as tmid*/
				addr->nia_n.nip_fmt_pvt.la.nla_autotm = true;
				ip_autotm[tmid] = 1;
				break;
			}
		}
		m0_mutex_unlock(&autotm_lock);
		if (i == ARRAY_SIZE(ip_autotm))
			return M0_ERR(-EADDRNOTAVAIL);
	} else
		addr->nia_n.nip_fmt_pvt.la.nla_autotm = false;

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

	addr->nia_n.nip_fmt_pvt.la.nla_portal =  (uint16_t)portal;
	if (portal < 30)
		portal = 30 + portal;

	portnum  = tmid | (1 << 10) | ((portal - 30) << 11);
	if (portnum > M0_NET_IP_PORT_MAX)
		return M0_ERR(-EINVAL);
	snprintf(port, ARRAY_SIZE(port), "%d", (uint32_t)portnum);
	addr->nia_n.nip_format = M0_NET_IP_LNET_FORMAT;
	inet_pton(AF_INET, node, &addr->nia_n.nip_ip_n.sn[0]);
	addr->nia_n.nip_fmt_pvt.la.nla_tmid = (uint16_t)tmid;
	addr->nia_n.nip_port = (uint16_t)portnum;
	addr->nia_n.nip_fmt_pvt.la.nla_type = is_localhost ? 0xFF : type;
	/*
	 * To fix codacy warning for strlen, check only if the strnlen exceeds
	 * the array size of addr->nia_p.
	 */
	M0_ASSERT(strnlen(name, ARRAY_SIZE(addr->nia_p) + 1 ) <
		  ARRAY_SIZE(addr->nia_p));
	strcpy(addr->nia_p, name);

	return M0_RC(0);
}

/**
 * Compare ipv4 address in network byte order.
 */
static bool m0_net_ip_v4_eq(const uint32_t *a1, const uint32_t *a2)
{
	return (a1[0] == a2[0]);
}

/**
 * Compare ipv6 address in network byte order.
 */
static bool m0_net_ip_v6_eq(const uint64_t *a1, const uint64_t *a2)
{
	return (a1[0] == a2[0] && a1[1] == a2[1]);
}

/**
 * Compare lnet address format specific parameters.
 */
static bool m0_net_ip_la_eq(const struct m0_net_ip_addr *a1,
			    const struct m0_net_ip_addr *a2)
{
	const struct m0_net_ip_lnet_addr *la1 = &a1->nia_n.nip_fmt_pvt.la;
	const struct m0_net_ip_lnet_addr *la2 = &a2->nia_n.nip_fmt_pvt.la;

	M0_PRE(a1 != NULL && a2 != NULL);

	return (la1->nla_type   == la2->nla_type   &&
		la1->nla_portal == la2->nla_portal &&
		la1->nla_tmid   == la2->nla_tmid   &&
		la1->nla_autotm == la2->nla_autotm &&
		m0_net_ip_v4_eq(&a1->nia_n.nip_ip_n.sn[0],
				&a2->nia_n.nip_ip_n.sn[0]));
}

/**
 * Compare inet address format specific parameters.
 */
static bool m0_net_ip_ia_eq(const struct m0_net_ip_addr *a1,
			    const struct m0_net_ip_addr *a2)
{
	const struct m0_net_ip_inet_addr *ia1 = &a1->nia_n.nip_fmt_pvt.ia;
	const struct m0_net_ip_inet_addr *ia2 = &a2->nia_n.nip_fmt_pvt.ia;

	M0_PRE(a1 != NULL && a2 != NULL);

	return (ia1->nia_family == ia2->nia_family &&
		ia1->nia_type   == ia2->nia_type &&
		ia1->nia_family == M0_NET_IP_AF_INET ?
		m0_net_ip_v4_eq(&a1->nia_n.nip_ip_n.sn[0],
				&a2->nia_n.nip_ip_n.sn[0]) :
		m0_net_ip_v6_eq(&a1->nia_n.nip_ip_n.ln[0],
				&a2->nia_n.nip_ip_n.ln[0]));
}

M0_INTERNAL int m0_net_ip_parse(const char *name, struct m0_net_ip_addr *addr)
{
	return isdigit(name[0]) ? m0_net_ip_lnet_parse(name, addr) :
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

	M0_ENTRY("frmt=%d nip_ip_n=[0x%" PRIx64 ",0x%" PRIx64 "] port=%d",
		 (int)na->nip_format, na->nip_ip_n.ln[0], na->nip_ip_n.ln[1],
		 (int)na->nip_port);

	if (na->nip_format == M0_NET_IP_LNET_FORMAT)
		M0_LOG(M0_DEBUG, "type=%d portal=%d tmid=%d autotm=%s",
		       (int)na->nip_fmt_pvt.la.nla_type,
		       (int)na->nip_fmt_pvt.la.nla_portal,
		       (int)na->nip_fmt_pvt.la.nla_tmid,
		       na->nip_fmt_pvt.la.nla_autotm ? "true" : "false");
	else
		M0_LOG(M0_DEBUG, "family=%d type=%d",
		       (int)na->nip_fmt_pvt.ia.nia_family,
		       (int)na->nip_fmt_pvt.ia.nia_type);

	if (na->nip_format == M0_NET_IP_LNET_FORMAT) {
		rc = na->nip_fmt_pvt.la.nla_autotm ?
		     snprintf(tmid, ARRAY_SIZE(tmid), "*") :
		     snprintf(tmid, ARRAY_SIZE(tmid), "%d",
			      na->nip_fmt_pvt.la.nla_tmid);
		M0_ASSERT(rc < ARRAY_SIZE(tmid));
		inet_ntop(AF_INET, &na->nip_ip_n.sn[0], ip_p, ARRAY_SIZE(ip_p));
		rc = snprintf(buf, M0_NET_IP_STRLEN_MAX,
			      "%s@%s:12345:%d:%s",
			      na->nip_fmt_pvt.la.nla_type == 0xFF ? "0" : ip_p,
			      na->nip_fmt_pvt.la.nla_type == 0xFF ? "lo" :
			      ((na->nip_fmt_pvt.la.nla_type ==
			      M0_NET_IP_PROTO_TCP) ? "tcp": "o2ib"),
			      na->nip_fmt_pvt.la.nla_portal, tmid);
	} else if (na->nip_format == M0_NET_IP_INET_IP_FORMAT) {
		if (na->nip_fmt_pvt.ia.nia_family != M0_NET_IP_AF_UNIX) {
			inet_ntop(na->nip_fmt_pvt.ia.nia_family ==
				  M0_NET_IP_AF_INET ? AF_INET : AF_INET6,
				  &na->nip_ip_n.sn[0], ip_p, ARRAY_SIZE(ip_p));
			rc = snprintf(buf, M0_NET_IP_STRLEN_MAX, "%s:%s:%s@%d",
				      na->nip_fmt_pvt.ia.nia_family ==
				      M0_NET_IP_AF_INET ? "inet" : "inet6",
				      ip_protocol[na->nip_fmt_pvt.ia.nia_type],
				      ip_p, na->nip_port);
		} else
			M0_LOG(M0_ERROR, "Format is currently not supported");
	} else if (na->nip_format == M0_NET_IP_INET_HOSTNAME_FORMAT) {
		inet_ntop(AF_INET, &na->nip_ip_n.sn[0], ip_p, ARRAY_SIZE(ip_p));
		rc = m0_net_ip_to_hostname(ip_p, hostname);
		if (rc != 0 )
			return M0_ERR(rc);
		rc = snprintf(buf, M0_NET_IP_STRLEN_MAX, "inet:%s:%s@%d",
			      ip_protocol[na->nip_fmt_pvt.ia.nia_type],
			      hostname, na->nip_port);
	}
	if (rc <= 0 || rc >= M0_NET_IP_STRLEN_MAX)
		return M0_ERR(-EINVAL);
	M0_LOG(M0_DEBUG, "Address constructed: %s", buf);

	return 0;
}

M0_INTERNAL int m0_net_hostname_to_ip(const char *hostname, char *ip,
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
		hname = gethostbyname(name);
		if (hname == NULL) {
			M0_LOG(M0_ERROR, "gethostbyname err=%d for %s",
			       h_errno, (char*)name);
			/* Return positive rc for gethostbyname failure */
			return M0_ERR(h_errno);
		}
		addr = (struct in_addr **)hname->h_addr_list;
		for (i = 0; addr[i] != NULL; i++) {
			/* Return the first one. */
			strcpy(ip, inet_ntoa(*addr[i]));
			M0_LOG(M0_DEBUG, "fqdn=%s to ip=%s", (char*)name, ip);
			return M0_RC(0);
		}

		/* If no valid addr structure found, then return error */
		return M0_ERR(-errno);
	}

	return M0_RC(0);
}

M0_INTERNAL bool m0_net_ip_addr_eq(const struct m0_net_ip_addr *addr1,
				   const struct m0_net_ip_addr *addr2,
				   bool is_ncmp)
{
	M0_PRE(addr1 != NULL && addr2 != NULL);
	if (!is_ncmp)
		return (strcmp(addr1->nia_p, addr2->nia_p) == 0);
	else
		return (addr1->nia_n.nip_format == addr2->nia_n.nip_format &&
			addr1->nia_n.nip_port == addr2->nia_n.nip_port     &&
			/* For lnet address compare using m0_net_ip_la_eq(). */
			((addr1->nia_n.nip_format == M0_NET_IP_LNET_FORMAT &&
			m0_net_ip_la_eq(addr1, addr2))                    ||
			/* For inet address compare using m0_net_ip_ia_eq(). */
			(addr1->nia_n.nip_format != M0_NET_IP_LNET_FORMAT  &&
			m0_net_ip_ia_eq(addr1, addr2))));
}

M0_INTERNAL int m0_net_ip_init(void)
{
	m0_mutex_init(&autotm_lock);
	M0_SET_ARR0(ip_autotm);
	return 0;
}

M0_INTERNAL void m0_net_ip_fini(void)
{
	m0_mutex_fini(&autotm_lock);
}

#endif /* __KERNEL__ */

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
