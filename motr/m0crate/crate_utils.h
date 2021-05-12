/* -*- C -*- */
/*
 * Copyright (c) 2017-2021 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_M0CRATE_CRATE_UTILS_H__
#define __MOTR_M0CRATE_CRATE_UTILS_H__

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>


/**
 * @defgroup crate_utils
 *
 * @{
 */

typedef unsigned long long bcnt_t;
unsigned long long getnum(const char *str, const char *msg);
void init_rand_generator(unsigned long long seed);
int generate_fid(int seed, unsigned long *low, unsigned long *high);
unsigned char *calc_md5sum (char *buffer, int blocksize);
void timeval_norm(struct timeval *t);
void timeval_diff(const struct timeval *start, const struct timeval *end,
                         struct timeval *diff);
void timeval_add(struct timeval *sum, struct timeval *term);
void timeval_sub(struct timeval *end, struct timeval *start);
double tsec(const struct timeval *tval);
double rate(bcnt_t items, const struct timeval *tval, int scale);
unsigned long long genrand64_int64(void);
void cr_get_random_string(char *dest, size_t length);
void cr_time_acc(m0_time_t *t1, m0_time_t t2);


/** @} end of crate_utils group */
#endif /* __MOTR_M0CRATE_CRATE_UTILS_H__ */

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
