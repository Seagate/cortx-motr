/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER
#include "fid/fid_list.h"
#include "motr/magic.h"

/**
   @addtogroup fid

   @{
 */

M0_TL_DESCR_DEFINE(m0_fids, "m0_fid list", M0_INTERNAL,
		   struct m0_fid_item, i_link, i_magic,
		   M0_FID_MAGIC, M0_FID_HEAD_MAGIC);

M0_TL_DEFINE(m0_fids, M0_INTERNAL, struct m0_fid_item);

#undef M0_TRACE_SUBSYSTEM

/** @} end of fid group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
