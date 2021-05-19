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


/**
  @addtogroup cookie
  @{
 */
#include <linux/uaccess.h>

#define try_read(addr, dummy)			\
({						\
	long rc;				\
	mm_segment_t save_fs = get_fs();	\
						\
	set_fs(KERNEL_DS);			\
	pagefault_disable();			\
	rc = __copy_from_user_inatomic(&(dummy), \
		(__force typeof(dummy) __user *)(addr), \
		sizeof(dummy));			\
	pagefault_enable();			\
	set_fs(save_fs);			\
	rc;					\
})

M0_INTERNAL bool m0_arch_addr_is_sane(const uint64_t * addr)
{
	uint64_t dummy;

	return try_read(addr, dummy) == 0;
}

M0_INTERNAL int m0_arch_cookie_global_init(void)
{
	return 0;
}

/**
 * This function is intentionally kept blank.
 */
M0_INTERNAL void m0_arch_cookie_global_fini(void)
{
}

/** @} end of cookie group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
