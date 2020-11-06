/* -*- C -*- */
/*
 * Copyright (c) 2019-2020 Seagate Technology LLC and/or its Affiliates
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

#pragma once

#ifndef __MOTR_NET_LIBFAB_XCODE_H__
#define __MOTR_NET_LIBFAB_XCODE_H__

#ifndef __KERNEL__
#include <sys/types.h>
#include <netinet/in.h>                    /* INET_ADDRSTRLEN */
#include <netinet/ip.h>
#include <arpa/inet.h>                     /* inet_pton, htons */
#else
#include <linux/in.h>
#include <linux/in6.h>
#endif

#include "xcode/xcode_attr.h"

/**
 * libfabric module binary structures
 * -----------------------------
 */

/** Peer address. */
struct fab_addr {
	uint32_t        a_family;
	uint32_t        a_socktype;
	uint32_t        a_protocol;
	uint32_t        a_port;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** @} end of netlibfab group */
#endif /* __MOTR_NET_LIBFAB_XCODE_H__ */

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
