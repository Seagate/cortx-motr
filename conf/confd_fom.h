/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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
#ifndef __MOTR_CONFD_FOM_H__
#define __MOTR_CONFD_FOM_H__

#include "fop/fom.h"  /* m0_fom */

/**
 * @addtogroup confd_dfspec
 *
 * @{
 */

struct m0_confd_fom {
	struct m0_fom dm_fom;
};

M0_INTERNAL int m0_confd_fom_create(struct m0_fop   *fop,
				    struct m0_fom  **out,
				    struct m0_reqh  *reqh);

/** @} confd_dfspec */
#endif /* __MOTR_CONFD_FOM_H__ */
