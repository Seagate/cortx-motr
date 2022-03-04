/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_SSS_PROCESS_FOPS_H__
#define __MOTR_SSS_PROCESS_FOPS_H__

#include "lib/types_xc.h"
#include "lib/buf_xc.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"

/**
 * @defgroup process_fop Driver Process FOP
 * @{
 */

struct m0_ref;
struct m0_rpc_machine;
extern struct m0_fop_type m0_fop_process_fopt;
extern struct m0_fop_type m0_fop_process_rep_fopt;
extern struct m0_fop_type m0_fop_process_svc_list_rep_fopt;

/**
 * Process command ID list except Reconfig command.
 * Reconfig command is send separate FOP
 */
enum m0_ss_process_req_cmd {
	M0_PROCESS_STOP,
	M0_PROCESS_RECONFIG,
	M0_PROCESS_HEALTH,
	M0_PROCESS_COUNTER,
	M0_PROCESS_QUIESCE,
	M0_PROCESS_RUNNING_LIST,
	M0_PROCESS_LIB_LOAD,
	M0_PROCESS_NR
};

/** Request to command a process. */
struct m0_ss_process_req {
	/**
	 * Command to execute.
	 * @see enum m0_ss_process_req_cmd
	 */
	uint32_t      ssp_cmd;
	/**
	 * Identifier of the process being started.
	 * fid type should set to M0_CONF_PROCESS_TYPE.cot_ftype
	 */
	struct m0_fid ssp_id;
	/**
	 * Additional parameter.
	 *
	 * Currently used only by M0_PROCESS_LIB_LOAD to pass the name of the
	 * library.
	 */
	struct m0_buf ssp_param;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Response to m0_ss_process_req. */
struct m0_ss_process_rep {
	/**
	 * Result of process operation or Process health
	 * (-Exxx = failure, 0 = success).
	 */
	int32_t sspr_rc;
	/**
	 * Process health. Valid for M0_PROCESS_HEALTH only.
	 */
	int32_t sspr_health;
	/**
	 * Filesystem stats - free space in BE segments. This will be reported
	 * as free inodes by df command.
	 *
	 * The field is meaningful only in case of M0_PROCESS_HEALTH command,
	 * and must be ignored in any other case.
	 */
	m0_bcount_t sspr_free_seg;
	/**
	 * Filesystem stats - total space in BE segments. This will be reported
	 * as total inodes by df command.
	 *
	 * The field is meaningful only in case of M0_PROCESS_HEALTH command,
	 * and must be ignored in any other case.
	 */
	m0_bcount_t sspr_total_seg;
	/**
	 * Filesystem stats - free space on disks. This will be the difference
	 * between total space in cluster and space consumed by user data.
	 */
	m0_bcount_t sspr_free_disk;
	/**
	 * Filesystem stats - space available for user data on disks. This will
	 * be reported as available disk space by  df command.
	 */
	m0_bcount_t sspr_avail_disk;
	/**
	 * Filesystem stats - total space on disks. This will be reported as
	 * total disk space by df command.
	 */
	m0_bcount_t sspr_total_disk;
	/**
	 * Buffer which holds keys from bytecount btree
	 */
	struct m0_buf sspr_bckey;
	/**
	 * Buffer which holds records from bytecount btree
	 */
	struct m0_buf sspr_bcrec;
	/**
	 * Number of key values in key and record buffers
	 */
	uint32_t      sspr_kv_count;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_ss_process_svc_item {
	struct m0_fid ssps_fid;
	/* real size set from ss_svc_list_running_fill */
	char          ssps_name[1];
};

struct m0_ss_process_svc_list_rep {
	/**
	 * Result of service start/stop operation
	 * (-Exxx = failure, 0 = success)
	 */
	int32_t        sspr_rc;
	struct m0_bufs sspr_services;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

M0_INTERNAL struct m0_fop *m0_ss_process_fop_create(struct m0_rpc_machine *mach,
						    uint32_t               cmd,
						    const struct m0_fid   *fid);

M0_INTERNAL bool m0_ss_fop_is_process_req(const struct m0_fop *fop);
M0_INTERNAL struct m0_ss_process_req *m0_ss_fop_process_req(struct m0_fop *fop);

M0_INTERNAL struct m0_ss_process_rep* m0_ss_fop_process_rep(struct m0_fop *fop);

M0_INTERNAL struct  m0_ss_process_svc_list_rep *
			m0_ss_fop_process_svc_list_rep(struct m0_fop *fop);

M0_INTERNAL int m0_ss_process_fops_init(void);
M0_INTERNAL void m0_ss_process_fops_fini(void);

M0_INTERNAL void m0_ss_process_stop_fop_release(struct m0_ref *ref);

/** @} ss_fop */
#endif /* __MOTR_SSS_PROCESS_FOPS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
