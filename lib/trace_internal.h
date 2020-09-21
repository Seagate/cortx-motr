/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_LIB_TRACE_INTERNAL_H__
#define __MOTR_LIB_TRACE_INTERNAL_H__

#include "lib/trace.h"  /* m0_trace_buf_header */


struct m0_trace_area {
	struct m0_trace_buf_header ta_header;
	char                       ta_buf[0];
};

M0_INTERNAL int m0_arch_trace_init(void);

M0_INTERNAL void m0_arch_trace_fini(void);

M0_INTERNAL int
m0_trace_subsys_list_to_mask(char *subsys_names, unsigned long *ret_mask);

M0_INTERNAL enum m0_trace_level m0_trace_level_parse(char *str);

M0_INTERNAL enum m0_trace_print_context
m0_trace_print_context_parse(const char *ctx_name);

M0_INTERNAL const char *m0_trace_level_name(enum m0_trace_level level);

M0_INTERNAL const char *m0_trace_subsys_name(uint64_t subsys);

union m0_trace_rec_argument {
	uint8_t  v8;
	uint16_t v16;
	uint32_t v32;
	uint64_t v64;
};

typedef union m0_trace_rec_argument m0_trace_rec_args_t[M0_TRACE_ARGC_MAX];

M0_INTERNAL int m0_trace_args_unpack(const struct m0_trace_rec_header *trh,
				     m0_trace_rec_args_t args,
				     const void *buf);

M0_INTERNAL const struct m0_trace_buf_header *m0_trace_logbuf_header_get(void);
M0_INTERNAL const void *m0_trace_logbuf_get(void);
M0_INTERNAL uint32_t m0_trace_logbuf_size_get(void);
M0_INTERNAL void m0_trace_logbuf_size_set(size_t size);
M0_INTERNAL uint64_t m0_trace_logbuf_pos_get(void);
M0_INTERNAL const void *m0_trace_magic_sym_addr_get(void);
M0_INTERNAL const char *m0_trace_magic_sym_name_get(void);
int m0_trace_magic_sym_extra_addr_add(const void *addr);

M0_INTERNAL const struct m0_trace_rec_header *m0_trace_last_record_get(void);

M0_INTERNAL const char *m0_trace_file_path_get(void);

M0_INTERNAL void m0_trace_stats_update(uint32_t rec_size);

M0_INTERNAL void m0_trace_buf_header_init(struct m0_trace_buf_header *tbh, size_t buf_size);
M0_INTERNAL void m0_arch_trace_buf_header_init(struct m0_trace_buf_header *tbh);

M0_INTERNAL void m0_trace_switch_to_static_logbuf(void);

M0_INTERNAL void m0_console_vprintf(const char *fmt, va_list ap);

#endif /* __MOTR_LIB_TRACE_INTERNAL_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
