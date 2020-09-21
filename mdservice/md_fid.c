/* -*- C -*- */
/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
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


#include "lib/errno.h"         /* EINVAL */
#include "lib/misc.h"          /* memcmp, strcmp */
#include "lib/string.h"        /* sscanf */
#include "lib/assert.h"        /* M0_PRE */
#include "fid/fid_xc.h"
#include "fid/fid.h"

/**
   @addtogroup md_fid

   @{
 */

/**
   Cob storage root. Not exposed to user.
 */
M0_INTERNAL const struct m0_fid M0_COB_ROOT_FID = {
	.f_container = 1ULL,
	.f_key       = 1ULL
};

M0_INTERNAL const char M0_COB_ROOT_NAME[] = "ROOT";

M0_INTERNAL const struct m0_fid M0_DOT_MOTR_FID = {
	.f_container = 1ULL,
	.f_key       = 4ULL
};

M0_INTERNAL const char M0_DOT_MOTR_NAME[] = ".motr";

M0_INTERNAL const struct m0_fid M0_DOT_MOTR_FID_FID = {
	.f_container = 1ULL,
	.f_key       = 5ULL
};

M0_INTERNAL const char M0_DOT_MOTR_FID_NAME[] = "fid";

/**
   Namespace root fid. Exposed to user.
*/
M0_INTERNAL const struct m0_fid M0_MDSERVICE_SLASH_FID = {
	.f_container = 1ULL,
	.f_key       = (1ULL << 16) - 1
};

M0_INTERNAL const struct m0_fid M0_MDSERVICE_START_FID = {
	.f_container = 1ULL,
	.f_key       = (1ULL << 16)
};

/** @} end of md_fid group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
