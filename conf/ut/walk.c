/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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


#include "conf/walk.h"
#include "conf/ut/common.h"  /* m0_conf_ut_cache_from_file */
#include "ut/misc.h"         /* M0_UT_PATH */
#include "ut/ut.h"

static int conf_ut_count_nondirs(struct m0_conf_obj *obj, void *args)
{
	if (m0_conf_obj_type(obj) != &M0_CONF_DIR_TYPE)
		++*(unsigned *)args;
	return M0_CW_CONTINUE;
}

static void test_conf_walk(void)
{
	struct m0_conf_cache *cache = &m0_conf_ut_cache;
	struct m0_conf_obj   *root;
	unsigned              n = 0;
	int                   rc;

	m0_conf_ut_cache_from_file(cache, M0_UT_PATH("conf.xc"));
	m0_conf_cache_lock(cache);
	root = m0_conf_cache_lookup(cache, &M0_CONF_ROOT_FID);
	M0_UT_ASSERT(root != NULL);
	rc = m0_conf_walk(conf_ut_count_nondirs, root, &n);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(n == M0_UT_CONF_NR_OBJS);
	m0_conf_cache_unlock(cache);

	/* XXX TODO: add more tests */
}

struct m0_ut_suite conf_walk_ut = {
	.ts_name  = "conf-walk-ut",
	.ts_init  = m0_conf_ut_cache_init,
	.ts_fini  = m0_conf_ut_cache_fini,
	.ts_tests = {
		{ "walk", test_conf_walk },
		{ NULL, NULL }
	}
};
