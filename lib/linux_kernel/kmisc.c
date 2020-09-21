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


#include <linux/cpu.h> /* simple_strtoull, simple_strtoul */
#include "lib/misc.h"

uint64_t m0_strtou64(const char *str, char **endptr, int base)
{
	return simple_strtoull(str, endptr, base);
}
M0_EXPORTED(m0_strtou64);

uint32_t m0_strtou32(const char *str, char **endptr, int base)
{
	return simple_strtoul(str, endptr, base);
}
M0_EXPORTED(m0_strtou32);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
