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

#define M0_NET_IP_STRLEN_MAX  100
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
	M0_NET_IP_AF_MAX
};

/** Represent protocol of network address */
enum m0_net_ip_proto {
	M0_NET_IP_PROTO_TCP,
	M0_NET_IP_PROTO_VERBS,
	M0_NET_IP_PROTO_STREAM,
	M0_NET_IP_PROTO_DGRAM,
	M0_NET_IP_PROTO_O2IB,
	M0_NET_IP_PROTO_LO,
	M0_NET_IP_PROTO_MAX
};

/** Structure to represent inet address format */
struct m0_net_ip_inet_addr {
	uint16_t nia_family;   /* AF_INET, AF_INET6, AF_UNIX */
	uint16_t nia_type;     /* tcp, verbs, stream, dgram, o2ib, lo */
};

/** Structure to represent lnet address format */
struct m0_net_ip_lnet_addr {
	uint16_t nla_type;     /* tcp, o2ib, lo */
	uint16_t nla_portal;
	uint16_t nla_tmid;
};

/** Genereic structure to store values related to network address */
struct m0_net_ip_addr {
	union {
		struct m0_net_ip_inet_addr ia;
		struct m0_net_ip_lnet_addr la;
	} na_addr;
	union {
		uint64_t ln[2];
		uint32_t sn[4];
	} na_n;
	enum m0_net_ip_format      na_format;
	uint16_t                   na_port;
	char                       na_p[M0_NET_IP_STRLEN_MAX];
};

/**
 * This function decodes addresses based on address format type
 * such as lnet or inet address format.
 */
int m0_net_ip_parse(const char *name, struct m0_net_ip_addr *addr);

/**
 * This function returns printable ip address format.
 */
int m0_net_ip_print(const struct m0_net_ip_addr *nia, char *buf, uint32_t len);

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
