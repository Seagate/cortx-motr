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

#include "dtm0/drlink.h"

M0_INTERNAL int  m0_dtm0_rpc_link_mod_init(void)
{
	return 0;
}

M0_INTERNAL void m0_dtm0_rpc_link_mod_fini(void)
{
}

/**
 * Asynchronously send a DTM0 message to a remote DTM0 service.
 * @param svc local DTM0 service.
 * @param req DTM0 message to be sent.
 * @param tgt FID of the remote DTM0 service.
 * @param parent_fom FOM that caused this DTM0 message (used by ADDB).
 * @param wait_for_ack Determines whether the local service should wait
 *        for a reply from the counterpart.
 *        TODO: this may be converted into a proper clink and/or co_op later.
 * @return An error is returned if there is not enough resources (-ENOMEM) or
 *         if the remote service does not exist in the conf cache (-ENOENT).
 */
M0_INTERNAL int m0_dtm0_req_post(struct m0_dtm0_service    *svc,
                                 struct m0_be_op           *op,
				 const struct dtm0_req_fop *req,
				 const struct m0_fid       *tgt,
				 const struct m0_fom       *parent_fom,
				 bool                       wait_for_ack)
{
	(void) svc;
	(void) op;
	(void) req;
	(void) tgt;
	(void) parent_fom;
	(void) wait_for_ack;
	return 0;
}

#include "dtm0/recovery.h"

M0_INTERNAL int m0_drm_domain_init(void)
{
	return 0;
}

M0_INTERNAL void m0_drm_domain_fini(void)
{

}

M0_INTERNAL int
m0_dtm0_recovery_machine_init(struct m0_dtm0_recovery_machine           *m,
			      const struct m0_dtm0_recovery_machine_ops *ops,
			      struct m0_dtm0_service                    *svc)
{
	(void) m;
	(void) svc;
	(void) ops;
	return 0;
}

M0_INTERNAL void
m0_dtm0_recovery_machine_start(struct m0_dtm0_recovery_machine *m)
{
	(void) m;
}

M0_INTERNAL void
m0_dtm0_recovery_machine_stop(struct m0_dtm0_recovery_machine *m)
{
	(void) m;
}

M0_INTERNAL void
m0_dtm0_recovery_machine_fini(struct m0_dtm0_recovery_machine *m)
{
	(void) m;
}

M0_INTERNAL void
m0_dtm0_recovery_machine_redo_post(struct m0_dtm0_recovery_machine *m,
				   struct dtm0_req_fop             *redo,
				   struct m0_be_op                 *op)
{
	(void) m;
	(void) redo;
	(void) op;
}
