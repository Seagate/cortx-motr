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

#ifndef __MOTR_SCRIPTS_SYSTEMTAP_KEM_KEM_ID_H__
#define __MOTR_SCRIPTS_SYSTEMTAP_KEM_KEM_ID_H__

#include "addb2/identifier.h"

/**
 * @defgroup kem Kernel Event Message ADDB2 definition
 *
 *
 * @{
 */

enum {
	M0_AVI_KEM_CPU = M0_AVI_KEM_RANGE_START + 1,
	M0_AVI_KEM_PAGE_FAULT,
	M0_AVI_KEM_CONTEXT_SWITCH,
};

/** @} end of kem group */

#endif /* __MOTR_SCRIPTS_SYSTEMTAP_KEM_KEM_ID_H__ */

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
