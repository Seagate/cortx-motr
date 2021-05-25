/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_ADDB2_UT_COMMON_H__
#define __MOTR_ADDB2_UT_COMMON_H__

/**
 * @defgroup addb2
 *
 * @{
 */

#include "lib/types.h"
#include "addb2/addb2.h"
#include "addb2/consumer.h"
#include "addb2/storage.h"

extern int submitted;

extern int (*submit)(const struct m0_addb2_mach *mach,
		     struct m0_addb2_trace *trace);
extern void (*idle)(const struct m0_addb2_mach *mach);
struct m0_addb2_mach *mach_set(int (*s)(const struct m0_addb2_mach  *,
					struct m0_addb2_trace *));
extern const struct m0_addb2_sensor_ops sensor_ops;
extern const uint64_t SENSOR_MARKER;
extern uint64_t seq;
extern bool sensor_finalised;

void mach_fini(struct m0_addb2_mach *m);
void mach_put(struct m0_addb2_mach *m);
int  fill_one(struct m0_addb2_mach *m);

/* define a smaller record type to fit into kernel stack frame. */
struct small_record {
	struct m0_addb2_value ar_val;
	unsigned              ar_label_nr;
	struct m0_addb2_value ar_label[4];
};

#define VAL(id, ...) {							\
	.va_id   = (id),						\
	.va_nr   = ARRAY_SIZE(((const uint64_t[]) { __VA_ARGS__ })),	\
	.va_data = ((const uint64_t[]) { __VA_ARGS__ } )		\
}

bool valeq(const struct m0_addb2_value *v0, const struct m0_addb2_value *v1);
bool receq(const struct m0_addb2_record *r0, const struct small_record *r1);

/** @} end of addb2 group */
#endif /* __MOTR_ADDB2_UT_COMMON_H__ */

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
