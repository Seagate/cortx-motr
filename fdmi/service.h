/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_FDMI_FDMI_SERVICE_H__
#define __MOTR_FDMI_FDMI_SERVICE_H__

#include "reqh/reqh_service.h"
#include "fdmi/source_dock_internal.h"
#include "fdmi/plugin_dock_internal.h"
#include "fdmi/filterc.h"
/**
 * @addtogroup fdmi_main
 * @see @ref FDMI-DLD-fspec "FDMI Functional Specification"
 * @see @ref reqh
 *
 * @{
 *
 * FDMI service runs as a part of Motr instance. FDMI service stores context
 * data for both FDMI source dock and FDMI plugin dock. FDMI service is
 * initialized and started on Motr instance start up, FDMI Source dock and FDMI
 * plugin dock are managed separately, and specific API is provided for this
 * purposes.
 *
 */

struct m0_reqh_fdmi_svc_params {
	/* FilterC operations can be patched by UT */
	const struct m0_filterc_ops *filterc_ops;
};

/**
 * Structure contains generic service structure and
 * service specific information.
 */
struct m0_reqh_fdmi_service {
	/** Generic reqh service object */
	struct m0_reqh_service  rfdms_gen;

	/**
	 * @todo Temporary field to indicate
	 * whether source dock was successfully started. (phase 2)
	 */
	bool                    rfdms_src_dock_inited;

	/** Magic to check fdmi service object */
	uint64_t                rfdms_magic;
};

M0_INTERNAL void m0_fdms_unregister(void);
M0_INTERNAL int m0_fdms_register(void);

/** @} end of addtogroup fdmi_main */

#endif /* __MOTR_FDMI_FDMI_SERVICE_H__ */
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
