/* -*- C -*- */
/*
 * Copyright (c) 2015-2021 Seagate Technology LLC and/or its Affiliates
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



/**
 * @addtogroup be
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include <stdio.h>              /* printf */
#include <stdlib.h>             /* EXIT_FAILURE */
#include <errno.h>              /* ERANGE */

#include "lib/string.h"         /* m0_streq */
#include "lib/errno.h"          /* ENOMEM */
#include "be/tool/st.h"         /* m0_betool_st_mkfs */
#include "be/tool/common.h"     /* m0_betool_m0_init */
#include "be/ut/helper.h"       /* m0_be_ut_backend */
#include "be/alloc_internal.h"  /* m0_be_allocator_header */
#include "stob/ad_private.h"    /* stob_ad_0type_rec */
#include "cob/cob.h"            /* m0_cob_domain */
#include "stob/ad.h"            /* m0_stob_ad_domain */
#include "be/extmap_internal.h" /* m0_be_emap */
#include "balloc/balloc.h"      /* m0_balloc */
#include "be/domain.h"          /*m0_be_segobj_opt_begine,m0_be_segobj_opt_next*/
#include "lib/uuid.h"           /* m0_node_uuid_string_set */

M0_TL_DESCR_DECLARE(zt, M0_EXTERN);
M0_TL_DEFINE(zt, M0_EXTERN, struct m0_be_0type);

static const char *betool_help = ""
"Usage: m0betool [cmd] [path] [size]\n"
"where [cmd] is one from (without quotes):\n"
"- 'st mkfs'\n"
"- 'st run'\n"
"- 'be_recovery_run'\n"
"- 'be_log_resize'\n"
"- 'track_btree'\n"
"- 'print_btree'\n"
"\n"
"Use case for 'st mkfs' and 'st run': run 'st mkfs' once to initialise \n"
"BE data structures, then run 'st run' and kill m0betool process during \n"
"'st run' execution. When 'st run' is called next time BE recovery will \n"
"replay BE log and 'st run' should be able to continue without any issues. \n"
"be_log_resize will create BE log file with custom size. \n"
"This is an ST for BE recovery.\n"
"\n"
"'be_recovery_run' runs BE recovery.\n"
"\n"
"'path' parameter is an optional path to BE domain stob domain location.\n"
"Default BE domain stob domain location is used when this parameter is absent:"
"\n"
"'be_log_resize' create BE log file without mkfs.\n"
"\n"
"'path' and 'size' mandatory arguments for be_log_resize. 'size' in bytes. \n"
"Usage: m0betool be_log_resize <path> <size> \n"
"\n";

extern void btree_dbg_print(struct m0_be_btree *tree);

void track_cob_btrees(struct m0_cob_domain *cdom, bool print_btree)
{
	if (print_btree) {
		M0_LOG(M0_ALWAYS, "cd_object_index ");
		btree_dbg_print((struct m0_be_btree *)cdom->cd_object_index);
		M0_LOG(M0_ALWAYS, "cd_namespace ");
		btree_dbg_print((struct m0_be_btree *)cdom->cd_namespace);
		M0_LOG(M0_ALWAYS, "cd_fileattr_basic ");
		btree_dbg_print((struct m0_be_btree *)cdom->cd_fileattr_basic);
	} else
		M0_LOG(M0_ALWAYS,"M0_BE:COB "
				"cd_object_index btree = %p "
				"cd_namespace btree = %p "
				"cd_fileattr_basic btree = %p "
				"cd_fileattr_omg btree = %p "
				"cd_fileattr_ea btree = %p",
				&cdom->cd_object_index,
				&cdom->cd_namespace,
				&cdom->cd_fileattr_basic,
				&cdom->cd_fileattr_omg,
				&cdom->cd_fileattr_ea);
}

void track_ad_btrees(struct stob_ad_0type_rec *rec, bool print_btree)
{
	struct m0_balloc   *m0balloc;
	struct m0_be_btree *db_ext;
	struct m0_be_btree *db_gd;

	m0balloc = container_of(rec->sa0_ad_domain->sad_ballroom,
				struct m0_balloc, cb_ballroom);
	db_ext = (struct m0_be_btree *)m0balloc->cb_db_group_extents;
	db_gd  = (struct m0_be_btree *)m0balloc->cb_db_group_desc;

	if (print_btree) {
		M0_LOG(M0_ALWAYS, "em_mapping");
		btree_dbg_print(&rec->sa0_ad_domain->sad_adata.em_mapping);
		M0_LOG(M0_ALWAYS, "grp_exts");
		btree_dbg_print(db_ext);
		M0_LOG(M0_ALWAYS, "grp_dsc");
		btree_dbg_print(db_gd);
	} else
		M0_LOG(M0_ALWAYS,"M0_BE:AD em_mapping = %p"
				 "cb_db_group_extents btree= %p "
				 "cb_db_group_desc btree= %p",
				 &rec->sa0_ad_domain->sad_adata.em_mapping,
				 &m0balloc->cb_db_group_extents,
				 &m0balloc->cb_db_group_desc);

}
static void scan_btree(struct m0_be_domain *dom, bool print_btree)
{
	int                     left;
	char                    *suffix;
	struct m0_buf            opt;
	struct m0_be_seg        *seg;
	struct m0_be_0type      *objtype;
	struct m0_cob_domain     *cdom;
	struct stob_ad_0type_rec *rec ;

	seg = m0_be_domain_seg0_get(dom);
	if (seg == NULL) {
		return;
	}

	m0_tl_for(zt, &dom->bd_0types, objtype) {
		for (left = m0_be_segobj_opt_begin(seg, objtype, &opt, &suffix);
		     left > 0 ;
		     left = m0_be_segobj_opt_next(seg, objtype,
						  &opt, &suffix)) {

			M0_LOG(M0_ALWAYS, "object b0_name = '%s' suffix = '%s'"
					  "objtype = %p seg = %p",
					  objtype->b0_name, suffix,
					  objtype, seg );

			if (m0_streq(objtype->b0_name, "M0_BE:COB") == 0) {
				cdom = *(struct m0_cob_domain**)opt.b_addr;
				track_cob_btrees(cdom, print_btree);
			}
			else if (m0_streq(objtype->b0_name, "M0_BE:AD") == 0) {
				rec = (struct stob_ad_0type_rec *)opt.b_addr;
				track_ad_btrees(rec, print_btree);
			}
		}
	} m0_tl_endfor;

}


