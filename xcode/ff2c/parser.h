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

#ifndef __MOTR_XCODE_FF2C_PARSER_H__
#define __MOTR_XCODE_FF2C_PARSER_H__

/**
   @addtogroup xcode

   <b>ff2c. Parser.</b>

   Recursive-descent parser.
 */
/** @{ */

#include "xcode/ff2c/lex.h"

/* export */
struct ff2c_term;

enum ff2c_term_type {
	FNT_FF = 1,
	FNT_REQUIRE,
	FNT_DECLARATION,
	FNT_ATOMIC,
	FNT_COMPOUND,
	FNT_TYPENAME,
	FNT_TAG,
	FNT_ESCAPE,
	FNT_NR
};
extern const char *ff2c_term_type_name[];

struct ff2c_term {
	enum ff2c_term_type  fn_type;
	struct ff2c_term    *fn_parent;
	struct ff2c_term    *fn_head;
	struct ff2c_term    *fn_tail;
	struct ff2c_term    *fn_next;
	struct ff2c_token    fn_tok;
};

int ff2c_parse(struct ff2c_context *ctx, struct ff2c_term **out);
void ff2c_term_fini(struct ff2c_term *term);


/** @} end of xcode group */

/* __MOTR_XCODE_FF2C_PARSER_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
