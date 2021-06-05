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
#include "conf/onwire_xc.h"  /* m0_confx_enclosure_xc */
#include "motr/magic.h"      /* M0_CONF_ENCLOSURE_MAGIC */

#define XCAST(xobj) ((struct m0_confx_enclosure *)(&(xobj)->xo_u))
M0_BASSERT(offsetof(struct m0_confx_enclosure, xe_header) == 0);

static bool enclosure_check(const void *bob)
{
	const struct m0_conf_enclosure *self = bob;

	M0_PRE(m0_conf_obj_type(&self->ce_obj) == &M0_CONF_ENCLOSURE_TYPE);

	return true;
}

M0_CONF__BOB_DEFINE(m0_conf_enclosure, M0_CONF_ENCLOSURE_MAGIC,
		    enclosure_check);
M0_CONF__INVARIANT_DEFINE(enclosure_invariant, m0_conf_enclosure);

static int
enclosure_decode(struct m0_conf_obj *dest, const struct m0_confx_obj *src)
{
	int                              rc;
	struct m0_conf_obj              *obj;
	struct m0_conf_enclosure        *d = M0_CONF_CAST(dest,
							  m0_conf_enclosure);
	const struct m0_confx_enclosure *s = XCAST(src);

	rc = m0_conf_obj_find(dest->co_cache, &s->xe_node, &obj);
	if (rc != 0)
		return M0_ERR(rc);

	d->ce_node = M0_CONF_CAST(obj, m0_conf_node);

	return M0_RC(dir_create_and_populate(
			     &d->ce_ctrls,
			     &CONF_DIR_ENTRIES(&M0_CONF_ENCLOSURE_CTRLS_FID,
					       &M0_CONF_CONTROLLER_TYPE,
					       &s->xe_ctrls), dest) ?:
                     conf_pvers_decode(&d->ce_pvers, &s->xe_pvers,
				       dest->co_cache));
}

static int
enclosure_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	struct m0_conf_enclosure  *s = M0_CONF_CAST(src, m0_conf_enclosure);
	struct m0_confx_enclosure *d = XCAST(dest);
	const struct conf_dir_encoding_pair dirs[] = {
		{ s->ce_ctrls, &d->xe_ctrls }
	};

	confx_encode(dest, src);
	if (s->ce_node != NULL)
		d->xe_node = s->ce_node->cn_obj.co_id;

	return M0_RC(conf_dirs_encode(dirs, ARRAY_SIZE(dirs)) ?:
		     conf_pvers_encode(
			     &d->xe_pvers,
			     (const struct m0_conf_pver**)s->ce_pvers));
}

static bool enclosure_match(const struct m0_conf_obj  *cached,
			    const struct m0_confx_obj *flat)
{
	const struct m0_confx_enclosure *xobj = XCAST(flat);
	const struct m0_conf_enclosure  *obj  =
		M0_CONF_CAST(cached, m0_conf_enclosure);

	return m0_fid_eq(&obj->ce_node->cn_obj.co_id, &xobj->xe_node) &&
	       m0_conf_dir_elems_match(obj->ce_ctrls, &xobj->xe_ctrls);
}

static int enclosure_lookup(const struct m0_conf_obj *parent,
			    const struct m0_fid *name, struct m0_conf_obj **out)
{
	struct m0_conf_enclosure *e = M0_CONF_CAST(parent, m0_conf_enclosure);
	const struct conf_dir_relation dirs[] = {
		{ e->ce_ctrls, &M0_CONF_ENCLOSURE_CTRLS_FID }
	};

	M0_PRE(parent->co_status == M0_CS_READY);
	return M0_RC(conf_dirs_lookup(out, name, dirs, ARRAY_SIZE(dirs)));
}

static const struct m0_fid **enclosure_downlinks(const struct m0_conf_obj *obj)
{
	static const struct m0_fid *rels[] = { &M0_CONF_ENCLOSURE_CTRLS_FID,
					       NULL };
	M0_PRE(m0_conf_obj_type(obj) == &M0_CONF_ENCLOSURE_TYPE);
	return rels;
}

static void enclosure_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_enclosure *x = M0_CONF_CAST(obj, m0_conf_enclosure);
	m0_conf_enclosure_bob_fini(x);
	m0_free(x->ce_pvers);
	m0_free(x);
}

static const struct m0_conf_obj_ops enclosure_ops = {
	.coo_invariant = enclosure_invariant,
	.coo_decode    = enclosure_decode,
	.coo_encode    = enclosure_encode,
	.coo_match     = enclosure_match,
	.coo_lookup    = enclosure_lookup,
	.coo_readdir   = NULL,
	.coo_downlinks = enclosure_downlinks,
	.coo_delete    = enclosure_delete,
};

M0_CONF__CTOR_DEFINE(enclosure_create, m0_conf_enclosure, &enclosure_ops);

const struct m0_conf_obj_type M0_CONF_ENCLOSURE_TYPE = {
	.cot_ftype = {
		.ft_id   = M0_CONF__ENCLOSURE_FT_ID,
		.ft_name = "conf_enclosure"
	},
	.cot_create  = &enclosure_create,
	.cot_xt      = &m0_confx_enclosure_xc,
	.cot_branch  = "u_enclosure",
	.cot_xc_init = &m0_xc_m0_confx_enclosure_struct_init,
	.cot_magic   = M0_CONF_ENCLOSURE_MAGIC
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
