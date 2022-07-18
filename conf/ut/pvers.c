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


#include "conf/pvers.h"
#include "conf/helpers.h"    /* m0_conf_pvers */
#include "conf/ut/common.h"  /* m0_conf_ut_cache */
#include "lib/errno.h"       /* ENOENT */
#include "ut/ut.h"

static void test_pver_fid(void)
{
	const struct {
		enum m0_conf_pver_kind t_kind;
		uint64_t               t_container;
		uint64_t               t_key;
	} tests[] = {
		{ M0_CONF_PVER_ACTUAL,    1, 25 },
		{ M0_CONF_PVER_FORMULAIC, 1, 25 },
		{ M0_CONF_PVER_VIRTUAL,   1, 25 },
		/* XXX TODO: add more tests */
	};
	struct m0_fid          fid;
	enum m0_conf_pver_kind kind;
	uint64_t               container;
	uint64_t               key;
	unsigned               i;
	int                    rc;

	for (i = 0; i < ARRAY_SIZE(tests); ++i) {
		/* encode */
		fid = m0_conf_pver_fid(tests[i].t_kind, tests[i].t_container,
				       tests[i].t_key);
		M0_UT_ASSERT(m0_conf_fid_type(&fid) == &M0_CONF_PVER_TYPE);
		/* decode */
		rc = m0_conf_pver_fid_read(&fid, &kind, &container, &key);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(kind      == tests[i].t_kind);
		M0_UT_ASSERT(container == tests[i].t_container);
		M0_UT_ASSERT(key       == tests[i].t_key);
	}
}

static void conf_ut_ha_state_set(const struct m0_conf_cache *cache,
				 const struct m0_fid *objid,
				 enum m0_ha_obj_state state)
{
	struct m0_conf_obj          *obj;
	struct m0_conf_pver_subtree *pvsub;

	obj = m0_conf_cache_lookup(cache, objid);
	M0_UT_ASSERT(obj != NULL);
	M0_UT_ASSERT(obj->co_ha_state != state);
	obj->co_ha_state = state;

	pvsub = &m0_conf_pvers(obj)[0]->pv_u.subtree;
	if (state == M0_NC_ONLINE)
		M0_CNT_DEC(pvsub->pvs_recd[m0_conf_pver_level(obj)]);
	else
		M0_CNT_INC(pvsub->pvs_recd[m0_conf_pver_level(obj)]);
}

