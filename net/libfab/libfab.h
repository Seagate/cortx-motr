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


#pragma once

#ifndef __MOTR_NET_LIBFAB_H__
#define __MOTR_NET_LIBFAB_H__

#include "rdma/fi_eq.h"
#include "rdma/fi_domain.h"
#include "rdma/fi_endpoint.h"
#include "rdma/fabric.h"

/**
 * @defgroup netlibfab
 *
 * @{
 */

struct libfab_ct {
	struct fi_info *fi_pep, *fi, *hints;
	struct fid_fabric *fabric;
	struct fid_domain *domain;
	struct fid_pep *pep;
	struct fid_ep *ep;
	struct fid_cq *txcq, *rxcq;
	struct fid_mr *mr;
	struct fid_av *av;
	struct fid_eq *eq;
#if 0
	struct fid_mr no_mr;
	void *tx_ctx_ptr, *rx_ctx_ptr;
	struct fi_context tx_ctx[2], rx_ctx[2];
	uint64_t remote_cq_data;

	uint64_t tx_seq, rx_seq, tx_cq_cntr, rx_cq_cntr;

	fi_addr_t local_fi_addr, remote_fi_addr;
	void *buf, *tx_buf, *rx_buf;
	size_t buf_size, tx_size, rx_size;
	size_t rx_prefix_size, tx_prefix_size;

	int timeout_sec;
	uint64_t start, end;

	struct fi_av_attr av_attr;
	struct fi_eq_attr eq_attr;
	struct fi_cq_attr cq_attr;
	struct pp_opts opts;

	long cnt_ack_msg;

	SOCKET ctrl_connfd;
	char ctrl_buf[PP_CTRL_BUF_LEN + 1];

	void *local_name, *rem_name;
#endif
};

/** @} end of netlibfab group */
#endif /* __MOTR_NET_LIBFAB_H__ */

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

