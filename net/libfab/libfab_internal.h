/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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
#include <netinet/in.h>                    /* INET_ADDRSTRLEN */

#include "rdma/fi_eq.h"
#include "rdma/fi_domain.h"
#include "rdma/fi_endpoint.h"
#include "rdma/fabric.h"

extern struct m0_net_xprt m0_net_libfab_xprt;

/**
 * @defgroup netlibfab
 *
 * @{
 */

struct m0_fab__tm {
	struct m0_net_transfer_mc *ftm_ma;     /* Generic transfer machine */
	struct m0_thread           ftm_poller; /* Poller thread */
	int                        ftm_epollfd;/* epoll(2) file descriptor. */
	struct fid_poll           *ftm_pollset;
	struct fid_wait           *ftm_waitset;
	struct m0_tl               ftm_fin_ep; /* List of finalised ep */
	bool                       ftm_close;  /* tm Shutdown flag */
};

struct libfab_params {
	struct fid_mr     *mr; 	/* Memory region to be registered */
};

struct m0_fab__ep_name {
	char fen_addr[INET6_ADDRSTRLEN];       /*  */
	char fen_port[6];                      /* Port range 0-65535 */
};

struct m0_fab__ep_res {
	struct fid_av      *fer_av;            /* Address vector */
	struct fi_av_attr   fer_av_attr;       /* Address Vector attributes */
	struct fid_eq      *fer_eq;            /* Event queue */
	struct fi_eq_attr   fer_eq_attr;       /* Event Queue attributes */
	struct fid_cq      *fer_tx_cq;         /* Transmit Completion Queue */
	struct fid_cq      *fer_rx_cq;         /* Recv Completion Queue */
	struct fi_cq_attr   fer_cq_attr;       /* Completion Queue attributes */
	struct fid_cntr    *fer_tx_cntr;       /* Transmit Counter */
	struct fid_cntr    *fer_rx_cntr;       /* Recv Counter */
	struct fi_cntr_attr fer_cntr_attr;     /* Counter attributes */
};

struct m0_fab__ep {
	struct m0_net_end_point  fep_nep;      /* linked into a per-tm list */
	struct m0_fab__ep_name   fep_name;     /* "addr:port" in str format */
	struct fi_info          *fep_fi;       /* Fabric interface info */
	struct fid_fabric       *fep_fabric;   /* Fabric fid */
	struct fid_domain       *fep_domain;   /* Domain fid */
	struct fid_ep           *fep_ep;       /* Endpoint */
	struct fid_pep          *fep_pep;      /* Passive endpoint */
	struct m0_fab__ep_res    fep_ep_res;
};

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

