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


#include "lib/lockers.h"
#include "ut/ut.h"

M0_LOCKERS_DECLARE(M0_INTERNAL, bank, 2);

struct bank {
	struct bank_lockers vault;
};

M0_LOCKERS_DEFINE(M0_INTERNAL, bank, vault);

static void bank_init(struct bank *bank)
{
	bank_lockers_init(bank);
}

static void bank_fini(struct bank *bank)
{
	bank_lockers_fini(bank);
}

void test_lockers(void)
{
	int         key;
	int         key1;
	char       *valuable = "Gold";
	char       *asset;
	struct bank federal;
	int         i;

	bank_init(&federal);

	key = bank_lockers_allot();
	M0_UT_ASSERT(key == 0);

	key1 = bank_lockers_allot();
	M0_UT_ASSERT(key != key1);

	M0_UT_ASSERT(bank_lockers_is_empty(&federal, key));
	bank_lockers_set(&federal, key, valuable);
	M0_UT_ASSERT(!bank_lockers_is_empty(&federal, key));

	asset = bank_lockers_get(&federal, key);
	M0_UT_ASSERT(asset == valuable);

	bank_lockers_clear(&federal, key);
	M0_UT_ASSERT(bank_lockers_is_empty(&federal, key));

	for (i = 0; i < 1000; ++i) {
		bank_lockers_free(key1);
		key1 = bank_lockers_allot();
		M0_UT_ASSERT(key != key1);
	}
	bank_lockers_free(key);
	bank_lockers_free(key1);
	bank_fini(&federal);
}
M0_EXPORTED(test_lockers);

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
