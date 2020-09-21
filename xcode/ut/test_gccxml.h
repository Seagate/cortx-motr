/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_XCODE_UT_TEST_GCCXML_H__
#define __MOTR_XCODE_UT_TEST_GCCXML_H__

#include <sys/types.h>
#include <stdint.h>

#include "xcode/xcode.h"
#include "lib/vec.h"
#include "lib/vec_xc.h"

#include "xcode/ut/test_gccxml_simple.h"
#include "xcode/ut/test_gccxml_simple_xc.h"


struct package {
	struct m0_fid   p_fid;
	struct m0_vec   p_vec;
	struct m0_cred *p_cred M0_XCA_OPAQUE("m0_package_cred_get");
	struct package_p_name {
		uint32_t  s_nr;
		uint8_t  *s_data;
	} M0_XCA_SEQUENCE p_name;
} M0_XCA_RECORD;

#endif /* __MOTR_XCODE_UT_TEST_GCCXML_H__ */

