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

#ifndef __MOTR_ADDB2_INTERNAL_H__
#define __MOTR_ADDB2_INTERNAL_H__

/**
 * @defgroup addb2
 *
 * @{
 */

#define HEADER_XO(h) &(struct m0_xcode_obj) {	\
	.xo_type = m0_addb2_frame_header_xc,	\
	.xo_ptr  = (h)				\
}

#define TRACE_XO(t) &(struct m0_xcode_obj) {	\
	.xo_type = m0_addb2_trace_xc,		\
	.xo_ptr  = (t)				\
}

enum {
	/**
	 * Maximal number of 64-bit values in a payload.
	 *
	 * @note this constant cannot really be increased. Trace buffer format
	 * assumes that this is less than 0x10.
	 */
	VALUE_MAX_NR    = 15,
	FRAME_TRACE_MAX = 128,
	FRAME_SIZE_MAX  = 4 * 1024 * 1024,
};

M0_INTERNAL m0_bcount_t m0_addb2_trace_size(const struct m0_addb2_trace *trace);

M0_EXTERN uint64_t m0_addb2__dummy_payload[];
M0_EXTERN uint64_t m0_addb2__dummy_payload_size;

M0_TL_DESCR_DECLARE(tr, M0_EXTERN);
M0_TL_DECLARE(tr, M0_INTERNAL, struct m0_addb2_trace_obj);

M0_TL_DESCR_DECLARE(mach, M0_EXTERN);
M0_TL_DECLARE(mach, M0_INTERNAL, struct m0_addb2_mach);

M0_INTERNAL void m0_addb2__mach_print(const struct m0_addb2_mach *m);

enum storage_constants {
	/**
	 * Use logical block size 2^16 independently of stob block size. This
	 * simplifies IO and makes format more portable.
	 */
	BSHIFT  = 16,
	BSIZE   = M0_BITS(BSHIFT)
};

struct m0_addb2_counter_data;
struct m0_addb2_sensor;

M0_INTERNAL void m0_addb2__counter_snapshot(struct m0_addb2_sensor *s,
					    uint64_t *area);
M0_INTERNAL void m0_addb2__counter_data_init(struct m0_addb2_counter_data *d);

enum {
	/**
	 * Maximal number of global philters. Arbitrary.
	 */
	M0_ADDB2_GLOBAL_PHILTERS = 512
};

/**
 * Global addb2 state (per m0 instance).
 */
struct m0_addb2_module {
	/**
	 * Sys instance used by global.c.
	 */
	struct m0_addb2_sys     *am_sys;
	/**
	 * Array of global philters.
	 */
	struct m0_addb2_philter *am_philter[M0_ADDB2_GLOBAL_PHILTERS];
};

M0_INTERNAL struct m0_addb2_module *m0_addb2_module_get(void);

/** @} end of addb2 group */
#endif /* __MOTR_ADDB2_INTERNAL_H__ */

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
