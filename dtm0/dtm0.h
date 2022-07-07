/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_DTM0_DTM0_H__
#define __MOTR_DTM0_DTM0_H__

/**
 * @page dtm0s-fspec The dtm0 service (DTM0S)
 *
 * DTM0 will reuse existing cas service and cas fops/foms to deliver dtx related
 * information.
 *
 * Cas fop will aggregate additional data related to dtx, txr {list of
 * participants, versions, ...}. It creates a tx descriptor containing this
 * information and embeds it in cas_op as a dtm0 transaction payload.
 *
 * The interface of the service for a user is a format of request/reply FOPs.
 * DTM0 service registers following FOP types during initialisation:
 * - @ref dtm0_req_fop_fopt
 * - @ref dtm0_rep_fop_fopt
 * - @ref dtm0_redo_fop_fopt
 *
 * DTM supports three kinds of FOP/FOM operations.
 * - EXECUTED FOP/FOM: when a cas fop is being executed, cas service will check
 *   for the existence of the dtm transaction information. If it is present, cas
 *   will log the tx descriptor locally on each of the dtm participants.  dtm0
 *   log will be updated inside cas fom (as property of tx). The txr will be
 *   executed just before tx_close phase of the fom inside fom_fol_rec_add().
 * - REDO FOP/FOM: When a participant service restarts after a transient failure
 *   the other existing participants sends REDO requests to the recovering
 *   participant.
 * - PERSISTENT FOP/FOM: When the transaction get logged, DTM sends a PERSISTENT
 *   FOP to the other participants and to the originator indicating that the
 *   change caused by the Cas operation has become permanent.
 *
 * @subsection dtm0-state State Specification
 *
 * @verbatim
 *
 *			    |
 *			    V
 *			FOPH_INIT
 *			    |
 *			    V
 *		    [generic phases]
 *			    |
 *			    V
 *			 TXN_INIT
 * 			    |
 *			    V
 *			 TXN_OPEN
 *			    |
 *			    V
 *		    UPDATE DTM0 LOG
 *			    |
 *			    V
 *			 TXN_CLOSE
 *			    |
 *			    V
 *			FOPH_FINI
 *
 * @verbatim
 *
 * Each dtm0_req_fop also carries a dtm_msg type which tells what kind of message it
 * is carrying.  A dtms0_op containing a request must specify a message type of
 * either DMT_EXECUTE,indicating that this request contains an operation to
 * be executed, a m0_dtms0_op containing a reply operation should contain either
 * a DMT_EXECUTED or a DMT_PERSISTENT message.
 */

/**
 * @{
 */

#include "fid/fid.h"            /* m0_fid */
#include "fid/fid_xc.h"         /* m0_fid_xc */
#include "lib/buf.h"            /* m0_bufs */
#include "lib/buf_xc.h"         /* m0_bufs_xc */
#include "lib/types.h"          /* uint64_t */
#include "xcode/xcode.h"        /* M0_XCA_RECORD */


struct m0_dtx0_id {
	uint64_t      dti_timestamp;    /* XXX fix the type */
	struct m0_fid dti_originator_sdev_fid;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc|be);


struct m0_dtx0_participants {
	uint64_t       dtpa_participants_nr;
	struct m0_fid *dtpa_participants;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc|be);

struct m0_dtx0_descriptor {
	struct m0_dtx0_id           dtd_id;
	struct m0_dtx0_participants dtd_participants;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc|be);

enum m0_dtx0_payload_type {
	M0_DTX0_PAYLOAD_CAS,    /** it's supposed to be handled by CAS */
	M0_DTX0_PAYLOAD_BLOB,   /**
				 *  configurable handler.
				 *  @see m0_dtm0_remach_cfg::dtrc_blob_handler()
				 */
} M0_XCA_ENUM M0_XCA_DOMAIN(rpc|be);

struct m0_dtx0_payload {
	uint32_t         dtp_type M0_XCA_FENUM(m0_dtx0_payload_type);
	struct m0_bufs   dtp_data;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc|be);

struct m0_dtm0_redo {
	struct m0_dtx0_descriptor dtr_descriptor;
	struct m0_dtx0_payload    dtr_payload;
};

struct m0_dtm0_p {
	struct m0_dtx0_id dtmp_id;
	struct m0_fid     dtmp_sdev_fid;
};

M0_INTERNAL int
m0_dtm0_redo_init(struct m0_dtm0_redo *redo,
		  const struct m0_dtx0_descriptor *descriptor,
		  const struct m0_buf             *payload,
		  enum m0_dtx0_payload_type        type);

M0_INTERNAL void m0_dtm0_redo_fini(struct m0_dtm0_redo *redo);

M0_INTERNAL bool m0_dtx0_id_eq(const struct m0_dtx0_id *left,
			       const struct m0_dtx0_id *right);

M0_INTERNAL int m0_dtx0_id_cmp(const struct m0_dtx0_id *left,
			       const struct m0_dtx0_id *right);

M0_INTERNAL int  m0_dtm0_mod_init(void);
M0_INTERNAL void m0_dtm0_mod_fini(void);

#endif /* __MOTR_DTM0_DTM0_H__ */

/*
 * }@
 */

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
