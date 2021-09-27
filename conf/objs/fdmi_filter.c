/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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
#include "conf/onwire_xc.h"     /* m0_confx_fdmi_filter_xc */
#include "fdmi/filter.h"
#include "motr/magic.h"         /* M0_CONF_FDMI_FILTER_MAGIC */
#include "lib/memory.h"         /* m0_free */
#include "lib/buf.h"            /* m0_buf_strdup */
#include "lib/string.h"         /* m0_strings_free */

static bool fdmi_filter_check(const void *bob)
{
	const struct m0_conf_fdmi_filter *self = bob;
	const struct m0_conf_obj         *self_obj = &self->ff_obj;

	M0_PRE(m0_conf_obj_type(self_obj) == &M0_CONF_FDMI_FILTER_TYPE);

	/** @todo Phase 2: Do checks */
	return true;
}

M0_CONF__BOB_DEFINE(m0_conf_fdmi_filter, M0_CONF_FDMI_FILTER_MAGIC,
		    fdmi_filter_check);

M0_CONF__INVARIANT_DEFINE(fdmi_filter_invariant, m0_conf_fdmi_filter);

#define XCAST(xobj) ((struct m0_confx_fdmi_filter *)(&(xobj)->xo_u))
M0_BASSERT(offsetof(struct m0_confx_fdmi_filter, xf_header) == 0);

static int
fdmi_filter_decode(struct m0_conf_obj *dest, const struct m0_confx_obj *src)
{
	char                              *flt_root_str;
	struct m0_fdmi_flt_node           *flt_root;
	struct m0_conf_obj                *node;
	struct m0_conf_obj                *dix_pver;
	struct m0_conf_fdmi_filter        *d;
	const struct m0_confx_fdmi_filter *s;
	int                                rc;

	M0_ENTRY();

	d = M0_CONF_CAST(dest, m0_conf_fdmi_filter);
	s = XCAST(src);

	d->ff_type = s->xf_type;
	d->ff_filter_id = s->xf_filter_id;
	rc = m0_bufs_to_strings(&d->ff_endpoints, &s->xf_endpoints);
	if (rc != 0)
		return M0_ERR(rc);
	switch (d->ff_type) {
	case M0_FDMI_FILTER_TYPE_TREE:
		rc = m0_conf_obj_find(dest->co_cache, &s->xf_node, &node);
		if (rc != 0) {
			m0_strings_free(d->ff_endpoints);
			return M0_ERR(rc);
		}
		d->ff_node = M0_CONF_CAST(node, m0_conf_node);
		flt_root_str = m0_buf_strdup(&s->xf_filter_root);
		if (flt_root_str == NULL) {
			m0_strings_free(d->ff_endpoints);
			return M0_ERR(-ENOMEM);
		}
		M0_ALLOC_PTR(flt_root);
		if (flt_root == NULL) {
			m0_free(flt_root_str);
			m0_strings_free(d->ff_endpoints);
			return M0_ERR(-ENOMEM);
		}
		m0_fdmi_filter_root_set(&d->ff_filter, flt_root);
		rc = m0_fdmi_flt_node_parse(flt_root_str, flt_root);
		if (rc != 0) {
			M0_ASSERT(d->ff_filter.ff_root == flt_root);
			m0_free0(&d->ff_filter.ff_root);
			m0_strings_free(d->ff_endpoints);
		}
		m0_free(flt_root_str);
		break;
	case M0_FDMI_FILTER_TYPE_KV_SUBSTRING:
		rc = m0_conf_obj_find(dest->co_cache, &s->xf_dix_pver,
				      &dix_pver);
		if (rc != 0) {
			m0_strings_free(d->ff_endpoints);
			return M0_ERR(rc);
		}
		d->ff_dix_pver = M0_CONF_CAST(dix_pver, m0_conf_pver);
		rc = m0_bufs_to_strings(&d->ff_substrings, &s->xf_substrings);
		if (rc != 0) {
			m0_strings_free(d->ff_endpoints);
			return M0_ERR(rc);
		}
		break;
	default:
		M0_IMPOSSIBLE("unknown FDMI filter type %"PRIu32, d->ff_type);
	}
	return M0_RC(rc);
}

