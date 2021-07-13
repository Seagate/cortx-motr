/* -*- C -*- */
/*
* Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
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

#ifndef __MOTR_FDMI_PLUGINS_SCHED_H__
#define __MOTR_FDMI_PLUGINS_SCHED_H__

#include "fid/fid.h"
#include "motr/client.h"
#include "motr/client_internal.h"
#include "lib/getopts.h"	/* M0_GETOPTS */
#include "lib/trace.h"
#include "fdmi/fdmi.h"
#include "fdmi/plugin_dock.h"
#include "fdmi/service.h"
#include "reqh/reqh.h"
#include "ut/ut.h"

#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <stddef.h>             /* ptrdiff_t */
#include <fcntl.h>
#include <stdio.h>

/* sched client conf params */
struct sched_conf {
	char          *local_addr;
	char          *ha_addr;
	char          *profile_fid;
	char          *process_fid;
	char          *fdmi_plugin_fid_s;
	struct m0_fid  fdmi_plugin_fid;
} c_params;

#endif /*  __MOTR_FDMI_PLUGINS_SCHED_H__ */
