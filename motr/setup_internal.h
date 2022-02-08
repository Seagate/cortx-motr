/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_SETUP_INTERNAL_H__
#define __MOTR_SETUP_INTERNAL_H__

#include "motr/setup.h"

/* import */
struct m0_storage_devs;

/**
   @addtogroup m0d
   @{
 */

/** Represents list of buffer pools in the motr context. */
struct cs_buffer_pool {
	/** Network buffer pool object. */
	struct m0_net_buffer_pool cs_buffer_pool;
	/** Linkage into network buffer pool list. */
	struct m0_tlink           cs_bp_linkage;
	uint64_t                  cs_bp_magic;
};

M0_INTERNAL int cs_service_init(const char *name, struct m0_reqh_context *rctx,
				struct m0_reqh *reqh, struct m0_fid *fid);
M0_INTERNAL void cs_service_fini(struct m0_reqh_service *service);

/** Uses confc API to generate CLI arguments, understood by _args_parse(). */
M0_INTERNAL int cs_conf_to_args(struct cs_args *dest, struct m0_conf_root *r);

M0_INTERNAL int cs_conf_storage_init(struct cs_stobs        *stob,
				     struct m0_storage_devs *devs,
				     bool                    force);

M0_INTERNAL int cs_conf_device_reopen(struct m0_poolmach *pm,
				      struct cs_stobs *stob, uint32_t dev_id);

M0_INTERNAL int cs_conf_services_init(struct m0_motr *cctx);

M0_INTERNAL int cs_conf_get_parition_dev (struct cs_stobs      *stob,
					  struct m0_conf_sdev **sdev,
					  uint32_t             *dev_count,
					  bool ioservice);
/** @} endgroup m0d */
#endif /* __MOTR_SETUP_INTERNAL_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
