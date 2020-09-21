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

#ifndef __MOTR_ADDB2_COUNTER_H__
#define __MOTR_ADDB2_COUNTER_H__

/**
 * @addtogroup addb2
 *
 * @{
 */

#include "lib/types.h"
#include "lib/tlist.h"
#include "addb2/addb2.h"

struct m0_tl;

struct m0_addb2_counter_data {
	uint64_t cod_nr;
	int64_t  cod_min;
	int64_t  cod_max;
	int64_t  cod_sum;
	uint64_t cod_ssq;
	uint64_t cod_datum;
};

enum {
	M0_ADDB2_COUNTER_VALS =
		sizeof(struct m0_addb2_counter_data) / sizeof(uint64_t)
};

struct m0_addb2_counter {
	struct m0_addb2_sensor       co_sensor;
	struct m0_addb2_counter_data co_val;
};

void m0_addb2_counter_add(struct m0_addb2_counter *counter, uint64_t label,
			  int idx);
void m0_addb2_counter_del(struct m0_addb2_counter *counter);
void m0_addb2_counter_mod(struct m0_addb2_counter *counter, int64_t val);
void m0_addb2_counter_mod_with(struct m0_addb2_counter *counter,
			       int64_t val, uint64_t datum);

struct m0_addb2_list_counter {
	struct m0_addb2_sensor  lc_sensor;
	struct m0_tl           *lc_list;
};

void m0_addb2_list_counter_add(struct m0_addb2_list_counter *counter,
			       struct m0_tl *list, uint64_t label, int idx);
void m0_addb2_list_counter_del(struct m0_addb2_list_counter *counter);

void m0_addb2_clock_add(struct m0_addb2_sensor *clock, uint64_t label, int idx);
void m0_addb2_clock_del(struct m0_addb2_sensor *clock);

/** Common part of M0_ADDB2_TIMED() and M0_ADDB2_HIST(). */
#define M0_ADDB2_TIMED_0(id, datum, ...)			\
	m0_time_t __start = m0_time_now();			\
	m0_time_t __end;					\
	m0_time_t __duration;					\
	uint64_t  __datum = (datum);				\
	uint64_t  __id    = (id);				\
	__VA_ARGS__;						\
	__end = m0_time_now();					\
	__duration = (__end - __start) >> 10;			\
	if (__id != 0)						\
		M0_ADDB2_ADD(__id, __duration, __datum);	\

#define M0_ADDB2_TIMED(id, counter, datum, ...)			\
do {									\
	struct m0_addb2_counter *__counter = (counter);		\
	M0_ADDB2_TIMED_0((id), (datum), __VA_ARGS__);			\
	if (__counter != NULL)						\
		m0_addb2_counter_mod_with(__counter, __duration, __datum); \
} while (0)

struct m0_addb2_local_counter {
	uint64_t lc_id;
	int      lc_key;
};

int m0_addb2_local_counter_init(struct m0_addb2_local_counter *lc,
				uint64_t id, uint64_t counter);
void m0_addb2_local_counter_mod(struct m0_addb2_local_counter *lc,
				uint64_t val, uint64_t datum);

/** @} end of addb2 group */
#endif /* __MOTR_ADDB2_COUNTER_H__ */

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
