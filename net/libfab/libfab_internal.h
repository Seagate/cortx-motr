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

extern struct m0_net_xprt m0_net_libfab_xprt;

/**
 * @defgroup netlibfab
 *
 * @{
 */

struct m0_fab__dom_param {
	struct fi_info    *fab_fi;      /* Fabric info */
	struct fi_info    *fab_hints;   /* Fabric info to configure flags */
	struct fid_fabric *fab_fabric;  /* Fabric fid */
	struct fid_domain *fab_domain;  /* Domain fid */
};

struct libfab_params {
	struct fid_mr     *mr; 	/* Memory region to be registered */
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

