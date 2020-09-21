/* -*- C -*- */
/*
 * Copyright (c) 2019-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_SCRIPTS_SYSTEMTAP_KEM_KEM_DEV_H__
#define __MOTR_SCRIPTS_SYSTEMTAP_KEM_KEM_DEV_H__

#include <linux/types.h>
#include <linux/cdev.h>
#include "kem.h"

#define KEMD_BUFFER_SIZE (4*1024)
#define KEMD_READ_PORTION 20

/*
 * Major number 60 has been chosen as it's number for local/experimental
 * devices. Refer to Linux Kernel documentation for details:
 *
 * https://www.kernel.org/doc/Documentation/admin-guide/devices.txt
 */
#define KEMD_MAJOR 60
#define KEMD_MINOR 0

/**
 * @defgroup kem_dev KEM device and ring buffer
 *
 * @{
 */

struct kem_rb {
	struct ke_msg *kr_buf;
	unsigned int   kr_size;
	unsigned int   kr_read_idx;
	unsigned int   kr_write_idx;
	atomic_t       kr_written;
	unsigned int   kr_logged;
	unsigned int   kr_occurred;
};

struct kem_dev {
	struct cdev    kd_cdev;
	atomic_t       kd_busy;
	int            kd_num;
	struct kem_rb *kd_rb;
};

/** @} end of kem_dev group */

#endif /* __MOTR_SCRIPTS_SYSTEMTAP_KEM_KEM_DEV_H__ */

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
