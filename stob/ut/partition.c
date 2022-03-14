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

/**
   @addtogroup stob
   @{
 */
#define PART_DEV_NAME "/var/motr/m0ut/ut-sandbox/__s/sdb"
#define PART_DEV_SIZE (1073741824UL)             /* 10 GB*/
enum {
	SEG_SIZE               = 1 << 24,
};


void m0_stob_ut_part_init_override(struct m0_be_ut_backend *ut_be,
			  char                    *location,
			  char                    *part_cfg)
{
	struct m0_be_part_cfg *pcfg = &ut_be->but_dom.bd_cfg.bc_part_cfg;
	M0_SET0(pcfg);
	/* override default config */
	pcfg->bpc_location = location;
	pcfg->bpc_init_cfg = part_cfg;
	pcfg->bpc_create_cfg = part_cfg;
	pcfg->bpc_part_mode_set = true;
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_SEG0].bps_enble = true;
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_SEG1].bps_enble = true;
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_LOG].bps_enble = true;
	pcfg->bpc_stobs_cfg[M0_BE_DOM_PART_IDX_DATA].bps_enble = true;
	pcfg->bpc_chunk_size_in_bits = 21;
	pcfg->bpc_total_chunk_count = PART_DEV_SIZE >> 21;
}

int m0_stob_ut_part_init(struct m0_be_ut_backend *ut_be)
{
	struct m0_be_domain_cfg  cfg;
	int			 rc;

	M0_SET0(ut_be);
	rmdir("./__s");
	rc = mkdir("./__s", 0777);
	if (rc == 0)
	{
		rc = open("./__s/sdb", O_RDWR|O_CREAT, 0700);
		if (rc >= 0)
			rc = close(rc);

	}
	m0_be_ut_backend_cfg_default(&cfg);
	rc = m0_be_ut_backend_init_cfg(ut_be, &cfg, true);
	return rc;
}

void m0_stob_ut_part_fini(struct m0_be_ut_backend *ut_be)
{
	M0_SET0( &ut_be->but_dom.bd_cfg.bc_part_cfg);
	m0_be_ut_backend_fini(ut_be);
	rmdir("./__s");
}

void m0_stob_ut_part_cfg_make(char                *str,
			      struct m0_be_domain  *dom)
{
	sprintf(str, "%p %s %"PRIu64,
		dom,
		PART_DEV_NAME,
		PART_DEV_SIZE);

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
