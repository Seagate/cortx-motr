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

#ifndef __MOTR_SSS_DEVICE_FOPS_H__
#define __MOTR_SSS_DEVICE_FOPS_H__

#include "lib/types_xc.h"
#include "lib/buf_xc.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"
#include "rpc/rpc_machine.h"

/**
 * @page DLD_sss_device DLD Device command
 *
 * - @ref DLD-sss_device-ds
 * - @ref DLD-sss_device-usecases
 * - @ref DLDGRP_sss_device "Device command internal" <!-- Note link -->
 *
 * Device commands derived disk (see m0_conf_drive) on Motr. Each device
 * must preregister on Pool machine, and has record in confc subsystem. All
 * additional information about disk and its storage device must present in
 * confc on server side.
 *
 * Server side - all know Motr instances with ioservices service.
 *
 * Commands
 *
 * Attach device - create AD stob domain and backing store stob. Also create
 * and run Pool event for set status "Device online" in Pool machine.
 * Its mechanism like create each disk duration start Motr instance.
 *
 * Detach device - finalize AD stob domain and backing store stob in memory.
 * Its not destroy file on storage device - remove information about storage
 * device file for Motr only. Also create and run Pool event for set status
 * "Device offline".
 *
 * Format device - formatting storage device. Not implement on server side.

 * @section DLD-sss_device-ds Data Structures
 *
 * Device command interface data structures include fop m0_sss_device_fop,
 * command enumerated m0_sss_device_req_cmd.
 *
 * Example:
 *
@code
struct m0_fop  *fop;

fop  = m0_sss_device_fop_create(rmachine, cmd, dev_fid);
if (fop == NULL)
	return M0_RC(-ENOMEM);
@endcode

 * cmd - device command id - see m0_sss_device_req_cmd
 * dev_fid - Disk FID.
 *
 * Reply fop - m0_sss_device_fop_rep contain error status of command.
 *
 * @section DLD-sss_device-usecases Unit tests
 *
 * spiel-ci-ut:device-cmds
 *
 * Contain 4 steps:
 *
 * - detach - detach device. Device was connect on standard start UT and
 * description in file ut/conf.xc.
 * Expected return value - OK (0)
 *
 * - attach - attach device. Attach device which was detach on previous step.
 * Expected return value - OK (0)
 *
 * - format - format device. Client send command and receive answer.
 * No activity on server side except receive command and send answer.
 * Expected return value - OK (0)
 *
 * - detach with invalid fid - detach device with invalid fid. Command not send.
 * Expected return value - (-ENOENT)
 *
 *
 * sss-ut:device-fom-fail
 *
 * Test some error when create fom for this commands. See create command
 * m0_sss_device_fop_create and custom fom struct m0_sss_device_fom.
 */

/**
 * @defgroup DLDGRP_sss_device Device command
 * @brief Detailed functional Device command
 *
 * All Device commands use one type of fop m0_sss_device_fop for send
 * command to sss service and one type of fop m0_sss_device_fop_rep
 * for reply.
 * Command different command ID m0_sss_device_req_cmd only.
 *
 * @{
 */

extern struct m0_fop_type m0_sss_fop_device_fopt;
extern struct m0_fop_type m0_sss_fop_device_rep_fopt;

/**
 * Device commands enumerated
 *
 * Value of custom fop field @ref ssd_cmd, determines device operation.
 */
enum m0_sss_device_req_cmd {
	/**
	 * Attach command.
	 * Create AD stob domain, stob and change device status to online in
	 * Pool machine.
	 */
	M0_DEVICE_ATTACH,
	/**
	 * Detach command.
	 * Finalization AD stob domain, stob and change device status to offline
	 * in Pool machine.
	 */
	M0_DEVICE_DETACH,
	/**
	 * Format command.
	 * Format select device.
	 */
	M0_DEVICE_FORMAT,
	/**
	 * Number of device commands.
	 */
	M0_DEVICE_CMDS_NR
};

/** Request to command a device.
 *
 * Request fop contain ID command and device fid.
 * All needs to execute command: index in Pool machine, device cid,
 * etc. fom @ref m0_sss_device_fom - reads form confc uses ssd_fid as
 * disk fid.
 */
struct m0_sss_device_fop {
	/**
	 * Command to execute.
	 * @see enum m0_sss_device_req_cmd
	 */
	uint32_t      ssd_cmd;
	/**
	 * Disk fid.
	 */
	struct m0_fid ssd_fid;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Response to m0_sss_device_fop. */
struct m0_sss_device_fop_rep {
	/**
	 * Result of device operation
	 * (-Exxx = failure, 0 = success).
	 * @see enum m0_reqh_process_state
	 */
	int32_t  ssdp_rc;
	/**
	 * Device HA state found on the called SSS side. The field is valid in
	 * case of M0_DEVICE_ATTACH command only.
	 */
	uint32_t ssdp_ha_state;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);


M0_INTERNAL struct m0_fop *m0_sss_device_fop_create(struct m0_rpc_machine *mach,
						    uint32_t               cmd,
						    const struct m0_fid   *fid);

M0_INTERNAL bool m0_sss_fop_is_dev_req(const struct m0_fop *fop);

M0_INTERNAL struct m0_sss_device_fop *m0_sss_fop_to_dev_req(struct m0_fop *fop);

M0_INTERNAL bool m0_sss_fop_is_dev_rep(const struct m0_fop *fop);

M0_INTERNAL
struct m0_sss_device_fop_rep *m0_sss_fop_to_dev_rep(struct m0_fop *fop);

M0_INTERNAL int m0_sss_device_fops_init(void);

M0_INTERNAL void m0_sss_device_fops_fini(void);

/** @} end group  DLDGRP_sss_device */

#endif /* __MOTR_SSS_DEVICE_FOPS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
