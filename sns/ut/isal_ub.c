/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <isa-l.h>

#include "lib/ub.h"
#include "lib/misc.h"

#define UB_ITER 1
#define MMAX    255
#define KMAX    255


static int ub_init(const char *opts M0_UNUSED)
{
    srand(1285360231);
    return 0;
}

/*
 * Generate decode matrix from encode matrix and erasure list
 *
 */

static int gf_gen_decode_matrix_simple(uint8_t * encode_matrix,
                    uint8_t * decode_matrix,
                    uint8_t * invert_matrix,
                    uint8_t * temp_matrix,
                    uint8_t * decode_index, uint8_t * frag_err_list, int nerrs, int k,
                    int m)
{
    int i, j, p, r;
    int nsrcerrs = 0;
    uint8_t s, *b = temp_matrix;
    uint8_t frag_in_err[MMAX];

    memset(frag_in_err, 0, sizeof(frag_in_err));

    // Order the fragments in erasure for easier sorting
    for (i = 0; i < nerrs; i++) {
        if (frag_err_list[i] < k)
            nsrcerrs++;
        frag_in_err[frag_err_list[i]] = 1;
    }

    // Construct b (matrix that encoded remaining frags) by removing erased rows
    for (i = 0, r = 0; i < k; i++, r++) {
        while (frag_in_err[r])
            r++;
        for (j = 0; j < k; j++)
            b[k * i + j] = encode_matrix[k * r + j];
        decode_index[i] = r;
    }

    // Invert matrix to get recovery matrix
    if (gf_invert_matrix(b, invert_matrix, k) < 0)
        return -1;

    // Get decode matrix with only wanted recovery rows
    for (i = 0; i < nerrs; i++) {
        if (frag_err_list[i] < k)    // A src err
            for (j = 0; j < k; j++)
                decode_matrix[k * i + j] =
                    invert_matrix[k * frag_err_list[i] + j];
    }

    // For non-src (parity) erasures need to multiply encode matrix * invert
    for (p = 0; p < nerrs; p++) {
        if (frag_err_list[p] >= k) {    // A parity err
            for (i = 0; i < k; i++) {
                s = 0;
                for (j = 0; j < k; j++)
                    s ^= gf_mul(invert_matrix[j * k + i],
                            encode_matrix[k * frag_err_list[p] + j]);
                decode_matrix[k * p + i] = s;
            }
        }
    }
    return 0;
}

static int ub_test(uint32_t k, uint32_t p, uint32_t len)
{
    uint32_t i, j, m;
    uint32_t nerrs = 0;

    // Fragment buffer pointers
    uint8_t *frag_ptrs[MMAX];
    uint8_t *recover_srcs[KMAX];
    uint8_t *recover_outp[KMAX];
    uint8_t frag_err_list[MMAX];

    // Coefficient matrices
    uint8_t *encode_matrix, *decode_matrix;
    uint8_t *invert_matrix, *temp_matrix;
    uint8_t *g_tbls;
    uint8_t decode_index[MMAX];

    m = k + p;

    // Allocate coding matrices
    encode_matrix = malloc(m * k);
    decode_matrix = malloc(m * k);
    invert_matrix = malloc(m * k);
    temp_matrix = malloc(m * k);
    g_tbls = malloc(k * p * 32);

    if (encode_matrix == NULL || decode_matrix == NULL
        || invert_matrix == NULL || temp_matrix == NULL || g_tbls == NULL) {
        printf("Test failure! Error with malloc\n");
        return -1;
    }

    // Allocate the src & parity buffers
    for (i = 0; i < m; i++) {
        if (NULL == (frag_ptrs[i] = malloc(len))) {
            printf("alloc error: Fail\n");
            return -1;
        }
    }

    // Allocate buffers for recovered data
    for (i = 0; i < p; i++) {
        if (NULL == (recover_outp[i] = malloc(len))) {
            printf("alloc error: Fail\n");
            return -1;
        }
    }

    // Fill sources with random data
    for (i = 0; i < k; i++)
        for (j = 0; j < len; j++)
            frag_ptrs[i][j] = rand();

    gf_gen_cauchy1_matrix(encode_matrix, m, k);

    // Initialize g_tbls from encode matrix
    ec_init_tables(k, p, &encode_matrix[k * k], g_tbls);

    // Generate EC parity blocks from sources
    ec_encode_data(len, k, p, g_tbls, frag_ptrs, &frag_ptrs[k]);

    for (i = 0; i < p; i++)
    {
        uint32_t err_id = rand() % m;
        frag_err_list[nerrs++] = err_id;
        memset(frag_ptrs[err_id], 0xFF, len);
    }

    // Find a decode matrix to regenerate all erasures from remaining frags
    gf_gen_decode_matrix_simple(encode_matrix, decode_matrix,
                        invert_matrix, temp_matrix, decode_index,
                        frag_err_list, nerrs, k, m);

    // Pack recovery array pointers as list of valid fragments
    for (i = 0; i < k; i++)
        recover_srcs[i] = frag_ptrs[decode_index[i]];

    // Recover data
    ec_init_tables(k, nerrs, decode_matrix, g_tbls);
    ec_encode_data(len, k, nerrs, g_tbls, recover_srcs, recover_outp);

    return 0;
}

static void ub_small_4096(int iter)
{
    ub_test(10, 5, 4096);
}

static void ub_medium_4096(int iter)
{
    ub_test(20, 6, 4096);
}

static void ub_large_4096(int iter)
{
    ub_test(30, 12, 4096);
}

static void ub_small_32768(int iter)
{
    ub_test(10, 5, 32768);
}

static void ub_medium_32768(int iter)
{
    ub_test(20, 6, 32768);
}

static void ub_large_32768(int iter)
{
    ub_test(30, 12, 32768);
}

static void ub_small_1048576(int iter)
{
    ub_test(3, 2, 1048576);
}

static void ub_medium_1048576(int iter)
{
    ub_test(6, 3, 1048576);
}

static void ub_large_1048576(int iter)
{
    ub_test(8, 4, 1048576);
}

struct m0_ub_set ec_isa_ub = {
    .us_name = "ec-isa-ub",
    .us_init = ub_init,
    .us_fini = NULL,
    .us_run  = {
            { .ub_name  = "s 10/05/ 4K",
                .ub_iter  = UB_ITER,
                .ub_round = ub_small_4096 },

            { .ub_name  = "m 20/06/ 4K",
              .ub_iter  = UB_ITER,
              .ub_round = ub_medium_4096 },

            { .ub_name  = "l 30/12/ 4K",
              .ub_iter  = UB_ITER,
              .ub_round = ub_large_4096 },

            { .ub_name  = "s 10/05/32K",
              .ub_iter  = UB_ITER,
              .ub_round = ub_small_32768 },

            { .ub_name  = "m 20/06/32K",
              .ub_iter  = UB_ITER,
              .ub_round = ub_medium_32768 },

            { .ub_name  = "l 30/12/32K",
              .ub_iter  = UB_ITER,
              .ub_round = ub_large_32768 },

            { .ub_name  = "s  03/02/ 1M",
              .ub_iter  = UB_ITER,
              .ub_round = ub_small_1048576 },

            { .ub_name  = "m 06/03/ 1M",
              .ub_iter  = UB_ITER,
              .ub_round = ub_medium_1048576 },

            { .ub_name  = "l 08/04/ 1M",
              .ub_iter  = UB_ITER,
              .ub_round = ub_large_1048576 },

           { .ub_name = NULL}
    }
};
