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

#ifndef __MOTR_ADDB2_PLUGIN_API_H__
#define __MOTR_ADDB2_PLUGIN_API_H__

/**
 * @addtogroup addb2
 *
 * m0addb2dump plugins API.
 *
 * External plugins allow to add custom addb records interpreters.
 * Custom interpreters should have ids (field ii_id of structure m0_id_intrp)
 * from reserved external ranges (addb2/addb2_internal.h).
 *
 * @{
 */

#include <stdint.h>
#include "addb2/addb2_internal.h"

/**
 * This function is called by the m0addb2dump utility.
 * It should return an array of interpreters in the intrp parameter.
 * The last terminating element of the array must have zero-struct { 0 }.
 */
int m0_addb2_load_interps(uint64_t flags, struct m0_addb2__id_intrp **intrp);

/** @} end of addb2 group */

#endif /* __MOTR_ADDB2_PLUGIN_API_H__ */

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
