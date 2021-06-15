/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
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
#include "conf/onwire_xc.h"  /* m0_confx_controller_xc */
#include "motr/magic.h"      /* M0_CONF_CONTROLLER_MAGIC */

#define XCAST(xobj) ((struct m0_confx_controller *)(&(xobj)->xo_u))
M0_BASSERT(offsetof(struct m0_confx_controller, xc_header) == 0);

static bool controller_check(const void *bob)
{
	const struct m0_conf_controller *self = bob;
	const struct m0_conf_obj        *self_obj = &self->cc_obj;

	M0_PRE(m0_conf_obj_type(self_obj) == &M0_CONF_CONTROLLER_TYPE);

	return true;
}

M0_CONF__BOB_DEFINE(m0_conf_controller, M0_CONF_CONTROLLER_MAGIC,
		    controller_check);
M0_CONF__INVARIANT_DEFINE(controller_invariant, m0_conf_controller);

static int
controller_decode(struct m0_conf_obj *dest, const struct m0_confx_obj *src)
{
	int                               rc;
	const struct m0_confx_controller *s = XCAST(src);
	struct m0_conf_controller        *d =
		M0_CONF_CAST(dest, m0_conf_controller);

	rc = dir_create_and_populate(&d->cc_drives,
		     &CONF_DIR_ENTRIES(&M0_CONF_CONTROLLER_DRIVES_FID,
				       &M0_CONF_DRIVE_TYPE, &s->xc_drives),
		                     dest) ?:
	     conf_pvers_decode(&d->cc_pvers, &s->xc_pvers, dest->co_cache);

	return M0_RC(rc);
}

static int
controller_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	struct m0_conf_controller  *s = M0_CONF_CAST(src, m0_conf_controller);
	struct m0_confx_controller *d = XCAST(dest);
	const struct conf_dir_encoding_pair dirs[] = {
		{ s->cc_drives, &d->xc_drives }
	};

	confx_encode(dest, src);
	return M0_RC(conf_dirs_encode(dirs, ARRAY_SIZE(dirs)) ?:
		     conf_pvers_encode(
			     &d->xc_pvers,
			     (const struct m0_conf_pver**)s->cc_pvers));
}

static bool controller_match(const struct m0_conf_obj *cached,
			     const struct m0_confx_obj *flat)
{
	const struct m0_confx_controller *xobj = XCAST(flat);
	const struct m0_conf_controller  *obj  =
		M0_CONF_CAST(cached, m0_conf_controller);

	return m0_conf_dir_elems_match(obj->cc_drives, &xobj->xc_drives);
}

static int controller_lookup(const struct m0_conf_obj *parent,
			     const struct m0_fid *name,
			     struct m0_conf_obj **out)
{
	struct m0_conf_controller *c = M0_CONF_CAST(parent, m0_conf_controller);
	const struct conf_dir_relation dirs[] = {
		{ c->cc_drives, &M0_CONF_CONTROLLER_DRIVES_FID }
	};

	M0_PRE(parent->co_status == M0_CS_READY);
	return M0_RC(conf_dirs_lookup(out, name, dirs, ARRAY_SIZE(dirs)));
}

static const struct m0_fid **controller_downlinks(const struct m0_conf_obj *obj)
{
	static const struct m0_fid *rels[] = { &M0_CONF_CONTROLLER_DRIVES_FID,
					       NULL };
	M0_PRE(m0_conf_obj_type(obj) == &M0_CONF_CONTROLLER_TYPE);
	return rels;
}

static void controller_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_controller *x = M0_CONF_CAST(obj, m0_conf_controller);
	m0_conf_controller_bob_fini(x);
	m0_free(x->cc_pvers);
	m0_free(x);
}

static const struct m0_conf_obj_ops controller_ops = {
	.coo_invariant = controller_invariant,
	.coo_decode    = controller_decode,
	.coo_encode    = controller_encode,
	.coo_match     = controller_match,
	.coo_lookup    = controller_lookup,
	.coo_readdir   = NULL,
	.coo_downlinks = controller_downlinks,
	.coo_delete    = controller_delete
};

M0_CONF__CTOR_DEFINE(controller_create, m0_conf_controller, &controller_ops);

const struct m0_conf_obj_type M0_CONF_CONTROLLER_TYPE = {
	.cot_ftype = {
		.ft_id   = M0_CONF__CONTROLLER_FT_ID,
		.ft_name = "conf_controller"
	},
	.cot_create  = &controller_create,
	.cot_xt      = &m0_confx_controller_xc,
	.cot_branch  = "u_controller",
	.cot_xc_init = &m0_xc_m0_confx_controller_struct_init,
	.cot_magic   = M0_CONF_CONTROLLER_MAGIC
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
