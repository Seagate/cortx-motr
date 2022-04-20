/* -*- C -*- */
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


#pragma once

#ifndef __MOTR_HA_LINK_FOPS_H__
#define __MOTR_HA_LINK_FOPS_H__

#include "lib/types.h"          /* m0_uint128 */
#include "xcode/xcode_attr.h"   /* M0_XCA_RECORD */
#include "fop/fop.h"            /* m0_fop_type */
#include "ha/msg.h"             /* m0_ha_msg */

#include "lib/types_xc.h"       /* m0_uint128_xc */
#include "ha/msg_xc.h"          /* m0_ha_msg_xc */

/**
 * @defgroup ha
 *
 * @{
 */

struct m0_ha_link_tags {
	uint64_t hlt_confirmed;
	uint64_t hlt_delivered;
	uint64_t hlt_next;
	uint64_t hlt_assign;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_ha_link_msg_fop {
	uint64_t               lmf_msg_nr;
	struct m0_ha_msg       lmf_msg;
	struct m0_uint128      lmf_id_local;
	struct m0_uint128      lmf_id_remote;
	struct m0_uint128      lmf_id_connection;
	uint64_t               lmf_out_next;
	uint64_t               lmf_in_delivered;
	/** @see m0_ha_link::hln_req_fop_seq */
	uint64_t               lmf_seq;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_ha_link_msg_rep_fop {
	int32_t                lmr_rc;
	uint64_t               lmr_out_next;
	uint64_t               lmr_in_delivered;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_ha_link_params {
	struct m0_uint128      hlp_id_local;
	struct m0_uint128      hlp_id_remote;
	struct m0_uint128      hlp_id_connection;
	struct m0_ha_link_tags hlp_tags_local;
	struct m0_ha_link_tags hlp_tags_remote;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#define HLTAGS_F "(confirmed=%" PRIu64 " delivered=%" PRIu64 \
		 " next=%" PRIu64 " assign=%" PRIu64 ")"
#define HLTAGS_P(_tags) (_tags)->hlt_confirmed, (_tags)->hlt_delivered,       \
			(_tags)->hlt_next, (_tags)->hlt_assign

M0_INTERNAL void m0_ha_link_tags_initial(struct m0_ha_link_tags *tags,
                                         bool                    tag_even);
M0_INTERNAL bool m0_ha_link_tags_eq(const struct m0_ha_link_tags *tags1,
                                    const struct m0_ha_link_tags *tags2);
M0_INTERNAL void m0_ha_link_params_invert(struct m0_ha_link_params       *dst,
                                          const struct m0_ha_link_params *src);

extern struct m0_fop_type m0_ha_link_msg_fopt;
extern struct m0_fop_type m0_ha_link_msg_rep_fopt;

M0_INTERNAL int  m0_ha_link_fops_init(void);
M0_INTERNAL void m0_ha_link_fops_fini(void);

/** @} end of ha group */
#endif /* __MOTR_HA_LINK_FOPS_H__ */

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
