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

#ifndef __MOTR_XCODE_FF2C_SEM_H__
#define __MOTR_XCODE_FF2C_SEM_H__

/**
   @addtogroup xcode
 */
/** @{ */

#include <stdbool.h>

/* import */
struct ff2c_term;

/* export */
struct ff2c_require;
struct ff2c_type;
struct ff2c_field;
struct ff2c_escape;
struct ff2c_list;

struct ff2c_list {
	void *l_head;
	void *l_tail;
};

struct ff2c_require {
	struct ff2c_require *r_next;
	const char          *r_path;
};

struct ff2c_type {
	struct ff2c_type       *t_next;
	const struct ff2c_term *t_term;
	const char             *t_name;
	const char             *t_xc_name;
	const char             *t_c_name;
	bool                    t_compound;
	bool                    t_atomic;
	bool                    t_opaque;
	bool                    t_sequence;
	bool                    t_array;
	bool                    t_union;
	bool                    t_record;
	bool                    t_public;
	int                     t_nr;
	struct ff2c_list        t_field;
};

struct ff2c_field {
	struct ff2c_field *f_next;
	struct ff2c_type  *f_parent;
	struct ff2c_type  *f_type;
	const char        *f_name;
	const char        *f_c_name;
	const char        *f_decl;
	const char        *f_xc_type;
	const char        *f_tag;
	const char        *f_escape;
};

struct ff2c_escape {
	struct ff2c_escape *e_next;
	const char         *e_escape;
};

struct ff2c_ff {
	struct ff2c_list ff_require;
	struct ff2c_list ff_type;
	struct ff2c_list ff_escape;
};

void ff2c_sem_init(struct ff2c_ff *ff, struct ff2c_term *top);
void ff2c_sem_fini(struct ff2c_ff *ff);

char *fmt(const char *format, ...) __attribute__((format(printf, 1, 2)));

/** @} end of xcode group */

/* __MOTR_XCODE_FF2C_SEM_H__ */
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
