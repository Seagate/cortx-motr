/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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


#include "udb.h"

/**
   @addtogroup udb
   @{
 */


M0_INTERNAL int m0_udb_ctxt_init(struct m0_udb_ctxt *ctxt)
{
	/* TODO add more here. Now it is a stub */
	return 0;
}

M0_INTERNAL void m0_udb_ctxt_fini(struct m0_udb_ctxt *ctxt)
{

	/* TODO add more here. Now it is a stub */
	return;
}

M0_INTERNAL int m0_udb_add(struct m0_udb_ctxt *ctxt,
			   const struct m0_udb_domain *edomain,
			   const struct m0_udb_cred *external,
			   const struct m0_udb_cred *internal)
{
	/* TODO add more here. Now it is a stub */
	return 0;
}

M0_INTERNAL int m0_udb_del(struct m0_udb_ctxt *ctxt,
			   const struct m0_udb_domain *edomain,
			   const struct m0_udb_cred *external,
			   const struct m0_udb_cred *internal)
{

	/* TODO add more here. Now it is a stub */
	return 0;
}

M0_INTERNAL int m0_udb_e2i(struct m0_udb_ctxt *ctxt,
			   const struct m0_udb_cred *external,
			   struct m0_udb_cred *internal)
{

	/* TODO add more here. Now it is a stub */
	return 0;
}

M0_INTERNAL int m0_udb_i2e(struct m0_udb_ctxt *ctxt,
			   const struct m0_udb_cred *internal,
			   struct m0_udb_cred *external)
{

	/* TODO add more here. Now it is a stub */
	return 0;
}

/** @} end group udb */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
