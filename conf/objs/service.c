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
#include "conf/onwire_xc.h"  /* m0_confx_service_xc */
#include "motr/magic.h"      /* M0_CONF_SERVICE_MAGIC */
#include "lib/string.h"      /* m0_strings_free */

static bool service_check(const void *bob)
{
	const struct m0_conf_service *self = bob;
	const struct m0_conf_obj     *self_obj = &self->cs_obj;

	M0_PRE(m0_conf_obj_type(self_obj) == &M0_CONF_SERVICE_TYPE);

	return m0_conf_obj_is_stub(self_obj) ||
		_0C(m0_conf_service_type_is_valid(self->cs_type));
}

M0_CONF__BOB_DEFINE(m0_conf_service, M0_CONF_SERVICE_MAGIC, service_check);
M0_CONF__INVARIANT_DEFINE(service_invariant, m0_conf_service);

#define XCAST(xobj) ((struct m0_confx_service *)(&(xobj)->xo_u))
M0_BASSERT(offsetof(struct m0_confx_service, xs_header) == 0);

static int
service_decode(struct m0_conf_obj *dest, const struct m0_confx_obj *src)
{
	int                            rc;
	struct m0_conf_service        *d = M0_CONF_CAST(dest, m0_conf_service);
	const struct m0_confx_service *s = XCAST(src);

	d->cs_type = s->xs_type;
	rc = m0_bufs_to_strings(&d->cs_endpoints, &s->xs_endpoints);
	if (rc != 0)
		return M0_ERR(rc);
	rc = m0_bufs_to_strings(&d->cs_params, &s->xs_params);
	if (rc != 0) {
		m0_strings_free(d->cs_endpoints);
		return M0_RC(rc);
	}
	rc = m0_conf_dir_new(dest, &M0_CONF_SERVICE_SDEVS_FID,
			     &M0_CONF_SDEV_TYPE, &s->xs_sdevs, &d->cs_sdevs);
	if (rc != 0) {
		m0_strings_free(d->cs_endpoints);
		m0_strings_free(d->cs_params);
	}
	return M0_RC(rc);
}

static int
service_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	int                      rc;
	struct m0_conf_service  *s = M0_CONF_CAST(src, m0_conf_service);
	struct m0_confx_service *d = XCAST(dest);

	confx_encode(dest, src);
	d->xs_type = s->cs_type;

	rc = m0_bufs_from_strings(&d->xs_endpoints, s->cs_endpoints);
	if (rc != 0)
		return M0_ERR(-ENOMEM);

	rc = m0_bufs_from_strings(&d->xs_params, s->cs_params);
	if (rc != 0) {
		m0_bufs_free(&d->xs_endpoints);
		return M0_ERR(-ENOMEM);
	}

	rc = arrfid_from_dir(&d->xs_sdevs, s->cs_sdevs);
	if (rc != 0) {
		m0_bufs_free(&d->xs_endpoints);
		m0_bufs_free(&d->xs_params);
	}
	return M0_RC(rc);
}

static bool
service_match(const struct m0_conf_obj *cached, const struct m0_confx_obj *flat)
{
	const struct m0_confx_service *xobj = XCAST(flat);
	const struct m0_conf_service  *obj = M0_CONF_CAST(cached,
							  m0_conf_service);
	M0_PRE(xobj->xs_endpoints.ab_count != 0);

	return obj->cs_type == xobj->xs_type &&
	       m0_bufs_streq(&xobj->xs_endpoints, obj->cs_endpoints) &&
	       m0_conf_dir_elems_match(obj->cs_sdevs, &xobj->xs_sdevs);
}

static int service_lookup(const struct m0_conf_obj *parent,
			  const struct m0_fid *name, struct m0_conf_obj **out)
{
	struct m0_conf_service *svc = M0_CONF_CAST(parent, m0_conf_service);
	const struct conf_dir_relation dirs[] = {
		{ svc->cs_sdevs, &M0_CONF_SERVICE_SDEVS_FID }
	};

	M0_PRE(parent->co_status == M0_CS_READY);
	return M0_RC(conf_dirs_lookup(out, name, dirs, ARRAY_SIZE(dirs)));
}

static const struct m0_fid **service_downlinks(const struct m0_conf_obj *obj)
{
	static const struct m0_fid *rels[] = { &M0_CONF_SERVICE_SDEVS_FID,
					       NULL };
	M0_PRE(m0_conf_obj_type(obj) == &M0_CONF_SERVICE_TYPE);
	return rels;
}

static void service_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_service *x = M0_CONF_CAST(obj, m0_conf_service);

	m0_strings_free(x->cs_endpoints);
	m0_strings_free(x->cs_params);
	m0_conf_service_bob_fini(x);
	m0_free(x);
}

static const struct m0_conf_obj_ops service_ops = {
	.coo_invariant = service_invariant,
	.coo_decode    = service_decode,
	.coo_encode    = service_encode,
	.coo_match     = service_match,
	.coo_lookup    = service_lookup,
	.coo_readdir   = NULL,
	.coo_downlinks = service_downlinks,
	.coo_delete    = service_delete
};

M0_CONF__CTOR_DEFINE(service_create, m0_conf_service, &service_ops);

const struct m0_conf_obj_type M0_CONF_SERVICE_TYPE = {
	.cot_ftype = {
		.ft_id   = M0_CONF__SERVICE_FT_ID,
		.ft_name = "conf_service"
	},
	.cot_create  = &service_create,
	.cot_xt      = &m0_confx_service_xc,
	.cot_branch  = "u_service",
	.cot_xc_init = &m0_xc_m0_confx_service_struct_init,
	.cot_magic   = M0_CONF_SERVICE_MAGIC
};

M0_INTERNAL const char *
m0_conf_service_type2str(enum m0_conf_service_type type)
{
	static const char *names[] = {
#define X_CST(name) [name] = #name,
		M0_CONF_SERVICE_TYPES
#undef X_CST
	};

	M0_PRE(m0_conf_service_type_is_valid(type));
	return names[type];
}

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
