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
