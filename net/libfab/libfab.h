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

// #define FI_MAJOR_VERSION 1
// #define FI_MINOR_VERSION 1

struct libfab_fab_params {
	struct fi_info    *fi;      /* Fabric info */
	struct fi_info    *hints;   /* Fabric info to configure flags */
	struct fid_fabric *fabric;  /* Fabric fid */
	struct fid_domain *domain;  /* Domain fid */
};
struct libfab_buf_params {
	struct fid_mr     *mr; 	/* Memory region to be registered */
};

struct libfab_ep_params {
	struct fi_ep   *ep;      /* Endpoint */
	struct fi_pep  *pep;     /* Passive endpoint */
	struct fi_av   *av;      /* Address vector */
	struct fi_eq   *eq;      /* Event queue */
	struct fi_cq   *tx_cq;   /* Transmit Completion Queue */
	struct fi_cq   *rx_cq;   /* Recv Completion Queue */
	struct fi_cntr *tx_cntr; /* Transmit Counter */
	struct fi_cntr *rx_cntr; /* Recv Counter */
};

struct libfab_ep_res {
	struct fi_cq_attr   *cq_attr;   /* Completion Queue attributes */
	struct fi_av_attr   *av_attr;   /* Address Vector attributes */
	struct fi_eq_attr   *eq_attr;   /* Event Queue attributes */
	struct fi_cntr_attr *cntr_attr; /* Counter attributes */
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

