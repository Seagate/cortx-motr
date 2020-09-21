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

#ifndef __MOTR_SCRIPTS_SYSTEMTAP_KEM_KEM_H__
#define __MOTR_SCRIPTS_SYSTEMTAP_KEM_KEM_H__

#include <linux/types.h>

/**
 * @defgroup kem Kernel Event Message entity definition
 *
 *
 * @{
 */

#define KEMD_DEV_NAME "kemd"

enum ke_type {
	KE_PAGE_FAULT,
	KE_CONTEXT_SWITCH,
};

struct pf_event {
	pid_t              pfe_pid;
	pid_t              pfe_tgid;
	unsigned long long pfe_rdtsc_call;
	unsigned long long pfe_rdtsc_ret;
	unsigned long      pfe_address;
	unsigned int       pfe_write_access;
	int                pfe_fault;
};

struct cs_event {
	pid_t              cse_prev_pid;
	pid_t              cse_prev_tgid;
	pid_t              cse_next_pid;
	pid_t              cse_next_tgid;
	unsigned long long cse_rdtsc;
};

struct ke_data {
	unsigned int ked_type;
	union {
		struct pf_event ked_pf;
		struct cs_event ked_cs;
	} u;
};

struct ke_msg {
	struct timeval kem_timestamp;
	struct ke_data kem_data;
};

/** @} end of kem group */

#endif /* __MOTR_SCRIPTS_SYSTEMTAP_KEM_KEM_H__ */

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
