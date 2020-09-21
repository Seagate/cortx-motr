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


#pragma once

#ifndef __MOTR_ST_ASSERT_H__
#define __MOTR_ST_ASSERT_H__

#include "motr/client.h"

/* XXX juan: file defining assertimpl should be #included */
#define ST_ASSERT_FATAL(a) \
	do { \
		if (!st_assertimpl((a),#a,__FILE__,__LINE__,__func__))\
			return; \
	} while(0);


/* XXX juan: we need doxygen doc for all these functions. */
/* Functions for cleaner*/
int st_cleaner_init(void);
void st_cleaner_fini(void);
bool st_is_cleaner_up(void);
void st_cleaner_enable(void);
void st_cleaner_disable(void);
void st_cleaner_empty_bin(void);

void st_mark_op(struct m0_op *op);
void st_unmark_op(struct m0_op *op);
void st_mark_entity(struct m0_entity *entity);
void st_unmark_entity(struct m0_entity *entity);
void st_mark_ptr(void *ptr);
void st_unmark_ptr(void *ptr);

#endif /* __MOTR_ST_ASSERT_H__ */
