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

#ifndef __MOTR_IOSERVICE_FID_CONVERT_H__
#define __MOTR_IOSERVICE_FID_CONVERT_H__

#include "lib/types.h"          /* uint32_t */

/**
 * @defgroup fidconvert
 *
 * @verbatim
 *                     8 bits           24 bits             96 bits
 *                 +-----------------+-----------+---------------------------+
 *    GOB fid      |   GOB type id   |  zeroed   |                           |
 *                 +-----------------+-----------+---------------------------+
 *                                                            ||
 *                                                            \/
 *                 +-----------------+-----------+---------------------------+
 *    COB fid      |   COB type id   | device id |                           |
 *                 +-----------------+-----------+---------------------------+
 *                                        ||                  ||
 *                                        \/                  \/
 *    AD stob      +-----------------+-----------+---------------------------+
 *      fid        | AD stob type id | device id |                           |
 *                 +-----------------+-----------+---------------------------+
 *
 *                       8 bits                96 bits             24 bits
 *                 +-----------------+---------------------------+-----------+
 *     AD stob     | AD stob domain  |          zeroed           | device id |
 *   domain fid    |    type id      |                           |           |
 *                 +-----------------+---------------------------------------+
 *                                                      ||
 *                                                      \/
 *  AD stob domain +-----------------+---------------------------------------+
 *  backing store  |   linux stob    |                                       |
 *    stob fid     |     type id     |                                       |
 *                 +-----------------+---------------------------------------+
 *                       8 bits                     120 bits
 *                 +-----------------+---------------------------------------+
 *     Linux stob  | Linux stob dom  |                                       |
 *   domain fid    |    type id      |        FIXED DOM KEY                  |
 *                 +-----------------+---------------------------------------+
 * @endverbatim
 *
 * Note: ad stob backing store conversion is here because ad stob itself doesn't
 * have limitation on backing store stob fid, but ioservice has such limitation.
 *
 * @{
 */

struct m0_fid;
struct m0_stob_id;

enum {
	M0_FID_DEVICE_ID_BITS     = 24,
	M0_FID_DEVICE_ID_OFFSET   = 32,
	M0_FID_DEVICE_ID_MASK     = ((1ULL << M0_FID_DEVICE_ID_BITS) - 1) <<
				    M0_FID_DEVICE_ID_OFFSET,
	M0_FID_DEVICE_ID_MAX      = (1ULL << M0_FID_DEVICE_ID_BITS) - 1,
	M0_FID_GOB_CONTAINER_MASK = (1ULL << M0_FID_DEVICE_ID_OFFSET) - 1,
	M0_AD_STOB_LINUX_DOM_KEY  = 0xadf11e, /* AD file */
	/** Default cid for fake linuxstob storage. */
	M0_SDEV_CID_DEFAULT       = 1,
};

M0_INTERNAL void m0_fid_gob_make(struct m0_fid *gob_fid,
				 uint32_t       container,
				 uint64_t       key);

M0_INTERNAL void m0_fid_convert_gob2cob(const struct m0_fid *gob_fid,
					struct m0_fid       *cob_fid,
					uint32_t             device_id);
M0_INTERNAL void m0_fid_convert_cob2gob(const struct m0_fid *cob_fid,
					struct m0_fid       *gob_fid);
M0_INTERNAL void m0_fid_convert_cob2stob(const struct m0_fid *cob_fid,
					 struct m0_stob_id   *stob_id);

M0_INTERNAL void m0_fid_convert_cob2adstob(const struct m0_fid *cob_fid,
					   struct m0_stob_id   *stob_id);
M0_INTERNAL void m0_fid_convert_adstob2cob(const struct m0_stob_id *stob_id,
					   struct m0_fid           *cob_fid);
M0_INTERNAL void m0_fid_convert_stob2cob(const struct m0_stob_id   *stob_id,
					 struct m0_fid *cob_fid);
M0_INTERNAL void
m0_fid_convert_bstore2adstob(const struct m0_fid *bstore_fid,
			     struct m0_fid       *stob_domain_fid);
M0_INTERNAL void
m0_fid_convert_adstob2bstore(const struct m0_fid *stob_domain_fid,
			     struct m0_fid       *bstore_fid);

M0_INTERNAL uint32_t m0_fid_cob_device_id(const struct m0_fid *cob_fid);
M0_INTERNAL uint64_t m0_fid_conf_sdev_device_id(const struct m0_fid *sdev_fid);

M0_INTERNAL bool m0_fid_validate_gob(const struct m0_fid *gob_fid);
M0_INTERNAL bool m0_fid_validate_cob(const struct m0_fid *cob_fid);
M0_INTERNAL bool m0_fid_validate_adstob(const struct m0_stob_id *stob_id);
M0_INTERNAL bool m0_fid_validate_bstore(const struct m0_fid *bstore_fid);
M0_INTERNAL bool m0_fid_validate_linuxstob(const struct m0_stob_id *stob_id);

M0_INTERNAL uint32_t m0_fid__device_id_extract(const struct m0_fid *fid);

/** @} end of fidconvert group */
#endif /* __MOTR_IOSERVICE_FID_CONVERT_H__ */

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
