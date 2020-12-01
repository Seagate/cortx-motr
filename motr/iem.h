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

#ifndef __MOTR_IEM_H__
#define __MOTR_IEM_H__

#include "lib/types.h"

enum m0_motr_iem_severity {
	M0_MOTR_IEM_SEVERITY_TEST = 0,
	M0_MOTR_IEM_SEVERITY_A_ALERT,
	M0_MOTR_IEM_SEVERITY_X_CRITICAL,
	M0_MOTR_IEM_SEVERITY_E_ERROR,
	M0_MOTR_IEM_SEVERITY_W_WARNING,
	M0_MOTR_IEM_SEVERITY_N_NOTICE,
	M0_MOTR_IEM_SEVERITY_C_CONFIGURATION,
	M0_MOTR_IEM_SEVERITY_I_INFORMATIONAL,
	M0_MOTR_IEM_SEVERITY_D_DETAIL,
	M0_MOTR_IEM_SEVERITY_B_DEBUG,
};

/**
 * The members of enum m0_motr_iem_module and enum m0_motr_iem_event are mapped
 * against the file [low-level/files/iec_mapping/motr] in the sspl repo
 * https://github.com/Seagate/cortx-sspl
 *
 * Field description of the mpping file is available in slide 11 of the  "RAS
 * IEM Alerts" document
 *
 * 0020010001,TestIEM,Motr test IEM
 * 002 is component id, 001 is module id and 0001 is event id
 *
 * As per "RAS IEM Alerts" document,
 *     Other teams inform RAS team about a new IEMs.
 *     RAS team will add new IEMs to this file.
 *     File will be local to SSPL and may be a part of repo.
 *
 * Any new entry to these enums must also be updated to the mapping file as
 * well.
 */

enum m0_motr_iem_module {
	M0_MOTR_IEM_MODULE_TEST = 1,
	M0_MOTR_IEM_MODULE_IO,
	M0_MOTR_IEM_MODULE_OS,
};

enum m0_motr_iem_event {
	M0_MOTR_IEM_EVENT_TEST = 1,
	M0_MOTR_IEM_EVENT_IOQ,
	M0_MOTR_IEM_EVENT_FREE_SPACE,
	M0_MOTR_IEM_EVENT_RPC_FAILED,
	M0_MOTR_IEM_EVENT_FOM_HANG,
	M0_MOTR_IEM_EVENT_MD_ERROR,
	M0_IEM_EVENT_NR
};

/**
 * The function must be called with appropriate parameters using
 * the macros M0_MOTR_IEM() & M0_MOTR_IEM_DESC() to send an IEM alert.
 * This IEM is throttled by a simple throttling scheme.
 * The throttling scheme is the maximum number of IEM alerts for the defined
 * interval.
 *
 * @param file from where m0_iem is called, use __FILE__
 * @param function from where m0_iem is called, use __FUNCTION__
 * @param line from where m0_iem is called, use __LINE__
 * @param sev_id a valid value from enum m0_motr_iem_severity
 * @param mod_id a valid value from enum m0_motr_iem_module
 * @param evt_id a valid value from enum m0_motr_iem_event
 * @param report_count print event count if TRUE.
 * @param desc a string description with variable args. Can be NULL,
 *             max (512-1) bytes in length.
 */
void m0_iem(const char* file, const char* function, int line,
	    const enum m0_motr_iem_severity sev_id,
	    const enum m0_motr_iem_module mod_id,
	    const enum m0_motr_iem_event evt_id,
	    const bool report_evt_count,
	    const char* desc, ...);

#define M0_MOTR_IEM(_sev_id, _mod_id, _evt_id) \
	m0_iem(__FILE__, __FUNCTION__, __LINE__, \
	       _sev_id, _mod_id, _evt_id, true, NULL)

#define M0_MOTR_IEM_DESC(_sev_id, _mod_id, _evt_id, _desc, ...) \
	m0_iem(__FILE__, __FUNCTION__, __LINE__, \
	       _sev_id, _mod_id, _evt_id, true, _desc, __VA_ARGS__)


#endif  // __MOTR_IEM_H__
