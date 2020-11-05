/* -*- C -*- */
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "conf/ut/common.h"
#include "lib/fs.h"         /* m0_file_read */
#include "rpc/rpclib.h"     /* m0_rpc_server_ctx */
#include "net/lnet/lnet.h"  /* m0_net_lnet_xprt */
#include "ut/ut.h"

struct m0_conf_cache m0_conf_ut_cache;
struct m0_sm_group   m0_conf_ut_grp;
struct m0_net_xprt  *m0_conf_ut_xprt = &m0_net_xprt_obj;

/* Filters out intermediate state transitions of m0_confc_ctx::fc_mach. */
static bool _filter(struct m0_clink *link)
{
	struct m0_conf_ut_waiter *waiter = M0_AMB(waiter, link, w_clink);
	return !m0_confc_ctx_is_completed(&waiter->w_ctx);
}

M0_INTERNAL void m0_conf_ut_waiter_init(struct m0_conf_ut_waiter *w,
					struct m0_confc *confc)
{
	m0_confc_ctx_init(&w->w_ctx, confc);
	m0_clink_init(&w->w_clink, _filter);
	m0_clink_add_lock(&w->w_ctx.fc_mach.sm_chan, &w->w_clink);
}

M0_INTERNAL void m0_conf_ut_waiter_fini(struct m0_conf_ut_waiter *w)
{
	m0_clink_del_lock(&w->w_clink);
	m0_clink_fini(&w->w_clink);
	m0_confc_ctx_fini(&w->w_ctx);
}

M0_INTERNAL int m0_conf_ut_waiter_wait(struct m0_conf_ut_waiter *w,
				       struct m0_conf_obj **result)
{
	int rc;

	while (!m0_confc_ctx_is_completed_lock(&w->w_ctx))
		m0_chan_wait(&w->w_clink);

	rc = m0_confc_ctx_error_lock(&w->w_ctx);
	if (rc == 0 && result != NULL)
		*result = m0_confc_ctx_result(&w->w_ctx);

	return rc;
}

static struct conf_ut_ast {
	bool             run;
	struct m0_thread thread;
} g_ast;

static void conf_ut_ast_thread(int _ M0_UNUSED)
{
	while (g_ast.run) {
		m0_chan_wait(&m0_conf_ut_grp.s_clink);
		m0_sm_group_lock(&m0_conf_ut_grp);
		m0_sm_asts_run(&m0_conf_ut_grp);
		m0_sm_group_unlock(&m0_conf_ut_grp);
	}
}

M0_INTERNAL int m0_conf_ut_ast_thread_init(void)
{
	M0_SET0(&m0_conf_ut_grp);
	M0_SET0(&g_ast);
	m0_sm_group_init(&m0_conf_ut_grp);
	g_ast.run = true;
	return M0_THREAD_INIT(&g_ast.thread, int, NULL, &conf_ut_ast_thread, 0,
			      "ast_thread");
}

M0_INTERNAL int m0_conf_ut_ast_thread_fini(void)
{
	g_ast.run = false;
	m0_clink_signal(&m0_conf_ut_grp.s_clink);
	m0_thread_join(&g_ast.thread);
	m0_sm_group_fini(&m0_conf_ut_grp);
	return 0;
}

static struct m0_mutex conf_ut_lock;

M0_INTERNAL int m0_conf_ut_cache_init(void)
{
	m0_mutex_init(&conf_ut_lock);
	m0_conf_cache_init(&m0_conf_ut_cache, &conf_ut_lock);
	return 0;
}

M0_INTERNAL int m0_conf_ut_cache_fini(void)
{
	m0_conf_cache_fini(&m0_conf_ut_cache);
	m0_mutex_fini(&conf_ut_lock);
	return 0;
}

#ifndef __KERNEL__
M0_INTERNAL void
m0_conf_ut_cache_from_file(struct m0_conf_cache *cache, const char *path)
{
	char *confstr = NULL;
	int   rc;

	M0_PRE(path != NULL && *path != '\0');

	rc = m0_file_read(path, &confstr);
	M0_UT_ASSERT(rc == 0);

	m0_conf_cache_lock(cache);
	m0_conf_cache_clean(cache, NULL); /* start from scratch */
	rc = m0_conf_cache_from_string(cache, confstr);
	M0_UT_ASSERT(rc == 0);
	m0_conf_cache_unlock(cache);

	m0_free(confstr);
}
#endif /* !__KERNEL__ */

#undef M0_TRACE_SUBSYSTEM
