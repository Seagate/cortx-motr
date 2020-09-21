/* -*- C -*- */
/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
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


#pragma once

#ifndef __MOTR_UT_STOB_H__
#define __MOTR_UT_STOB_H__

#include "lib/types.h"		/* uint64_t */
#include "module/module.h"	/* m0_module */

/**
 * @defgroup utstob
 *
 * @{
 */

struct m0_stob;
struct m0_stob_domain;
struct m0_be_tx_credit;
struct ut_stob_module;
struct m0_stob_id;
struct m0_be_domain;

enum {
	M0_LEVEL_UT_STOB,
};

struct m0_ut_stob_module {
	struct m0_module       usm_module;
	struct ut_stob_module *usm_private;
};

extern struct m0_modlev m0_levels_ut_stob[];
extern const unsigned m0_levels_ut_stob_nr;

M0_INTERNAL struct m0_stob *m0_ut_stob_linux_get(void);
M0_INTERNAL struct m0_stob *m0_ut_stob_linux_create(char *stob_create_cfg);
M0_INTERNAL struct m0_stob *m0_ut_stob_linux_get_by_key(uint64_t stob_key);
M0_INTERNAL void m0_ut_stob_put(struct m0_stob *stob, bool destroy);

M0_INTERNAL int m0_ut_stob_create(struct m0_stob *stob, const char *str_cfg,
				  struct m0_be_domain *be_dom);
M0_INTERNAL int m0_ut_stob_destroy(struct m0_stob *stob,
				   struct m0_be_domain *be_dom);
M0_INTERNAL struct m0_stob *m0_ut_stob_open(struct m0_stob_domain *dom,
					    uint64_t stob_key,
					    const char *str_cfg);
M0_INTERNAL void m0_ut_stob_close(struct m0_stob *stob, bool destroy);

M0_INTERNAL int m0_ut_stob_create_by_stob_id(struct m0_stob_id *stob_id,
					     const char *str_cfg);
M0_INTERNAL int m0_ut_stob_destroy_by_stob_id(struct m0_stob_id *stob_id);

/* XXX move somewhere else */
M0_INTERNAL struct m0_dtx *m0_ut_dtx_open(struct m0_be_tx_credit *cred,
					  struct m0_be_domain    *be_dom);
M0_INTERNAL void m0_ut_dtx_close(struct m0_dtx *dtx);

M0_INTERNAL int m0_ut_stob_init(void);
M0_INTERNAL void m0_ut_stob_fini(void);

/** @} end of utstob group */
#endif /* __MOTR_UT_STOB_H__ */

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
