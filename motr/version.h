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

#ifndef __MOTR_MOTR_VERSION_H__
#define __MOTR_MOTR_VERSION_H__

#include "motr/version_macros.h"
#include "lib/types.h"

struct m0_build_info {
	uint32_t     bi_version;
	const char  *bi_version_string;
	const char  *bi_git_rev_id;
	const char  *bi_git_describe;
	const char  *bi_git_branch;
	const char  *bi_xcode_protocol_checksum;
	const char  *bi_xcode_protocol_be_checksum;
	const char  *bi_xcode_protocol_conf_checksum;
	const char  *bi_xcode_protocol_rpc_checksum;
	const char  *bi_host;
	const char  *bi_user;
	const char  *bi_time;
	const char  *bi_toolchain;
	const char  *bi_kernel;
	const char  *bi_cflags;
	const char  *bi_kcflags;
	const char  *bi_ldflags;
	const char  *bi_configure_opts;
	const char  *bi_build_dir;
	const char  *bi_lustre_src;
	const char  *bi_lustre_version;
};

const struct m0_build_info *m0_build_info_get(void);

void m0_build_info_print(void);

#endif /* __MOTR_MOTR_VERSION_H__ */

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
