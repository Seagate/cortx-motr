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
#include <stdlib.h>             /* atoi */

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
 * This function decodes the inet format address.
 *  The inet address format is of type
 *    <family>:<type>:<ipaddr/hostname_FQDN>@<port>
 *    for example: "inet:tcp:127.0.0.1@3000",
 *                 "inet:stream:lanl.gov@23",
 *                 "inet6:dgram:FE80::0202:B3FF:FE1E:8329@6663"
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
		addr->na_port = (uint16_t)portnum;
	}

	rc = m0_net_hostname_to_ip((char *)ep_name, ip, &addr->na_format);
	if (rc == 0)
		inet_pton(f == M0_NET_IP_AF_INET ? AF_INET : AF_INET6,
			  ip, &addr->na_n.sn[0]);

	M0_ASSERT(strlen(name) < ARRAY_SIZE(addr->na_p));
	strcpy(addr->na_p, name);
	addr->na_addr.ia.nia_family = f;
	addr->na_addr.ia.nia_type = s;

	/* Ignore the error due to gethostbyname() as it will be retried. */
	return rc >=0 ? M0_RC(0) : M0_ERR(rc);
}

/**
 * This function decodes the lnet format address and extracts the ip address and
 * port number from it.
 * This is also used to allocate unique transfer machine identifiers for LNet
 * network addresses with wildcard transfer machine identifier (like
 * "192.168.96.128@tcp:12345:31:*").
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
	char         port[6] = {};
	const char  *ep_name = name;
	uint32_t     na_n;
	int          shift;
	int          s;

	at = strchr(ep_name, '@');
	if (strncmp(ep_name, "0@lo", 4) == 0) {
		na_n = htonl(INADDR_LOOPBACK);
		inet_ntop(AF_INET, &na_n, node, ARRAY_SIZE(node));
	} else {
		if (at == NULL || at - ep_name >= sizeof node)
			return M0_ERR(-EPROTO);

		M0_PRE(sizeof node >= (at-ep_name)+1);
		memcpy(node, ep_name, at - ep_name);
	}
	at++;

	for (s = 0; s < ARRAY_SIZE(ip_protocol); s++) {
		if (ip_protocol[s] != NULL) {
			shift = strlen(ip_protocol[s]);
			if (strncmp(at, ip_protocol[s], shift) == 0)
			{
				addr->na_addr.la.nla_type = s;
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
				break;
			}
		}
		if (i == ARRAY_SIZE(ip_autotm))
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

	addr->na_addr.la.nla_portal =  portal;
	if ( portal < 30)
		portal = 30 + portal;

	portnum  = tmid | (1 << 10) | ((portal - 30) << 11);
	M0_ASSERT(portnum < M0_NET_IP_PORT_MAX);
	sprintf(port, "%d", (int)portnum);
	ip_autotm[tmid] = 1;

	addr->na_format = M0_NET_IP_LNET_FORMAT;
	inet_pton(AF_INET, node, &addr->na_n.sn[0]);
	addr->na_addr.la.nla_tmid = tmid;
	addr->na_port = (uint16_t)portnum;
	M0_ASSERT(strlen(name) < ARRAY_SIZE(addr->na_p));
	strcpy(addr->na_p, name);

	return M0_RC(0);
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
	strncpy(name, hostname, n);

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
			/** Return the first one. */
			strcpy(ip, inet_ntoa(*addr[i]));
			M0_LOG(M0_DEBUG, "fqdn=%s ip=%s", (char*)name, ip);
			return M0_RC(0);
		}

		/* If no valid addr structure found, then return error */
		return M0_ERR(-errno);
	}

	return M0_RC(0);
}

M0_UNUSED int m0_net_ip_print(const struct m0_net_ip_addr *na, char *buf,
			      uint32_t len)
{
	char *star = NULL;
	char  node[M0_NET_IP_STRLEN_MAX] = {};

	M0_LOG(M0_DEBUG, "str=%s frmt=%d num=[0x%"PRIx64",0x%"PRIx64"] port=%d",
		(char*)na->na_p, (int)na->na_format, na->na_n.ln[0],
		na->na_n.ln[1], (int)na->na_port);
	if (na->na_format == M0_NET_IP_LNET_FORMAT)
		M0_LOG(M0_DEBUG, "type=%d portal=%d tmid=%d",
			(int)na->na_addr.la.nla_type,
			(int)na->na_addr.la.nla_portal,
			(int)na->na_addr.la.nla_tmid);
	else
		M0_LOG(M0_DEBUG, "family=%d type=%d port=%d",
			(int)na->na_addr.ia.nia_family,
			(int)na->na_addr.ia.nia_type,
			(int)na->na_port);

	if (na->na_format == M0_NET_IP_LNET_FORMAT) {
		inet_ntop(AF_INET, &na->na_n.sn[0], node, ARRAY_SIZE(node));

		star = strchr(na->na_p, '*');
		if (star != NULL) {
			sprintf(buf, "%s@%s:12345:%d:*",
				na->na_addr.la.nla_type == M0_NET_IP_PROTO_LO ?
				"0" : node,
				na->na_addr.la.nla_type == M0_NET_IP_PROTO_LO ?
				"lo" :
				((na->na_addr.la.nla_type ==
				  M0_NET_IP_PROTO_TCP) ? "tcp": "o2ib"),
				  na->na_addr.la.nla_portal);
		} else {
			sprintf(buf, "%s@%s:12345:%d:%d",
				na->na_addr.la.nla_type == M0_NET_IP_PROTO_LO ?
				"0" : node,
				na->na_addr.la.nla_type == M0_NET_IP_PROTO_LO ?
				"lo" :
				((na->na_addr.la.nla_type ==
				  M0_NET_IP_PROTO_TCP) ? "tcp": "o2ib"),
				  na->na_addr.la.nla_portal,
				  na->na_addr.la.nla_tmid);
		}
	} else if (na->na_format == M0_NET_IP_INET_IP_FORMAT) {
		if (na->na_addr.ia.nia_family == M0_NET_IP_AF_INET) {
			inet_ntop(AF_INET, &na->na_n.sn[0], node,
				  ARRAY_SIZE(node));
			sprintf(buf, "inet:%s:%s@%d",
				ip_protocol[na->na_addr.ia.nia_type],
				node, na->na_port);
		} else if (na->na_addr.ia.nia_family == M0_NET_IP_AF_INET6) {
			inet_ntop(AF_INET6, &na->na_n, node, ARRAY_SIZE(node));
			sprintf(buf, "inet6:%s:%s@%d",
				ip_protocol[na->na_addr.ia.nia_type],
				node, na->na_port);
		} else if (na->na_addr.ia.nia_family == M0_NET_IP_AF_UNIX) {
			M0_LOG(M0_DEBUG, "Format is currently not supported");
		}
	} else if (na->na_format == M0_NET_IP_INET_HOSTNAME_FORMAT) {
		M0_ASSERT(len >= strlen(buf));
		sprintf(buf, "%s", na->na_p);
	}

	return 0;
}

M0_INTERNAL int m0_net_ip_parse(const char *name, struct m0_net_ip_addr *addr)
{
	M0_PRE(name != NULL);
	return (name[0] >= '0' && name[0] <= '9') ?
		m0_net_ip_lnet_parse(name, addr) :
		m0_net_ip_inet_parse(name, addr);
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
