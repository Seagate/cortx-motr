/* -*- C -*- */
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"
#include "lib/finject.h" /* MO_FI_ENABLED  */

#include "filterc.h"

#ifndef __KERNEL__
static int m0_filterc_start(struct m0_filterc_ctx 	*ctx,
                            struct m0_reqh        	*reqh);
static void m0_filterc_stop(struct m0_filterc_ctx *ctx);
static int m0_filterc_open(struct m0_filterc_ctx    *ctx,
                           enum m0_fdmi_rec_type_id  rec_type_id,
                           struct m0_filterc_iter   *iter);
static int m0_filterc_get_next(struct m0_filterc_iter     *iter,
                               struct m0_conf_fdmi_filter **out);
static void m0_filterc_close(struct m0_filterc_iter *iter);


const struct m0_filterc_ops filterc_def_ops = {
	.fco_start     = m0_filterc_start,
	.fco_stop      = m0_filterc_stop,
	.fco_open      = m0_filterc_open,
	.fco_get_next  = m0_filterc_get_next,
	.fco_close     = m0_filterc_close
};

M0_INTERNAL void m0_filterc_ctx_init(struct m0_filterc_ctx       *ctx,
				     const struct m0_filterc_ops *ops)

{
	M0_ENTRY();
	M0_PRE(M0_IS0(ctx));
	ctx->fcc_ops = ops;
	M0_LEAVE();
}

/** Starts filterc. */
static int m0_filterc_start(struct m0_filterc_ctx  *ctx,
			    struct m0_reqh         *reqh)
{
	M0_ENTRY();
	M0_PRE(reqh != NULL);

	ctx->fcc_confc = m0_reqh2confc(reqh);
	if (ctx->fcc_confc == NULL)
		return M0_RC(-EINVAL);
	return M0_RC(0);

}

static void m0_filterc_stop(struct m0_filterc_ctx *ctx)
{
	M0_ENTRY();
	ctx->fcc_confc = NULL;
	M0_LEAVE();
}

M0_INTERNAL void m0_filterc_ctx_fini(struct m0_filterc_ctx *ctx)
{
	M0_ENTRY();
	M0_LEAVE();
}

static int open_filter_group(struct m0_filterc_ctx    *ctx,
                             enum m0_fdmi_rec_type_id  rec_type_id,
			     struct m0_conf_obj      **out)
{
	int                 rc;
	struct m0_conf_obj *flt_grp = NULL;
	struct m0_conf_obj *flt_grp_tmp;
	struct m0_conf_obj *flt_grp_dir;
	struct m0_conf_obj *flts_dir;

	M0_ENTRY();

	/** Some setups dont have conf or its root (example: cas-service ut) */
	if (ctx->fcc_confc == NULL || ctx->fcc_confc->cc_root == NULL)
		return M0_RC(-EINVAL);

	rc = m0_confc_open_sync(&flt_grp_dir, ctx->fcc_confc->cc_root,
				M0_CONF_ROOT_FDMI_FLT_GRPS_FID);
	if (rc != 0)
		goto open_err;
	for (flt_grp_tmp = NULL;
	     (rc = m0_confc_readdir_sync(flt_grp_dir, &flt_grp_tmp)) > 0; ) {
		struct m0_conf_fdmi_flt_grp *grp =
			M0_CONF_CAST(flt_grp_tmp, m0_conf_fdmi_flt_grp);

		if (grp->ffg_rec_type == rec_type_id) {
			flt_grp = flt_grp_tmp;
			break;
		}
		/** @todo Close opened groups? */
	}

	if (flt_grp != NULL) {
		rc = m0_confc_open_sync(&flts_dir, flt_grp,
					M0_CONF_FDMI_FGRP_FILTERS_FID);
		if (rc == 0)
			*out = flts_dir;
	} else {
		rc = (rc < 0) ? rc : -ENOENT;
	}

	m0_confc_close(flt_grp_tmp);
	m0_confc_close(flt_grp_dir);
open_err:
	return M0_RC(rc);
}

static int m0_filterc_open(struct m0_filterc_ctx    *ctx,
                           enum m0_fdmi_rec_type_id  rec_type_id,
                           struct m0_filterc_iter   *iter)
{
	int rc;

	M0_ENTRY();
	rc = open_filter_group(ctx, rec_type_id, &iter->fci_dir);

	if (rc != 0)
		goto find_err;

	iter->fci_filterc_ctx = ctx;
	iter->fci_cur_flt = NULL;
find_err:
	return M0_RC(rc);
}

static int m0_filterc_get_next(struct m0_filterc_iter     *iter,
                               struct m0_conf_fdmi_filter **out)
{
	int rc;

	M0_ENTRY();
	rc = m0_confc_readdir_sync((struct m0_conf_obj *)iter->fci_dir,
	                           (struct m0_conf_obj **)&iter->fci_cur_flt);

	if (rc > 0) {
		*out = iter->fci_cur_flt;
	}

	return M0_RC(rc);
}

static void m0_filterc_close(struct m0_filterc_iter *iter)
{
	M0_ENTRY();
	m0_confc_close((struct m0_conf_obj *)iter->fci_cur_flt);
	m0_confc_close((struct m0_conf_obj *)iter->fci_dir);
	M0_LEAVE();
}

#endif /* __KERNEL__ */

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
