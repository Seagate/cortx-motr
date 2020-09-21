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


#include <linux/module.h>

/*
  This file contains dummy init() and fini() routines for modules, that are
  not yet ported to kernel.

  Once the module compiles successfully for kernel mode, dummy routines from
  this file should be removed.
 */

M0_INTERNAL int m0_linux_stobs_init(void)
{
	return 0;
}

M0_INTERNAL void m0_linux_stobs_fini(void)
{
}

M0_INTERNAL int sim_global_init(void)
{
	return 0;
}

M0_INTERNAL void sim_global_fini(void)
{
}

M0_INTERNAL int m0_timers_init(void)
{
	return 0;
}

M0_INTERNAL void m0_timers_fini(void)
{
}
