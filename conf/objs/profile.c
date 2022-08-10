/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/objs/common.h"
#include "conf/onwire_xc.h"  /* m0_confx_profile_xc */
#include "motr/magic.h"      /* M0_CONF_PROFILE_MAGIC */

#define XCAST(xobj) ((struct m0_confx_profile *)(&(xobj)->xo_u))
M0_BASSERT(offsetof(struct m0_confx_profile, xp_header) == 0);

static bool profile_check(const void *bob)
{
	const struct m0_conf_profile *self = bob;

	M0_PRE(m0_conf_obj_type(&self->cp_obj) == &M0_CONF_PROFILE_TYPE);

	return m0_conf_obj_is_stub(&self->cp_obj) ||
		_0C(m0_fid_arr_all_unique(&self->cp_pools));
}

M0_CONF__BOB_DEFINE(m0_conf_profile, M0_CONF_PROFILE_MAGIC, profile_check);
M0_CONF__INVARIANT_DEFINE(profile_invariant, m0_conf_profile);

static int
profile_decode(struct m0_conf_obj *dest, const struct m0_confx_obj *src)
{
	const struct m0_confx_profile *s = XCAST(src);
	struct m0_conf_profile        *d = M0_CONF_CAST(dest, m0_conf_profile);

	M0_PRE(equi(s->xp_pools.af_count == 0, s->xp_pools.af_elems == NULL));

	return M0_RC(m0_fid_arr_copy(&d->cp_pools, &s->xp_pools));
}

static int
profile_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	const struct m0_conf_profile *s = M0_CONF_CAST(src, m0_conf_profile);
	struct m0_confx_profile      *d = XCAST(dest);

	confx_encode(dest, src);

	return M0_RC(m0_fid_arr_copy(&d->xp_pools, &s->cp_pools));
}

static bool
profile_match(const struct m0_conf_obj *cached, const struct m0_confx_obj *flat)
{
	const struct m0_confx_profile *xobj = XCAST(flat);
	const struct m0_conf_profile  *obj = M0_CONF_CAST(cached,
							  m0_conf_profile);
	return m0_fid_arr_eq(&obj->cp_pools, &xobj->xp_pools);
}

static void profile_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_profile *x = M0_CONF_CAST(obj, m0_conf_profile);

	m0_conf_profile_bob_fini(x);
	arrfid_free(&x->cp_pools);
	m0_free(x);
}

static const struct m0_conf_obj_ops profile_ops = {
	.coo_invariant = profile_invariant,
	.coo_decode    = profile_decode,
	.coo_encode    = profile_encode,
	.coo_match     = profile_match,
	.coo_lookup    = conf_obj_lookup_denied,
	.coo_readdir   = NULL,
	.coo_downlinks = conf_obj_downlinks_none,
	.coo_delete    = profile_delete
};

M0_CONF__CTOR_DEFINE(profile_create, m0_conf_profile, &profile_ops);

const struct m0_conf_obj_type M0_CONF_PROFILE_TYPE = {
	.cot_ftype = {
		.ft_id   = M0_CONF__PROFILE_FT_ID,
		.ft_name = "conf_profile"
	},
	.cot_create  = &profile_create,
	.cot_xt      = &m0_confx_profile_xc,
	.cot_branch  = "u_profile",
	.cot_xc_init = &m0_xc_m0_confx_profile_struct_init,
	.cot_magic   = M0_CONF_PROFILE_MAGIC
};

#undef XCAST
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
