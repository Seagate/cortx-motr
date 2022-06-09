/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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
 * @addtogroup be-ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/ha.h"
#include "ha/msg.h"           /* M0_HA_MSG_BE_IO_ERR */
#include "ha/ha.h"            /* m0_ha_send */
#include "module/instance.h"  /* m0_get */
#include "lib/memory.h"       /* M0_ALLOC_PTR */

M0_INTERNAL void
m0_be_io_err_send(uint32_t errcode, uint8_t location, uint8_t io_opcode)
{
	struct m0_ha_msg *msg;
	uint64_t          tag;

	M0_ENTRY("errcode=%d location=%u io_opcode=%u",
		 errcode, location, io_opcode);
	M0_PRE(errcode > 0);
	M0_PRE(M0_BE_LOC_NONE <= location && location <= M0_BE_LOC_SEGMENT_2);
	M0_PRE(SIO_INVALID <= io_opcode && io_opcode <= SIO_SYNC);

	M0_ALLOC_PTR(msg);
	if (msg == NULL) {
		M0_LOG(M0_ERROR, "m0_ha_msg allocation failed");
	} else {
		*msg = (struct m0_ha_msg){
			.hm_time = m0_time_now(),
			.hm_data = {
				.hed_type = M0_HA_MSG_BE_IO_ERR,
				.u.hed_be_io_err = (struct m0_be_io_err){
					.ber_errcode   = errcode,
					.ber_location  = location,
					.ber_io_opcode = io_opcode
				}
			}
		};
		m0_ha_send(m0_get()->i_ha, m0_get()->i_ha_link, msg, &tag);
		m0_free(msg);
	}
	/*
	 * Enter infinite loop.
	 *
	 * If you ever decide to delete this loop, update the function
	 * documentation.
	 */
	while (1)
		m0_nanosleep(m0_time(1, 0), NULL);
}

#undef M0_TRACE_SUBSYSTEM
/** @} be-ha */