static int
fdmi_filter_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	struct m0_conf_fdmi_filter  *s = M0_CONF_CAST(src, m0_conf_fdmi_filter);
	struct m0_confx_fdmi_filter *d = XCAST(dest);
	char                        *str = NULL;
	int                          rc;

	M0_ENTRY();
	confx_encode(dest, src);
	d->xf_type = s->ff_type;
	d->xf_filter_id = s->ff_filter_id;
	rc = m0_bufs_from_strings(&d->xf_endpoints, s->ff_endpoints);
	if (rc != 0)
		return M0_ERR(rc);
	switch (s->ff_type) {
	case M0_FDMI_FILTER_TYPE_TREE:
		d->xf_node = s->ff_node->cn_obj.co_id;
		rc = m0_fdmi_flt_node_print(s->ff_filter.ff_root, &str);
		if (rc != 0) {
			m0_bufs_free(&d->xf_endpoints);
			return M0_ERR(rc);
		}
		rc = m0_buf_copy(&d->xf_filter_root, &M0_BUF_INITS(str));
		m0_free(str);
		break;
	case M0_FDMI_FILTER_TYPE_KV_SUBSTRING:
		d->xf_dix_pver = s->ff_dix_pver->pv_obj.co_id;
		rc = m0_bufs_from_strings(&d->xf_substrings, s->ff_substrings);
		break;
	default:
		M0_IMPOSSIBLE("unknown FDMI filter type %"PRIu32, s->ff_type);
	}
	if (rc != 0)
		m0_bufs_free(&d->xf_endpoints);
	return M0_RC(rc);
}

static bool fdmi_filter_match(const struct m0_conf_obj *cached,
			      const struct m0_confx_obj *flat)
{
	const struct m0_confx_fdmi_filter *xobj = XCAST(flat);
	const struct m0_conf_fdmi_filter  *obj = M0_CONF_CAST(
		cached, m0_conf_fdmi_filter);

	M0_PRE(xobj->xf_endpoints.ab_count != 0);

	return m0_bufs_streq(&xobj->xf_endpoints, obj->ff_endpoints) &&
		m0_bufs_streq(&xobj->xf_substrings, obj->ff_substrings) &&
		m0_fid_eq(&obj->ff_node->cn_obj.co_id, &xobj->xf_node) &&
		m0_fid_eq(&obj->ff_dix_pver->pv_obj.co_id, &xobj->xf_dix_pver);
}

static void fdmi_filter_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_fdmi_filter *x = M0_CONF_CAST(obj, m0_conf_fdmi_filter);

	m0_strings_free(x->ff_endpoints);
	m0_strings_free(x->ff_substrings);
	m0_fdmi_filter_fini(&x->ff_filter);
	m0_conf_fdmi_filter_bob_fini(x);
	m0_free(x);
}

static const struct m0_conf_obj_ops conf_fdmi_filter_ops = {
	.coo_invariant = fdmi_filter_invariant,
	.coo_decode    = fdmi_filter_decode,
	.coo_encode    = fdmi_filter_encode,
	.coo_match     = fdmi_filter_match,
	.coo_lookup    = conf_obj_lookup_denied,
	.coo_readdir   = NULL,
	.coo_downlinks = conf_obj_downlinks_none,
	.coo_delete    = fdmi_filter_delete
};

M0_CONF__CTOR_DEFINE(fdmi_filter_create, m0_conf_fdmi_filter,
		     &conf_fdmi_filter_ops);

const struct m0_conf_obj_type M0_CONF_FDMI_FILTER_TYPE = {
	.cot_ftype = {
		.ft_id   = M0_CONF__FDMI_FILTER_FT_ID,
		.ft_name = "conf_fdmi_filter"
	},
	.cot_create  = &fdmi_filter_create,
	.cot_xt      = &m0_confx_fdmi_filter_xc,
	.cot_branch  = "u_fdmi_filter",
	.cot_xc_init = &m0_xc_m0_confx_fdmi_filter_struct_init,
	.cot_magic   = M0_CONF_FDMI_FILTER_MAGIC
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
