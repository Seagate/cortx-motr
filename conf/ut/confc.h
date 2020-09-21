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
#ifndef __MOTR_CONF_UT_CONFC_H__
#define __MOTR_CONF_UT_CONFC_H__

#include "fid/fid.h"       /* M0_FID_TINIT */

enum {
	M0_UT_CONF_ROOT,
	M0_UT_CONF_SITE,
	M0_UT_CONF_PROF,
	M0_UT_CONF_NODE,
	M0_UT_CONF_PROCESS0,
	M0_UT_CONF_PROCESS1,
	M0_UT_CONF_SERVICE0,
	M0_UT_CONF_SERVICE1,
	M0_UT_CONF_SDEV0,
	M0_UT_CONF_SDEV1,
	M0_UT_CONF_SDEV2,
	M0_UT_CONF_RACK,
	M0_UT_CONF_ENCLOSURE,
	M0_UT_CONF_CONTROLLER,
	M0_UT_CONF_DISK,
	M0_UT_CONF_POOL,
	M0_UT_CONF_PVER,
	M0_UT_CONF_RACKV,
	M0_UT_CONF_ENCLOSUREV,
	M0_UT_CONF_CONTROLLERV,
	M0_UT_CONF_DISKV,
	M0_UT_CONF_UNKNOWN_NODE
};

/* See ut/conf.xc */
static const struct m0_fid m0_ut_conf_fids[] = {
	[M0_UT_CONF_ROOT]         = M0_FID_TINIT('t', 1, 0),
	[M0_UT_CONF_SITE]         = M0_FID_TINIT('S', 1, 1),
	[M0_UT_CONF_PROF]         = M0_FID_TINIT('p', 1, 0),
	[M0_UT_CONF_NODE]         = M0_FID_TINIT('n', 1, 2),
	[M0_UT_CONF_PROCESS0]     = M0_FID_TINIT('r', 1, 5),
	[M0_UT_CONF_PROCESS1]     = M0_FID_TINIT('r', 1, 6),
	[M0_UT_CONF_SERVICE0]     = M0_FID_TINIT('s', 1, 9),
	[M0_UT_CONF_SERVICE1]     = M0_FID_TINIT('s', 1, 10),
	[M0_UT_CONF_SDEV0]        = M0_FID_TINIT('d', 1, 13),
	[M0_UT_CONF_SDEV1]        = M0_FID_TINIT('d', 1, 14),
	[M0_UT_CONF_SDEV2]        = M0_FID_TINIT('d', 1, 15),
	[M0_UT_CONF_RACK]         = M0_FID_TINIT('a', 1, 3),
	[M0_UT_CONF_ENCLOSURE]    = M0_FID_TINIT('e', 1, 7),
	[M0_UT_CONF_CONTROLLER]   = M0_FID_TINIT('c', 1, 11),
	[M0_UT_CONF_DISK]         = M0_FID_TINIT('k', 1, 16),
	[M0_UT_CONF_POOL]         = M0_FID_TINIT('o', 1, 4),
	[M0_UT_CONF_PVER]         = M0_FID_TINIT('v', 1, 8),
	[M0_UT_CONF_RACKV]        = M0_FID_TINIT('j', 1, 12),
	[M0_UT_CONF_ENCLOSUREV]   = M0_FID_TINIT('j', 1, 17),
	[M0_UT_CONF_CONTROLLERV]  = M0_FID_TINIT('j', 1, 18),
	[M0_UT_CONF_DISKV]        = M0_FID_TINIT('j', 1, 19),
	[M0_UT_CONF_UNKNOWN_NODE] = M0_FID_TINIT('n', 5, 5)
};

#endif /* __MOTR_CONF_UT_CONFC_H__ */
