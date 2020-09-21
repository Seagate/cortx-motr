/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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


#include "motr/client.h"
#include "motr/addb.h"
#include "motr/client_internal.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLIENT
#include "lib/trace.h"                /* M0_LOG */
#include "fid/fid.h"                  /* m0_fid */

void m0_container_init(struct m0_container     *con,
		       struct m0_realm         *parent,
		       const struct m0_uint128 *id,
		       struct m0_client        *instance)
{
	M0_PRE(con != NULL);
	M0_PRE(id != NULL);
	M0_PRE(instance != NULL);

	if (m0_uint128_cmp(&M0_UBER_REALM, id) == 0) {
		/* This should be an init/open cycle for the uber realm */
		M0_ASSERT(parent == NULL);

		con->co_realm.re_entity.en_id = *id;
		con->co_realm.re_entity.en_type = M0_ET_REALM;
		con->co_realm.re_entity.en_realm = &con->co_realm;
		con->co_realm.re_instance = instance;
		con->co_realm.re_entity.en_sm.sm_rc = 0;
	} else
		M0_ASSERT_INFO(0, "Feature not implemented");
}
M0_EXPORTED(m0_container_init);

#undef M0_TRACE_SUBSYSTEM

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