static void test_pver_find(void)
{
	const struct m0_fid base = M0_FID_TINIT('v', 1, 0); /* pver-0 */
	const struct m0_fid failed[] = {
		M0_FID_TINIT('k', 1, 2), /* disk-2 */
		M0_FID_TINIT('k', 1, 6), /* disk-6 */
		M0_FID_TINIT('c', 1, 1)  /* controller-1 */
	};
	struct m0_conf_cache *cache = &m0_conf_ut_cache;
	struct m0_conf_pool  *pool;
	struct m0_conf_pver  *pver;
	struct m0_conf_pver  *pver_virt;
	struct m0_conf_root  *root;
	unsigned              i;
	const uint32_t        tolvec[M0_CONF_PVER_HEIGHT] = {0, 0, 0, 1, 2};
	int                   rc;

	m0_conf_ut_cache_from_file(cache, M0_SRC_PATH("conf/ut/pvers.xc"));
	/*
	 * m0_conf_pver_find() tests.
	 */
	pool = M0_CONF_CAST(m0_conf_cache_lookup(cache, /* pool-0 */
						 &M0_FID_TINIT('o', 1, 0)),
			    m0_conf_pool);
	rc = m0_conf_pver_find(pool, NULL, &pver);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(pver->pv_kind == M0_CONF_PVER_ACTUAL);
	M0_UT_ASSERT(m0_fid_eq(&pver->pv_obj.co_id, &base));
	M0_UT_ASSERT(M0_IS0(&pver->pv_u.subtree.pvs_recd));

	rc = m0_conf_pver_find(pool, &pver->pv_obj.co_id, &pver);
	M0_UT_ASSERT(rc == -ENOENT);

	conf_ut_ha_state_set(cache, &failed[0], M0_NC_FAILED);
	rc = m0_conf_pver_find(pool, NULL, &pver);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(pver->pv_kind == M0_CONF_PVER_ACTUAL);
	for (i = 1; i < ARRAY_SIZE(failed); ++i)
		conf_ut_ha_state_set(cache, &failed[i], M0_NC_FAILED);
	rc = m0_conf_pver_find(pool, NULL, &pver_virt);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(pver_virt->pv_kind == M0_CONF_PVER_VIRTUAL);
	M0_UT_ASSERT(!m0_fid_eq(&pver_virt->pv_obj.co_id, &base));
	M0_UT_ASSERT(pver_virt->pv_u.subtree.pvs_attr.pa_P <
		     pver->pv_u.subtree.pvs_attr.pa_P);

	rc = m0_conf_pver_find(pool, NULL, &pver);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(pver == pver_virt);
	M0_UT_ASSERT(!memcmp(pver->pv_u.subtree.pvs_tolerance,
			     tolvec, sizeof tolvec));
	for (i = 0; i < ARRAY_SIZE(failed); ++i)
		conf_ut_ha_state_set(cache, &failed[i], M0_NC_ONLINE);
	rc = m0_conf_pver_find(pool, NULL, &pver);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(pver->pv_kind == M0_CONF_PVER_ACTUAL);
	M0_UT_ASSERT(!memcmp(pver->pv_u.subtree.pvs_tolerance,
			     tolvec, sizeof tolvec));

	/*
	 * m0_conf_pver_find_by_fid() tests.
	 */
	root = M0_CONF_CAST(m0_conf_cache_lookup(cache, /* root-0 */
						 &M0_FID_TINIT('t', 1, 0)),
			    m0_conf_root);
	rc = m0_conf_pver_find_by_fid(&pver_virt->pv_obj.co_id, root, &pver);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(pver == pver_virt);
	{
		const struct m0_conf_pver *fpver;

		rc = m0_conf_pver_formulaic_from_virtual(pver_virt, root,
							 &fpver);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(fpver->pv_kind == M0_CONF_PVER_FORMULAIC);
		M0_UT_ASSERT(m0_fid_eq(&fpver->pv_obj.co_id,
				       /* pver_f-0 */
				       &M0_FID_INIT(0x7640000000000001, 0)));
	}
	/*
	 * Now let us request a slightly different fid.
	 *
	 * The value added to .f_key (1) is carefully chosen to be compatible
	 * with the structure of base pver subtree and the allowance vector.
	 */
	rc = m0_conf_pver_find_by_fid(
		&M0_FID_INIT(pver_virt->pv_obj.co_id.f_container,
			     pver_virt->pv_obj.co_id.f_key + 1), root, &pver);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(pver->pv_kind == M0_CONF_PVER_VIRTUAL);
	M0_UT_ASSERT(pver != pver_virt);
	M0_UT_ASSERT(pver->pv_u.subtree.pvs_attr.pa_P ==
		     pver_virt->pv_u.subtree.pvs_attr.pa_P);
	rc = m0_conf_pver_find_by_fid(
		&M0_FID_INIT(pver_virt->pv_obj.co_id.f_container,
			     /*
			      * This cid is not compatible with the
			      * allowance vector.
			      */
			     pver_virt->pv_obj.co_id.f_key + 2), root, &pver);
	M0_UT_ASSERT(rc == -EINVAL);
}

struct m0_ut_suite conf_pvers_ut = {
	.ts_name  = "conf-pvers-ut",
	.ts_init  = m0_conf_ut_cache_init,
	.ts_fini  = m0_conf_ut_cache_fini,
	.ts_tests = {
		{ "fid",         test_pver_fid },
		{ "pver-find",   test_pver_find },
		{ NULL, NULL }
	}
};
