/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_DIX_FID_CONVERT_H__
#define __MOTR_DIX_FID_CONVERT_H__

#include "lib/types.h"          /* uint32_t */

/**
 * @addtogroup dix
 *
 *  Fids of component catalogues are build in the same manner as it is done for
 *  cobs: component catalogue fid is a combination of distributed index fid with
 *  'component catalogue' type and target storage id.
 *
 * @verbatim
 *                     8 bits           24 bits             96 bits
 *                 +-----------------+-----------+---------------------------+
 *    DIX fid      |   DIX type id   |  zeroed   |                           |
 *                 +-----------------+-----------+---------------------------+
 *                                                            ||
 *                                                            \/
 *                 +-----------------+-----------+---------------------------+
 *    CCTG fid     |   CCTG type id  | device id |                           |
 *                 +-----------------+-----------+---------------------------+
 * @endverbatim
 *
 * @{
 */

struct m0_fid;

enum {
	M0_DIX_FID_DEVICE_ID_BITS     = 24,
	M0_DIX_FID_DEVICE_ID_OFFSET   = 32,
	M0_DIX_FID_DEVICE_ID_MASK     = ((1ULL << M0_DIX_FID_DEVICE_ID_BITS) - 1)
					<< M0_DIX_FID_DEVICE_ID_OFFSET,
	M0_DIX_FID_DEVICE_ID_MAX      = (1ULL << M0_DIX_FID_DEVICE_ID_BITS) - 1,
	M0_DIX_FID_DIX_CONTAINER_MASK = (1ULL << M0_DIX_FID_DEVICE_ID_OFFSET)
					- 1,
};

M0_INTERNAL void m0_dix_fid_dix_make(struct m0_fid *dix_fid,
				     uint32_t       container,
				     uint64_t       key);

M0_INTERNAL void m0_dix_fid_convert_dix2cctg(const struct m0_fid *dix_fid,
					     struct m0_fid       *cctg_fid,
					     uint32_t             device_id);
M0_INTERNAL void m0_dix_fid_convert_cctg2dix(const struct m0_fid *cctg_fid,
					     struct m0_fid       *dix_fid);

M0_INTERNAL uint32_t m0_dix_fid_cctg_device_id(const struct m0_fid *cctg_fid);

M0_INTERNAL bool m0_dix_fid_validate_dix(const struct m0_fid *dix_fid);
M0_INTERNAL bool m0_dix_fid_validate_cctg(const struct m0_fid *cctg_fid);

M0_INTERNAL uint32_t m0_dix_fid__device_id_extract(const struct m0_fid *fid);

M0_INTERNAL void m0_dix_fid__device_id_set(struct m0_fid *fid,
					   uint32_t       dev_id);
/** @} end of dix group */
#endif /* __MOTR_DIX_FID_CONVERT_H__ */

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
