/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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


#include "lib/memory.h"
#include "lib/assert.h"
#include "lib/string.h"
#include "lib/fs.h"
#include "ut/stob.h"		/* m0_ut_stob_create */
#include "ut/ut.h"

#include "stob/partition.h"
#include "stob/domain.h"
#include "stob/io.h"
#include "stob/stob.h"

#include "be/ut/helper.h"
#include "lib/errno.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "be/partition_table.h"
/**
   @addtogroup stob
   @{
 */
#define PART_DEV_NAME "/var/motr/m0ut/ut-sandbox/__s/sdb"
#define PART_DEV_NAME_WITH_ATTRIB "/var/motr/m0ut/ut-sandbox/__s/sdb:directio:true"
#define PART_DEV_SIZE (1073741824UL)             /* 10 GB*/
enum {
	SEG_SIZE               = 1 << 24,
};
extern void m0_stob_ut_ad_part_io(struct m0_stob *back_stob,
				  struct m0_stob_domain *back_domain);
extern struct m0_be_ut_backend ut_be;

void m0_stob_ut_part_init_override(struct m0_be_ut_backend *ut_be,
			  char                    *location,
			  char                    *part_cfg,
			  bool                     part_io)
{
	struct m0_be_part_cfg *pcfg = &ut_be->but_dom.bd_cfg.bc_part_cfg;
	M0_SET0(pcfg);
	/* override default config */
	pcfg->bpc_location = location;
	pcfg->bpc_init_cfg = part_cfg;
	pcfg->bpc_create_cfg = part_cfg;
	pcfg->bpc_part_mode_set = true;
	if(part_io == false) {
		pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_SEG0].bps_enble = true;
		pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_SEG1].bps_enble = true;
		pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_LOG].bps_enble = true;
	}
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_DATA].bps_enble = true;
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_SEG0].bps_id = M0_BE_PTABLE_ENTRY_SEG0;
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_SEG1].bps_id = M0_BE_PTABLE_ENTRY_SEG1;
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_LOG].bps_id = M0_BE_PTABLE_ENTRY_LOG;
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_DATA].bps_id = M0_BE_PTABLE_ENTRY_BALLOC;
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_SEG1].bps_create_cfg =
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_SEG1].bps_init_cfg =
			PART_DEV_NAME;
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_SEG0].bps_create_cfg =
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_SEG0].bps_init_cfg =
			PART_DEV_NAME;
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_LOG].bps_create_cfg =
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_LOG].bps_init_cfg =
			PART_DEV_NAME_WITH_ATTRIB;
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_DATA].bps_create_cfg =
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_DATA].bps_init_cfg =
			PART_DEV_NAME_WITH_ATTRIB;
	pcfg->bpc_chunk_size_in_bits = 21;
	pcfg->bpc_total_chunk_count = PART_DEV_SIZE >> 21;
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_DATA].bps_size_in_chunks =
					(pcfg->bpc_total_chunk_count * 80)/100;
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_LOG].bps_size_in_chunks = 1;
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_SEG0].bps_size_in_chunks = 1;
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_SEG1].bps_size_in_chunks =
					(pcfg->bpc_total_chunk_count * 10)/100;
}

int m0_stob_ut_part_init(struct m0_be_ut_backend *ut_be)
{
	struct m0_be_domain_cfg *pbdcfg = &ut_be->but_dom.bd_cfg;
	int			 rc;

	M0_SET0(ut_be);
	m0_cleandir("./__s");
	rc = mkdir("./__s", 0777);
	M0_ASSERT(rc >= 0);
	if (rc == 0)
	{
		rc = open("./__s/sdb", O_RDWR|O_CREAT, 0700);
		if (rc >= 0)
			rc = close(rc);

	}
	m0_be_ut_backend_cfg_default(pbdcfg);
	rc = m0_be_ut_backend_init_cfg(ut_be, pbdcfg, true);
	return rc;
}

void m0_stob_ut_part_fini(struct m0_be_ut_backend *ut_be)
{
	M0_SET0( &ut_be->but_dom.bd_cfg.bc_part_cfg);
	m0_be_ut_backend_fini(ut_be);
	m0_cleandir("./__s");
}

void m0_stob_ut_part_cfg_make(char                *str,
			      struct m0_be_domain  *dom)
{
	sprintf(str, "%p %s %"PRIu64,
		dom,
		PART_DEV_NAME,
		PART_DEV_SIZE);

}


void m0_stob_ut_stob_part_io(void)
{
	char                    *prefix = "partitionstob";
	char                    *dev_name = "/var/motr/m0ut/ut-sandbox/__s/sdb";
	int rc;
	uint64_t                 dom_key = 0xec0de;
	struct m0_stob_domain   *dom;
	uint64_t                 stob_key = M0_BE_PTABLE_ENTRY_BALLOC;
	char                     location[512]= {0};
	char                     part_cfg[512] = {0};
	struct m0_stob          *stob;
	struct m0_stob_id        stob_id;

	sprintf(location, "%s:%s:%lx", prefix, dev_name,
		(uint64_t)&ut_be.but_dom);
	m0_stob_ut_part_cfg_make(part_cfg, &ut_be.but_dom);
	M0_ASSERT(part_cfg != NULL);
	rc = m0_stob_ut_part_init(&ut_be);
	M0_ASSERT(rc == 0);
	m0_stob_ut_part_init_override(&ut_be, location, part_cfg, true);

	rc = m0_stob_domain_create(location, part_cfg,
				   dom_key, part_cfg, &dom);
	ut_be.but_dom.bd_part_stob_domain = dom;
	M0_UT_ASSERT(rc == 0);
	m0_stob_id_make(0, stob_key, &dom->sd_id, &stob_id);
	rc = m0_stob_find(&stob_id, &stob);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(stob != NULL);
	M0_UT_ASSERT(m0_stob_state_get(stob) == CSS_UNKNOWN);

	rc = m0_stob_locate(stob);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_stob_state_get(stob) == CSS_NOENT);

	rc = m0_ut_stob_create(stob, NULL, &ut_be.but_dom);
	M0_UT_ASSERT(rc == 0);
        m0_stob_ut_ad_part_io(stob, dom);
	m0_stob_destroy(stob, NULL);
	m0_stob_domain_destroy(dom);
	m0_stob_ut_part_fini(&ut_be);
}
/** @} end group stob */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
