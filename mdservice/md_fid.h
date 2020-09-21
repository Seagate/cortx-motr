/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_MDSERVICE_MD_FID_H__
#define __MOTR_MDSERVICE_MD_FID_H__

/**
   @defgroup md_fid Md fid constants

   @{
 */

/* import */
#include "fid/fid.h"

/* Namespace name for root cob (not exposed to user) */
M0_EXTERN const char M0_COB_ROOT_NAME[];

/* Grobal cob root fid. */
M0_EXTERN const struct m0_fid M0_COB_ROOT_FID;

/* Namespace name for virtual .motr directory */
M0_EXTERN const char M0_DOT_MOTR_NAME[];

/* .motr directory fid. */
M0_EXTERN const struct m0_fid M0_DOT_MOTR_FID;

/* Namespace name for virtual .motr/fid directory */
M0_EXTERN const char M0_DOT_MOTR_FID_NAME[];

/* .motr/fid directory fid. */
M0_EXTERN const struct m0_fid M0_DOT_MOTR_FID_FID;

/* Hierarchy root fid (exposed to user). */
M0_EXTERN const struct m0_fid M0_MDSERVICE_SLASH_FID;

/* First fid that is allowed to be used by client for normal files and dirs. */
M0_EXTERN const struct m0_fid M0_MDSERVICE_START_FID;

/** @} end of md_fid group */
#endif /* __MOTR_MDSERVICE_MD_FID_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
