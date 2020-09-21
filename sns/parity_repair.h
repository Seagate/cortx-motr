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

#ifndef __MOTR_SNS_PARITY_REPAIR_H__
#define __MOTR_SNS_PARITY_REPAIR_H__

#include "layout/pdclust.h"
#include "fid/fid.h"
#include "pool/pool.h"

/**
 * Map the {failed device, spare slot} pair of a specified device.
 * @param pm pool machine.
 * @param fid Global file id.
 * @param pl pdclust layout instance.
 * @param group_number Parity group number for a given file.
 * @param unit_number Unit number in the parity group.
 * @param spare_slot_out the output spare slot.
 * @param spare_slot_out_prev the previous spare slot (in case of cascaded
 *        failures) Contains unit number in case of single failure.
 */
M0_INTERNAL int m0_sns_repair_spare_map(struct m0_poolmach *pm,
					const struct m0_fid *fid,
					struct m0_pdclust_layout *pl,
					struct m0_pdclust_instance *pi,
					uint64_t group_number,
					uint64_t unit_number,
					uint32_t *spare_slot_out,
					uint32_t *spare_slot_out_prev);

M0_INTERNAL int m0_sns_repair_spare_rebalancing(struct m0_poolmach *pm,
						const struct m0_fid *fid,
						struct m0_pdclust_layout *pl,
						struct m0_pdclust_instance *pi,
						uint64_t group, uint64_t unit,
						uint32_t *spare_slot_out,
						uint32_t *spare_slot_out_prev);
/**
 * Map the {spare slot, data/parity unit id} pair after repair.
 * @param pm pool machine.
 * @param fid Global file id.
 * @param pl pdclust layout instance.
 * @param group_number Parity group number for a given file.
 * @param unit_number Spare unit number in the parity group.
 * @param data_unit_id_out the output data unit index.
 */
M0_INTERNAL int m0_sns_repair_data_map(struct m0_poolmach *pm,
                                       struct m0_pdclust_layout *pl,
				       struct m0_pdclust_instance *pi,
                                       uint64_t group_number,
                                       uint64_t spare_unit_number,
                                       uint64_t *data_unit_id_out);

#endif /* __MOTR_SNS_PARITY_REPAIR_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
