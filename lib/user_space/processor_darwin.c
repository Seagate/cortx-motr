/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

#if defined(M0_DARWIN)

#include <sys/types.h>
#include <sys/sysctl.h>

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER
#include "lib/trace.h"
#include "lib/errno.h"
#include "lib/processor.h"

/**
 * @addtogroup processor
 *
 * @{
 */

#define SYSCTL(name, ttype)					\
({								\
	ttype  __v;						\
	size_t __size = sizeof __v;				\
	int    __result;					\
								\
	__result = sysctlbyname(name, &__v, &__size, NULL, 0);	\
	M0_ASSERT(__result == 0);				\
	M0_ASSERT(__size == sizeof __v);			\
	__v;							\
})

M0_INTERNAL int m0_processors_init(void)
{
	return 0;
}

M0_INTERNAL void m0_processors_fini(void)
{}

M0_INTERNAL m0_processor_nr_t m0_processor_nr_max(void)
{
	return 1; /* SYSCTL("hw.logicalcpu_max", int); */
}

M0_INTERNAL void m0_processors_possible(struct m0_bitmap *map)
{
	int ncpu = m0_processor_nr_max();
	int i;

	for (i = 0; i < ncpu; ++i)
		m0_bitmap_set(map, i, true);
}

M0_INTERNAL void m0_processors_available(struct m0_bitmap *map)
{
	m0_processors_possible(map);
}

M0_INTERNAL void m0_processors_online(struct m0_bitmap *map)
{
	m0_processors_possible(map);
}

M0_INTERNAL m0_processor_nr_t m0_processor_id_get(void)
{
	return 0; /* Can we do better? */
}

M0_INTERNAL int m0_processor_describe(m0_processor_nr_t id,
				      struct m0_processor_descr *pd)
{
	return M0_ERR(-ENOSYS);
}

#undef M0_TRACE_SUBSYSTEM

/* M0_DARWIN */
#endif

/** @} end of processor group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
