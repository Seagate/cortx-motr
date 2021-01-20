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

#ifndef __MOTR_NET_ULNET_CORE_H__
#define __MOTR_NET_ULNET_CORE_H__

/**
   @defgroup ULNetCore LNet Transport Core User Space Private Interface
   @ingroup LNetCore

   @{
 */

/**
   Userspace domain private data.
   This structure is pointed to by nlx_core_domain::cd_upvt.
 */
struct nlx_ucore_domain {
	uint64_t                        ud_magic;
	/** Cached maximum buffer size (counting all segments). */
	m0_bcount_t                     ud_max_buffer_size;
	/** Cached maximum size of a buffer segment. */
	m0_bcount_t                     ud_max_buffer_segment_size;
	/** Cached maximum number of buffer segments. */
	int32_t                         ud_max_buffer_segments;
	/** File descriptor to the kernel device. */
	int                             ud_fd;
};

/**
   Userspace transfer machine private data.
   This structure is pointed to by nlx_core_transfer_mc::ctm_upvt.
 */
struct nlx_ucore_transfer_mc {
	uint64_t utm_magic;
};

/**
   Userspace buffer private data.
   This structure is pointed to by nlx_core_buffer::cb_upvt.
 */
struct nlx_ucore_buffer {
	uint64_t                        ub_magic;
};

/** @} */ /* ULNetCore */

#endif /* __MOTR_NET_ULNET_CORE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
