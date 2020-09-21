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


/**
 * This is simple test plugin for m0addb2dump utility.
 * It's using in system test addb2/st/addb2dump_plugin.sh
 *
 */

#include "addb2/plugin_api.h"
#include <stdio.h>

static void param1(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
        sprintf(buf, "param1: 0x%lu", (long unsigned int)v[0]);
}

static void param2(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
        sprintf(buf, "param2: 0x%lu", (long unsigned int)v[0]);
}


struct m0_addb2__id_intrp ext_intrp[] = {
        { M0_ADDB2__EXT_RANGE_1, "measurement_1",   { &param1, &param2 } },
        { M0_ADDB2__EXT_RANGE_2, "measurement_2",   { &param1, &param2 } },
        { M0_ADDB2__EXT_RANGE_3, "measurement_3",   { &param1, &param2 } },
        { M0_ADDB2__EXT_RANGE_4, "measurement_4",   { &param1, &param2 } },
        { 0 }
};

int m0_addb2_load_interps(uint64_t flags, struct m0_addb2__id_intrp **intrp)
{
        *intrp = ext_intrp;
        return 0;
}

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
