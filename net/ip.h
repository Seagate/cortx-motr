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

#ifndef __MOTR_NET_IP_H__
#define __MOTR_NET_IP_H__

#define M0_NET_IP_STRLEN_MAX  255
#define M0_NET_IP_PORTLEN_MAX 6
#define M0_NET_IP_PORT_MAX    65536

/** Represents format of network address */
enum m0_net_ip_format {
	M0_NET_IP_INET_IP_FORMAT,
	M0_NET_IP_INET_HOSTNAME_FORMAT,
	M0_NET_IP_LNET_FORMAT,
	M0_NET_IP_FORMAT_MAX
};

/** Represents family of network address */
enum m0_net_ip_family {
	M0_NET_IP_AF_INET,
	M0_NET_IP_AF_INET6,
	M0_NET_IP_AF_UNIX,
	M0_NET_IP_AF_NR
};

/** Represent protocol of network address */
enum m0_net_ip_proto {
	M0_NET_IP_PROTO_TCP,
	M0_NET_IP_PROTO_UDP,
	M0_NET_IP_PROTO_VERBS,
	M0_NET_IP_PROTO_O2IB,
	M0_NET_IP_PROTO_NR
};

/** Structure to represent inet address format */
struct m0_net_ip_inet_addr {
	uint16_t nia_family;   /* AF_INET, AF_INET6, AF_UNIX */
	uint16_t nia_type;     /* tcp, udp, verbs, o2ib */
};

/** Structure to represent lnet address format */
struct m0_net_ip_lnet_addr {
	uint16_t nla_type;     /* tcp, o2ib */
	uint16_t nla_portal;
	uint16_t nla_tmid;
	bool     nla_autotm;
};

/** Structure for network address parameters */
struct m0_net_ip_params {
	enum m0_net_ip_format      nip_format;
	union {
		uint64_t ln[2];
		uint32_t sn[4];
	} nip_ip_n;
	uint16_t                   nip_port;
	union {
		struct m0_net_ip_inet_addr ia;
		struct m0_net_ip_lnet_addr la;
	} nip_fmt_pvt;
};

/** Generic structure to store values related to network address */
struct m0_net_ip_addr {
	struct m0_net_ip_params  nia_n;
	char                     nia_p[M0_NET_IP_STRLEN_MAX];
};

/**
 * This function decodes addresses based on address format type
 * such as lnet or inet address format.
 * Example:
 *        lnet format: <ip>@<type>:<pid=12345>:<portal>:<tmid>
 *                     for example:"192.168.96.128@tcp:12345:34:1"
 *        inet format: <family>:<type>:<ipaddr/hostname_FQDN>@<port>
 *                     for example: "inet:tcp:127.0.0.1@3000"
 * Return value: 0 in case of success.
 *               < 0 in case of error.
 */
M0_INTERNAL int m0_net_ip_parse(const char *name, struct m0_net_ip_addr *addr);

/**
 * This function generates printable address format from
 * struct m0_net_ip_addr::nia_n into struct m0_net_ip_addr::nia_p.
 */
M0_INTERNAL int m0_net_ip_print(const struct m0_net_ip_addr *nia);

/**
 * This function convert the hostname/FQDN format to ip format.
 * Here hostname can be FQDN or local machine hostname.
 * Return value:  = 0 in case of succesful fqdn to ip resolution.
 *                > 0 when gethostbyname fails (dns resolution failed).
 *                < 0 error.
 */
M0_INTERNAL int m0_net_hostname_to_ip(const char *hostname, char *ip,
				      enum m0_net_ip_format *fmt);

/**
 * This function is used to compare the fields in struct m0_net_ip_addr.
 * If is_ncmp is true then numeric address fields are compared else string addr
 * is compared.
 * Returns true if addr1 and addr2 are equal else false.
 */
M0_INTERNAL bool m0_net_ip_addr_eq(const struct m0_net_ip_addr *addr1,
				   const struct m0_net_ip_addr *addr2, bool is_ncmp);

M0_INTERNAL int m0_net_ip_init(void);

M0_INTERNAL void m0_net_ip_fini(void);

#endif /* __MOTR_NET_IP_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */