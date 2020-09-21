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


#include "ioservice/fid_convert.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_IOSERVICE
#include "lib/trace.h"
#include "lib/assert.h"         /* M0_PRE */

#include "fid/fid.h"            /* m0_fid */
#include "file/file.h"          /* m0_file_fid_type */
#include "cob/cob.h"            /* m0_cob_fid_type */

/**
 * @addtogroup fidconvert
 *
 * @{
 */

/* extract bits [32, 56) from fid->f_container */
M0_INTERNAL uint32_t m0_fid__device_id_extract(const struct m0_fid *fid)
{
	return (fid->f_container & M0_FID_DEVICE_ID_MASK) >>
	       M0_FID_DEVICE_ID_OFFSET;
}

M0_INTERNAL void m0_fid_gob_make(struct m0_fid *gob_fid,
				 uint32_t       container,
				 uint64_t       key)
{
	m0_fid_tset(gob_fid, m0_file_fid_type.ft_id, container, key);

	M0_POST(m0_fid_validate_gob(gob_fid));
}

M0_INTERNAL void m0_fid_convert_gob2cob(const struct m0_fid *gob_fid,
					struct m0_fid       *cob_fid,
					uint32_t             device_id)
{
	M0_PRE(m0_fid_validate_gob(gob_fid));
	M0_PRE(device_id <= M0_FID_DEVICE_ID_MAX);

	*cob_fid = *gob_fid;
	m0_fid_tassume(cob_fid, &m0_cob_fid_type);
	cob_fid->f_container |= (uint64_t)device_id << M0_FID_DEVICE_ID_OFFSET;

	M0_POST(m0_fid_validate_cob(cob_fid));
}

M0_INTERNAL void m0_fid_convert_cob2gob(const struct m0_fid *cob_fid,
					struct m0_fid       *gob_fid)
{
	M0_PRE(m0_fid_validate_cob(cob_fid));

	m0_fid_tset(gob_fid, m0_file_fid_type.ft_id,
		    cob_fid->f_container & M0_FID_GOB_CONTAINER_MASK,
		    cob_fid->f_key);

	M0_POST(m0_fid_validate_gob(gob_fid));
}

M0_INTERNAL uint32_t m0_fid_cob_device_id(const struct m0_fid *cob_fid)
{
	M0_PRE(m0_fid_validate_cob(cob_fid));

	return m0_fid__device_id_extract(cob_fid);
}

M0_INTERNAL uint64_t m0_fid_conf_sdev_device_id(const struct m0_fid *sdev_fid)
{
	return sdev_fid->f_key & ((1ULL << M0_FID_DEVICE_ID_BITS) - 1);
}

M0_INTERNAL bool m0_fid_validate_gob(const struct m0_fid *gob_fid)
{
	return m0_fid_tget(gob_fid) == m0_file_fid_type.ft_id &&
	       m0_fid__device_id_extract(gob_fid) == 0;
}

M0_INTERNAL bool m0_fid_validate_cob(const struct m0_fid *cob_fid)
{
	return m0_fid_tget(cob_fid) == m0_cob_fid_type.ft_id;
}

#undef M0_TRACE_SUBSYSTEM
/** @} end of fidconvert group */

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