static void be_recovery_run(char *path)
{
	struct m0_be_ut_backend ut_be = {};
	struct m0_be_domain_cfg cfg = {};
	char                    location[0x100] = {};
	int                     rc;

	m0_betool_m0_init();
	m0_be_ut_backend_cfg_default(&cfg);
	if (path != NULL) {
		snprintf((char *)&location, ARRAY_SIZE(location),
			 "linuxstob:%s", path);
		cfg.bc_stob_domain_location = (char *)&location;
	}
	rc = m0_be_ut_backend_init_cfg(&ut_be, &cfg, false);
	M0_ASSERT_INFO(rc == 0, "rc=%d", rc);

	m0_be_ut_backend_fini(&ut_be);
	m0_betool_m0_fini();
}

static int be_log_resize(char *path, uint64_t size)
{
	struct m0_be_ut_backend ut_be = {};
	struct m0_be_domain_cfg cfg = {};
	char                    location[0x100] = {};
	int                     rc;

	m0_betool_m0_init();
	m0_be_ut_backend_cfg_default(&cfg);

	if (path != NULL) {
		snprintf((char *)&location, ARRAY_SIZE(location),
			 "linuxstob:%s", path);
		cfg.bc_stob_domain_location = (char *)&location;
	}
	cfg.bc_log.lc_store_cfg.lsc_size = size;
	cfg.bc_log.lc_store_cfg.lsc_stob_dont_zero = false;

	rc = m0_be_ut_backend_log_resize(&ut_be, &cfg);
	M0_LOG(M0_DEBUG, "rc=%d", rc);

	m0_betool_m0_fini();
	return rc;
}

static void scan(char *path, char *what)
{
	struct m0_be_ut_backend        ut_be = {};
	struct m0_be_domain_cfg        cfg = {};
	struct m0_be_domain           *dom;
	char                           location[0x100] = {};
	int                            rc;

	m0_node_uuid_string_set(NULL);
	m0_betool_m0_init();
	m0_be_ut_backend_cfg_default(&cfg);
	if (path != NULL) {
		snprintf((char *)&location, ARRAY_SIZE(location),
			 "linuxstob:./%s", path);
		cfg.bc_stob_domain_location = (char *)&location;
		M0_LOG(M0_ALWAYS, "location=%s", cfg.bc_stob_domain_location);
	}
	rc = m0_be_ut_backend_init_cfg(&ut_be, &cfg, false);
	M0_ASSERT_INFO(rc == 0, "rc=%d", rc);
	dom = &ut_be.but_dom;
	if (m0_streq(what, "track_btree"))
		scan_btree(dom, false);
	if (m0_streq(what, "print_btree"))
		scan_btree(dom, true);
	m0_be_ut_backend_fini(&ut_be);
	m0_betool_m0_fini();
}

int main(int argc, char *argv[])
{
	struct m0_be_domain_cfg  cfg = {};
	char                    *path;
	int                      rc;

	if (argc > 1 && m0_streq(argv[1], "be_recovery_run")) {
		path = argc > 2 ? argv[2] : NULL;
		be_recovery_run(path);
		return EXIT_SUCCESS;
	}

	if (argc > 1 && m0_streq(argv[1], "be_log_resize")) {
		uint64_t size;

		if (argc > 3) {
			path = argv[2];
			errno = 0;
			size = strtoul(argv[3], NULL, 10);
		} else {
			printf("%s", betool_help);
			return EXIT_FAILURE;
		}

		if (errno == ERANGE || ((size == 0) && (errno != 0))) {
			printf("%s", betool_help);
			return EXIT_FAILURE;
		}

		rc = be_log_resize(path, size);
		return rc;
	}
	if (argc > 1 && (m0_streq(argv[1], "track_btree") ||
			 m0_streq(argv[1], "print_btree"))) {
		path = argc > 2 ? argv[2] : NULL;
		scan(path, argv[1]);
		return EXIT_SUCCESS;
	}

	if (argc == 3 && m0_streq(argv[1], "st") &&
	    (m0_streq(argv[2], "mkfs") || m0_streq(argv[2], "run"))) {
		if (m0_streq(argv[2], "mkfs") == 0)
			rc = m0_betool_st_mkfs();
		else
			rc = m0_betool_st_run();
	} else {
		m0_betool_m0_init();
		m0_be_ut_backend_cfg_default(&cfg);
		printf("%s", betool_help);
		printf("bc_stob_domain_location=%s\n",
		       cfg.bc_stob_domain_location);
		rc = EXIT_FAILURE;
		m0_betool_m0_fini();
	}
	return rc;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of be group */

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
