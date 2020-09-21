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


#include "capa.h"

/**
   @addtogroup capa
   @{
 */

M0_INTERNAL int m0_capa_init(struct m0_capa_ctxt *ctxt)
{
	/* TODO This is only stub */
	return 0;
}

M0_INTERNAL void m0_capa_fini(struct m0_capa_ctxt *ctxt)
{
	/* TODO This is only stub */
	return;
}

M0_INTERNAL int m0_capa_new(struct m0_object_capa *capa,
			    enum m0_capa_entity_type type,
			    enum m0_capa_operation opcode, void *data)
{
	capa->oc_ctxt = NULL;
	capa->oc_owner = NULL;
	capa->oc_type = type;
	capa->oc_opcode = opcode;
	capa->oc_data = data;
	m0_atomic64_set(&capa->oc_ref, 0);
	return 0;
}

M0_INTERNAL int m0_capa_get(struct m0_capa_ctxt *ctxt,
			    struct m0_capa_issuer *owner,
			    struct m0_object_capa *capa)
{
	/* TODO This is only stub */
	capa->oc_ctxt = ctxt;
	capa->oc_owner = owner;

	m0_atomic64_inc(&capa->oc_ref);
	return 0;
}

M0_INTERNAL void m0_capa_put(struct m0_capa_ctxt *ctxt,
			     struct m0_object_capa *capa)
{
	/* TODO This is only stub */
	M0_ASSERT(m0_atomic64_get(&capa->oc_ref) > 0);
	m0_atomic64_dec(&capa->oc_ref);
	return;
}

M0_INTERNAL int m0_capa_auth(struct m0_capa_ctxt *ctxt,
			     struct m0_object_capa *capa,
			     enum m0_capa_operation op)
{
	/* TODO This is only stub */
	return 0;
}

M0_INTERNAL int m0_capa_ctxt_init(struct m0_capa_ctxt *ctxt)
{
	/* TODO This is only stub */
	return 0;
}

M0_INTERNAL void m0_capa_ctxt_fini(struct m0_capa_ctxt *ctxt)
{
	/* TODO This is only stub */
}

/** @} end group capa */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
