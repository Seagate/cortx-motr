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
#include "conf/onwire_xc.h"  /* m0_confx_node_xc */
#include "lib/arith.h"       /* M0_CNT_INC */
#include "motr/magic.h"      /* M0_CONF_NODE_MAGIC */

#define XCAST(xobj) ((struct m0_confx_node *)(&(xobj)->xo_u))
M0_BASSERT(offsetof(struct m0_confx_node, xn_header) == 0);

static bool node_check(const void *bob)
{
	const struct m0_conf_node *self = bob;

	M0_PRE(m0_conf_obj_type(&self->cn_obj) == &M0_CONF_NODE_TYPE);

	return true;
}

M0_CONF__BOB_DEFINE(m0_conf_node, M0_CONF_NODE_MAGIC, node_check);
M0_CONF__INVARIANT_DEFINE(node_invariant, m0_conf_node);

static int node_decode(struct m0_conf_obj *dest, const struct m0_confx_obj *src)
{
	struct m0_conf_node        *d = M0_CONF_CAST(dest, m0_conf_node);
	const struct m0_confx_node *s = XCAST(src);

	d->cn_memsize    = s->xn_memsize;
	d->cn_nr_cpu     = s->xn_nr_cpu;
	d->cn_last_state = s->xn_last_state;
	d->cn_flags      = s->xn_flags;

	return M0_RC(m0_conf_dir_new(dest, &M0_CONF_NODE_PROCESSES_FID,
				     &M0_CONF_PROCESS_TYPE, &s->xn_processes,
				     &d->cn_processes));
}

static int node_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	struct m0_conf_node  *s = M0_CONF_CAST(src, m0_conf_node);
	struct m0_confx_node *d = XCAST(dest);

	confx_encode(dest, src);
	d->xn_memsize    = s->cn_memsize;
	d->xn_nr_cpu     = s->cn_nr_cpu;
	d->xn_last_state = s->cn_last_state;
	d->xn_flags      = s->cn_flags;

	return arrfid_from_dir(&d->xn_processes, s->cn_processes);
}

static bool
node_match(const struct m0_conf_obj *cached, const struct m0_confx_obj *flat)
{
	const struct m0_confx_node *xobj = XCAST(flat);
	const struct m0_conf_node  *obj = M0_CONF_CAST(cached, m0_conf_node);

	return  obj->cn_memsize    == xobj->xn_memsize    &&
		obj->cn_nr_cpu     == xobj->xn_nr_cpu     &&
		obj->cn_last_state == xobj->xn_last_state &&
		obj->cn_flags      == xobj->xn_flags      &&
		m0_conf_dir_elems_match(obj->cn_processes, &xobj->xn_processes);
}

static int node_lookup(const struct m0_conf_obj *parent,
		       const struct m0_fid *name, struct m0_conf_obj **out)
{
	struct m0_conf_node *node = M0_CONF_CAST(parent, m0_conf_node);
	const struct conf_dir_relation dirs[] = {
		{ node->cn_processes, &M0_CONF_NODE_PROCESSES_FID }
	};

	M0_PRE(parent->co_status == M0_CS_READY);
	return M0_RC(conf_dirs_lookup(out, name, dirs, ARRAY_SIZE(dirs)));
}

static const struct m0_fid **node_downlinks(const struct m0_conf_obj *obj)
{
	static const struct m0_fid *rels[] = { &M0_CONF_NODE_PROCESSES_FID,
					       NULL };
	M0_PRE(m0_conf_obj_type(obj) == &M0_CONF_NODE_TYPE);
	return rels;
}

static void node_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_node *x = M0_CONF_CAST(obj, m0_conf_node);

	m0_conf_node_bob_fini(x);
	m0_free(x);
}

static const struct m0_conf_obj_ops node_ops = {
	.coo_invariant = node_invariant,
	.coo_decode    = node_decode,
	.coo_encode    = node_encode,
	.coo_match     = node_match,
	.coo_lookup    = node_lookup,
	.coo_readdir   = NULL,
	.coo_downlinks = node_downlinks,
	.coo_delete    = node_delete
};

M0_CONF__CTOR_DEFINE(node_create, m0_conf_node, &node_ops);

const struct m0_conf_obj_type M0_CONF_NODE_TYPE = {
	.cot_ftype = {
		.ft_id   = M0_CONF__NODE_FT_ID,
		.ft_name = "conf_node"
	},
	.cot_create  = &node_create,
	.cot_xt      = &m0_confx_node_xc,
	.cot_branch  = "u_node",
	.cot_xc_init = &m0_xc_m0_confx_node_struct_init,
	.cot_magic   = M0_CONF_NODE_MAGIC
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
