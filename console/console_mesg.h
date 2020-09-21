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

#ifndef __MOTR_CONSOLE_MESG_H__
#define __MOTR_CONSOLE_MESG_H__

#include "fop/fop.h"
#include "rpc/rpc.h"

/**
 *  Prints name and opcode of FOP.
 *  It can be used to print more info if required.
 */
M0_INTERNAL void m0_cons_fop_name_print(const struct m0_fop_type *ftype);

/**
 * @brief Builds and send FOP using rpc_post and waits for reply.
 *
 * @param fop	   FOP to be send.
 * @param session  RPC connection session.
 * @param resend_interval
 * @param nr_sent_max
 *                 Attempt to send fop at most nr_sent_max number of
 *                 times after each resend_interval; timeout fop if reply
 *                 is not received after nr_sent_max attempts.
 */
M0_INTERNAL int m0_cons_fop_send(struct m0_fop *fop,
				 struct m0_rpc_session *session,
				 m0_time_t resend_interval,
				 uint64_t nr_sent_max);

/**
 *  @brief Iterate over FOP fields and print names.
 */
M0_INTERNAL int m0_cons_fop_show(struct m0_fop_type *fopt);

/**
 * @brief Helper function to print list of FOPs.
 */
M0_INTERNAL void m0_cons_fop_list_show(void);

/* __MOTR_CONSOLE_MESG_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */

