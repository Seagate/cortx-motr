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


#pragma once

#ifndef __MOTR_STOB_AD_PRIVATE_H__
#define __MOTR_STOB_AD_PRIVATE_H__

#include "fid/fid.h"       /* m0_fid */
#include "fid/fid_xc.h"    /* m0_fid_xc */
#include "be/extmap.h"     /* m0_be_emap_seg */
#include "be/extmap_xc.h"
#include "stob/stob.h"     /* m0_stob_id */
#include "stob/stob_xc.h"  /* m0_stob_id */

struct m0_stob_domain;
struct m0_stob_ad_domain;

struct stob_ad_0type_rec {
	struct m0_format_header   sa0_header;
	/* XXX pointer won't work with be_segment migration */
	struct m0_stob_ad_domain *sa0_ad_domain;
	struct m0_format_footer   sa0_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct stob_ad_rec_frag_seg {
	uint32_t                ps_segments;
	struct m0_be_emap_seg  *ps_old_data;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(be);

struct stob_ad_rec_frag {
	struct m0_fid               arp_dom_id;
	struct m0_stob_id           arp_stob_id;
	struct stob_ad_rec_frag_seg arp_seg;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

M0_INTERNAL struct m0_stob_ad_domain *
stob_ad_domain2ad(const struct m0_stob_domain *dom);

enum m0_stob_ad_0type_rec_format_version {
	M0_STOB_AD_0TYPE_REC_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_STOB_AD_0TYPE_REC_FORMAT_VERSION */
	/*M0_STOB_AD_0TYPE_REC_FORMAT_VERSION_2,*/
	/*M0_STOB_AD_0TYPE_REC_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_STOB_AD_0TYPE_REC_FORMAT_VERSION = M0_STOB_AD_0TYPE_REC_FORMAT_VERSION_1
};

/* __MOTR_STOB_AD_PRIVATE_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
