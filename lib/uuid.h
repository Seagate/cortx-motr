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

#ifndef __MOTR_LIB_UUID_H__
#define __MOTR_LIB_UUID_H__

#include "lib/types.h" /* struct m0_uint128 */

/**
   @defgroup uuid UUID support
   @{
 */

enum {
	M0_UUID_STRLEN = 36
};

/**
   Parse the 8-4-4-4-12 hexadecimal string representation of a UUID
   and convert to numerical form.
   See <a href="http://en.wikipedia.org/wiki/Universally_unique_identifier">
   Universally unique identifier</a> for more details.
 */
M0_INTERNAL int m0_uuid_parse(const char *str, struct m0_uint128 *val);

/**
   Produce the 8-4-4-4-12 hexadecimal string representation of a UUID
   from its numerical form.
   See <a href="http://en.wikipedia.org/wiki/Universally_unique_identifier">
   Universally unique identifier</a> for more details.
   @param val The numerical UUID.
   @param buf String buffer.
   @param len Length of the buffer.
              It must be at least M0_UUID_STRLEN+1 bytes long.
 */
M0_INTERNAL void m0_uuid_format(const struct m0_uint128 *val,
				char *buf, size_t len);

M0_INTERNAL void m0_uuid_generate(struct m0_uint128 *u);

/**
 * The UUID of a Motr node.
 */
M0_EXTERN struct m0_uint128 m0_node_uuid;

void m0_kmod_uuid_file_set(const char *path);
void m0_node_uuid_string_set(const char *uuid);
int  m0_node_uuid_string_get(char buf[M0_UUID_STRLEN + 1]);

/** @} end uuid group */

#endif /* __MOTR_LIB_UUID_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
