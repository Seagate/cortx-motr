/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"

#include "ut/ut.h"
#include "fdmi/fdmi.h"
#include "fdmi/source_dock_internal.h"

#include "fdmi/ut/sd_common.h"

void fdmi_sd_post_record(void);
void fdmi_sd_apply_filter(void);
void fdmi_sd_release_fom(void);
void fdmi_sd_send_notif(void);

struct m0_ut_suite fdmi_sd_ut = {
	.ts_name = "fdmi-sd-ut",
	.ts_tests = {
		{ "fdmi-sd-post-record", fdmi_sd_post_record},
		{ "fdmi-sd-apply-filter", fdmi_sd_apply_filter},
		{ "fdmi-sd-release-fom", fdmi_sd_release_fom},
		{ "fdmi-sd-send-notif", fdmi_sd_send_notif},

		{ NULL, NULL },
	},
};

#undef M0_TRACE_SUBSYSTEM

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
