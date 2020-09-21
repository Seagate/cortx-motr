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

#ifndef __MOTR_LIB_USERSP_TRACE_H__
#define __MOTR_LIB_USERSP_TRACE_H__

#include <stdio.h>  /* FILE */

#include "lib/types.h"  /* pid_t */

/**
   @defgroup trace Tracing.

   User-space specific declarations.

 */

extern pid_t m0_pid_cached;

enum m0_trace_parse_flags {
	M0_TRACE_PARSE_HEADER_ONLY             = 1 << 0,
	M0_TRACE_PARSE_YAML_SINGLE_DOC_OUTPUT  = 1 << 1,

	M0_TRACE_PARSE_DEFAULT_FLAGS           = 0 /* all flags off */
};

M0_INTERNAL int m0_trace_parse(FILE *trace_file, FILE *output_file,
			       const char *m0tr_ko_path,
			       enum m0_trace_parse_flags flags,
			       const void *magic_symbols[],
			       unsigned int magic_symbols_nr);

M0_INTERNAL void m0_trace_set_mmapped_buffer(bool val);

int m0_trace_set_buffer_size(size_t size);

/** @} end of trace group */
#endif /* __MOTR_LIB_USERSP_TRACE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
