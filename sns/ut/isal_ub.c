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
#include <sys/time.h>

#include "lib/ub.h"
#include "lib/misc.h"
#include "lib/memory.h"
#include "lib/buf.h"

#define UB_ITER 1
#define MMAX    255
#define KMAX    255


static int ub_init(const char *opts M0_UNUSED)
{
    time_t now;

    time(&now);
    srand(now);

    return 0;
}

static void unit_spoil( struct m0_buf *failed,
                        const uint32_t data_count )
{
    uint32_t err_id, i, j;
    bool found;
    uint8_t *err_list = (uint8_t *)failed->b_addr;

    i = 0;
    do {
        err_id = rand() % data_count;
        found = false;

        for (j = 0; j < i; j++) {
            if (err_list[j] == err_id) {
                found = true;
                break;
            }
        }
        if (!found) {
            err_list[i++] = err_id;
        }
    } while ( i < failed->b_nob );
}

/*
 * Generate decode matrix from encode matrix and erasure list
 *
 */

static int gen_decode_matrix(uint8_t *encode_matrix,
                             uint8_t *decode_matrix,
                             uint8_t *decode_index,
                             struct m0_buf failed,
                             int k, int m)
{
    int i, j, p, r;
    uint8_t s;
    uint8_t *temp_matrix = NULL;
    uint8_t *invert_matrix = NULL;
    uint8_t *err_list = (uint8_t *)failed.b_addr;
    uint8_t frag_in_err[MMAX];
    int8_t ret = 0;

    // Allocate memory for matrices
    temp_matrix = m0_alloc(m * k);
    invert_matrix = m0_alloc(m * k);
    if ((temp_matrix == NULL) || (invert_matrix == NULL)){
        printf("Error with m0_alloc\n");
        ret = -1;
        goto exit;
    }

    memset(frag_in_err, 0, sizeof(frag_in_err));

    // Order the fragments in erasure for easier sorting
    for (i = 0; i < failed.b_nob; i++) {
        frag_in_err[err_list[i]] = 1;
    }

    // Construct temp_matrix (matrix that encoded remaining frags)
    // by removing erased rows
    for (i = 0, r = 0; i < k; i++, r++) {
        while (frag_in_err[r])
            r++;
        for (j = 0; j < k; j++)
            temp_matrix[k * i + j] = encode_matrix[k * r + j];
        decode_index[i] = r;
    }

    // Invert matrix to get recovery matrix
    ret = gf_invert_matrix(temp_matrix, invert_matrix, k);
    if ( ret != 0)
        goto exit;

    for (p = 0; p < failed.b_nob; p++)
    {
        // Get decode matrix with only wanted recovery rows
        if (err_list[p] < k)    // A src err
        {
            for (i = 0; i < k; i++)
                decode_matrix[k * p + i] =
                    invert_matrix[k * err_list[p] + i];
        }
        // For non-src (parity) erasures need to multiply
        // encode matrix * invert
        else // A parity err
        {
            for (i = 0; i < k; i++)
            {
                s = 0;
                for (j = 0; j < k; j++)
                    s ^= gf_mul(invert_matrix[j * k + i],
                            encode_matrix[k * err_list[p] + j]);
                decode_matrix[k * p + i] = s;
            }
        }
    }

exit:
    m0_free(temp_matrix);
    m0_free(invert_matrix);

    return ret;
}

static int ub_test(uint32_t k, uint32_t p, uint32_t len)
{
    int result = 0;
    uint32_t i, j, m;

    // Fragment buffer pointers
    uint8_t *frag_ptrs[MMAX] = {NULL};
    uint8_t *recover_srcs[KMAX] = {NULL};
    uint8_t *recover_outp[KMAX] = {NULL};
    uint8_t *frag_err_list = NULL;

    // Coefficient matrices
    uint8_t *encode_matrix, *decode_matrix;
    uint8_t *g_tbls;
    uint8_t decode_index[MMAX];

    struct m0_buf fail_buf = {0};

    m = k + p;

    // Allocate coding matrices
    encode_matrix = m0_alloc(m * k);
    decode_matrix = m0_alloc(m * k);
    g_tbls = m0_alloc(k * p * 32);

    if (encode_matrix == NULL || decode_matrix == NULL || g_tbls == NULL) {
        printf("Test failure! Error with m0_alloc\n");
        result = -1;
        goto exit;
    }

    // Allocate the src & parity buffers
    for (i = 0; i < m; i++) {
        if (NULL == (frag_ptrs[i] = m0_alloc(len))) {
            printf("alloc error: Fail\n");
            result = -1;
            goto exit;
        }
    }

    // Allocate buffers for recovered data
    for (i = 0; i < p; i++) {
        if (NULL == (recover_outp[i] = m0_alloc(len))) {
            printf("alloc error: Fail\n");
            result = -1;
            goto exit;
        }
    }

    // Allocate buffer for list of failed fragments
    frag_err_list = m0_alloc(p);
    if (frag_err_list == NULL) {
        printf("alloc error: Fail\n");
        result = -1;
        goto exit;
    }

    m0_buf_init(&fail_buf, frag_err_list, p);

    // Fill sources with random data
    for (i = 0; i < k; i++)
        for (j = 0; j < len; j++)
            frag_ptrs[i][j] = rand();

    gf_gen_cauchy1_matrix(encode_matrix, m, k);

    // Initialize g_tbls from encode matrix
    ec_init_tables(k, p, &encode_matrix[k * k], g_tbls);

    // Generate EC parity blocks from sources
    ec_encode_data(len, k, p, g_tbls, frag_ptrs, &frag_ptrs[k]);

    // Get list of failed fragments
    unit_spoil(&fail_buf, m);

    // Find a decode matrix to regenerate all erasures from remaining frags
    result = gen_decode_matrix(encode_matrix, decode_matrix,
                               decode_index, fail_buf, k, m);
    if ( result != 0 ) {
            printf("Failed to generate decode matrix\n");
            goto exit;
    }

    // Pack recovery array pointers as list of valid fragments
    for (i = 0; i < k; i++)
        recover_srcs[i] = frag_ptrs[decode_index[i]];

    // Recover data
    ec_init_tables(k, fail_buf.b_nob, decode_matrix, g_tbls);
    ec_encode_data(len, k, fail_buf.b_nob, g_tbls, recover_srcs, recover_outp);

    // Check that recovered buffers are the same as original
    for (i = 0; i < fail_buf.b_nob; i++) {
        if (memcmp(recover_outp[i], frag_ptrs[frag_err_list[i]], len)) {
            printf(" Fail erasure recovery %d, frag %d\n", i, frag_err_list[i]);
            result = -1;
            goto exit;
        }
    }

exit:
    m0_free(encode_matrix);
    m0_free(decode_matrix);
    m0_free(g_tbls);
    m0_free(frag_err_list);
    for (i = 0; i < m; i++) {
        m0_free(frag_ptrs[i]);
    }
    for (i = 0; i < p; i++) {
        m0_free(recover_outp[i]);
    }

    return result;
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

            { .ub_name  = "s 03/02/ 1M",
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
