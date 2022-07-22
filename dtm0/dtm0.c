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

/**
 * @addtogroup dtm0
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM0
#include "lib/trace.h"

#include "dtm0/dtm0.h"
#include "dtm0/pruner.h"
#include "lib/memory.h" /* M0_ALLOC_ARR */
#include "lib/errno.h" /* ENOMEM */

M0_INTERNAL bool m0_dtx0_id_eq(const struct m0_dtx0_id *left,
			       const struct m0_dtx0_id *right)
{
	return m0_dtx0_id_cmp(left, right) == 0;
}

M0_INTERNAL int m0_dtx0_id_cmp(const struct m0_dtx0_id *left,
			       const struct m0_dtx0_id *right)
{
	return M0_3WAY(left->dti_timestamp, right->dti_timestamp) ?:
		m0_fid_cmp(&left->dti_originator_sdev_fid,
			   &right->dti_originator_sdev_fid);
}

static int descriptor_copy(struct m0_dtx0_descriptor *dst,
			   const struct m0_dtx0_descriptor *src)
{
	struct m0_fid *dst_arr;
	struct m0_fid *src_arr = src->dtd_participants.dtpa_participants;
	uint64_t       nr = src->dtd_participants.dtpa_participants_nr;

	if (nr == 0) {
		M0_SET0(dst);
		return 0;
	}

	M0_ALLOC_ARR(dst_arr, nr);
	if (dst_arr == NULL)
		return M0_ERR(-ENOMEM);

	memcpy(dst_arr, src_arr, sizeof(*dst_arr) * nr);

	dst->dtd_participants.dtpa_participants = dst_arr;
	dst->dtd_participants.dtpa_participants_nr = nr;
	return 0;
}

static int buf2bufs_copy(struct m0_bufs *dst, const struct m0_buf *src)
{
	struct m0_buf *dst_buf;
	int            rc;

	M0_ALLOC_PTR(dst_buf);
	if (dst_buf == NULL)
		return M0_ERR(-ENOMEM);
	rc = m0_buf_copy(dst_buf, src);
	if (rc != 0) {
		m0_free(dst_buf);
		return M0_ERR(rc);
	}
	dst->ab_count = 1;
	dst->ab_elems = dst_buf;
	return 0;
}

M0_INTERNAL int
m0_dtm0_redo_init(struct m0_dtm0_redo *redo,
		  const struct m0_dtx0_descriptor *descriptor,
		  const struct m0_buf             *payload,
		  enum m0_dtx0_payload_type        type)
{
	int rc;

	redo->dtr_payload.dtp_type = type;
	rc = descriptor_copy(&redo->dtr_descriptor, descriptor) ?:
		buf2bufs_copy(&redo->dtr_payload.dtp_data, payload);
	if (rc != 0)
		m0_dtm0_redo_fini(redo);
	return rc;
}

M0_INTERNAL void m0_dtm0_redo_fini(struct m0_dtm0_redo *redo)
{
	m0_free(redo->dtr_descriptor.dtd_participants.dtpa_participants);
	m0_bufs_free(&redo->dtr_payload.dtp_data);
}

M0_INTERNAL int  m0_dtm0_mod_init(void)
{
	/* TODO: Register stype here (and other types). */
	m0_dtm0_pruner_mod_init();
	return 0;
}

M0_INTERNAL void m0_dtm0_mod_fini(void)
{
	m0_dtm0_pruner_mod_fini();
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of dtm0 group */

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
