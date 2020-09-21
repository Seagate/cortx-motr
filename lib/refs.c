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


#include "lib/refs.h"

void m0_ref_init(struct m0_ref *ref, int init_num,
		void (*release) (struct m0_ref *ref))
{
	m0_atomic64_set(&ref->ref_cnt, init_num);
	ref->release = release;
	m0_mb();
}

M0_INTERNAL void m0_ref_get(struct m0_ref *ref)
{
	m0_atomic64_inc(&ref->ref_cnt);
	m0_mb();
}

M0_INTERNAL void m0_ref_put(struct m0_ref *ref)
{
	if (m0_atomic64_dec_and_test(&ref->ref_cnt))
		ref->release(ref);
}

M0_INTERNAL int64_t m0_ref_read(const struct m0_ref *ref)
{
	return m0_atomic64_get(&ref->ref_cnt);
}
