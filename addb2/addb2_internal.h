/* -*- C -*- */
/*
 * Copyright (c) 2019-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_ADDB2_ADDB2_INTERNAL_H__
#define __MOTR_ADDB2_ADDB2_INTERNAL_H__

/**
 * @defgroup addb2
 *
 * @{
 */

#include "lib/types.h"

struct m0_addb2__context;

/**
 * Custom addb2 records identifier ranges.
 * These ranges must be used in external projects (S3, NFS)
 * and addb2dump utility plugins
 */
enum {
        M0_ADDB2__EXT_RANGE_1 = 0x0010000,
        M0_ADDB2__EXT_RANGE_2 = 0x0020000,
        M0_ADDB2__EXT_RANGE_3 = 0x0030000,
        M0_ADDB2__EXT_RANGE_4 = 0x0040000
};

enum {
        M0_ADDB2__FIELD_MAX = 15
};

/**
 * Structure of the interpreter of addb2 records
 */
struct m0_addb2__id_intrp {
        uint64_t     ii_id;
        const char  *ii_name;
        void       (*ii_print[M0_ADDB2__FIELD_MAX])(
                        struct m0_addb2__context *ctx,
                        const uint64_t *v, char *buf);
        const char  *ii_field[M0_ADDB2__FIELD_MAX];
        void       (*ii_spec)(struct m0_addb2__context *ctx, char *buf);
        int          ii_repeat;
};

/**
 * addb2dump plugin function name
*/
#define M0_ADDB2__PLUGIN_FUNC_NAME "m0_addb2_load_interps"

typedef int (*m0_addb2__intrp_load_t)(uint64_t flags,
                                      struct m0_addb2__id_intrp **intrp);

/** @} end of addb2 group */
#endif /* __MOTR_ADDB2_ADDB2_INTERNAL_H__ */

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
