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



/**
 * @addtogroup addb2 ADDB.2
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ADDB

#include "lib/misc.h"            /* NULL */
#include "lib/assert.h"

struct m0_addb2_trace_obj;
struct m0_addb2_storage;
struct m0_addb2_storage_ops;
struct m0_addb2_frame_header;
struct m0_stob;

M0_INTERNAL struct m0_addb2_storage *
m0_addb2_storage_init(const char *location, uint64_t key, bool mkfs, bool force,
		      const struct m0_addb2_storage_ops *ops, m0_bcount_t size,
		      void *cookie)
{
	M0_IMPOSSIBLE("Storage init in kernel mode?");
	return NULL;
}

M0_INTERNAL int m0_addb2_storage_submit(struct m0_addb2_storage *stor,
					struct m0_addb2_trace_obj *obj)
{
	M0_IMPOSSIBLE("Storage submit in kernel mode?");
	return 0;
}

M0_INTERNAL void *m0_addb2_storage_cookie(const struct m0_addb2_storage *stor)
{
	M0_IMPOSSIBLE("Storage cookie in kernel mode?");
	return NULL;
}

M0_INTERNAL void m0_addb2_storage_fini(struct m0_addb2_storage *stor)
{;}

M0_INTERNAL void m0_addb2_storage_stop(struct m0_addb2_storage *stor)
{;}

M0_INTERNAL bool
m0_addb2_storage__is_not_locked(const struct m0_addb2_storage *stor)
{
	return true;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of addb2 group */

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
