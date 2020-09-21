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

#include "reqh/reqh.h"
#include "fdmi/filterc.h"

static int filterc_stub_start(struct m0_filterc_ctx 	*ctx,
			      struct m0_reqh        	*reqh);

static void filterc_stub_stop(struct m0_filterc_ctx *ctx);

static int filterc_stub_open(struct m0_filterc_ctx  *ctx,
                           enum m0_fdmi_rec_type_id  rec_type_id,
                           struct m0_filterc_iter   *iter);

static int filterc_stub_get_next(struct m0_filterc_iter     *iter,
                               struct m0_conf_fdmi_filter **out);

static void filterc_stub_close(struct m0_filterc_iter *iter);

const struct m0_filterc_ops filterc_stub_ops = {
	.fco_start     = filterc_stub_start,
	.fco_stop      = filterc_stub_stop,
	.fco_open      = filterc_stub_open,
	.fco_get_next  = filterc_stub_get_next,
	.fco_close     = filterc_stub_close
};

static int filterc_stub_start(struct m0_filterc_ctx 	*ctx,
			      struct m0_reqh        	*reqh)
{
	return 0;
}

static void filterc_stub_stop(struct m0_filterc_ctx *ctx)
{
}

static int filterc_stub_open(struct m0_filterc_ctx  *ctx,
                           enum m0_fdmi_rec_type_id  rec_type_id,
                           struct m0_filterc_iter   *iter)
{
	return 0;
}

static int filterc_stub_get_next(struct m0_filterc_iter     *iter,
                               struct m0_conf_fdmi_filter **out)
{
	*out = NULL;
	return 0;
}

static void filterc_stub_close(struct m0_filterc_iter *iter)
{
}

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
