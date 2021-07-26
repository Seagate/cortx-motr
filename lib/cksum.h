/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_CKSUM_H__
#define __MOTR_CKSUM_H__

#include "lib/vec.h"
#include "fid/fid.h"
#include "xcode/xcode_attr.h"
#ifndef __KERNEL__
#include <openssl/md5.h>
#endif


#define m0_cksum_print(buf, seg, dbuf, msg) \
do { \
        struct m0_vec *vec = &(buf)->ov_vec; \
        char *dst = (char *)(buf)->ov_buf[seg]; \
        char *data = (char *)(dbuf)->ov_buf[seg]; \
        M0_LOG(M0_DEBUG, msg " count[%d] = %"PRIu64 \
                        " cksum = %c%c data = %c%c", \
                        seg, vec->v_count[seg], dst[0], dst[1], data[0],data[1]); \
}while(0)


/*
 * Macro calculates the size of padding required in a struct
 * for byte alignment
 * size - size of all structure members
 * alignment - power of two, byte alignment
 */
#define M0_CALC_PAD(size, alignment) ( size%alignment ? (((size/alignment + 1 ) * alignment) - size) : 0)


/* Constants for protection info type, max types supported is 8 */
enum
{
        M0_PI_TYPE_MD5,
        M0_PI_TYPE_MD5_INC_CONTEXT,
        M0_PI_TYPE_CRC,
        M0_PI_TYPE_MAX
};

enum m0_pi_calc_flag {

        /* NO PI FLAG */
        M0_PI_NO_FLAG = 0,
        /* PI calculation for data unit 0 */
        M0_PI_CALC_UNIT_ZERO = 1 << 0,
        /* Skip PI final value calculation */
        M0_PI_SKIP_CALC_FINAL = 1 << 1

};

M0_BASSERT(M0_PI_TYPE_MAX <= 8);

struct m0_pi_hdr {
        /* type of protection algorithm being used */
        uint8_t pi_type : 8;
        /*size of PI Structure in multiple of  32 bytes*/
        uint8_t pi_size : 8;
};

struct m0_md5_pi {

        /* header for protection info */
        struct m0_pi_hdr hdr;
#ifndef __KERNEL__
        /* protection value computed for the current data*/
        unsigned char pi_value[MD5_DIGEST_LENGTH];
        /* structure should be 32 byte aligned */
        char pad[M0_CALC_PAD((sizeof(struct m0_pi_hdr)+ MD5_DIGEST_LENGTH), 32)];
#endif
};

struct m0_md5_inc_context_pi {

        /* header for protection info */
        struct m0_pi_hdr hdr;
#ifndef __KERNEL__
        /*context of previous data unit, required for checksum computation */
        unsigned char prev_context[sizeof(MD5_CTX)];
        /* protection value computed for the current data unit.
         * If seed is not provided then this checksum is
         * calculated without seed.
         */
        unsigned char pi_value[MD5_DIGEST_LENGTH];
        /* structure should be 32 byte aligned */
        char pad[M0_CALC_PAD((sizeof(struct m0_pi_hdr)+ sizeof(MD5_CTX)+
                        MD5_DIGEST_LENGTH), 32)];
#endif
};

struct m0_generic_pi {
        /* header for protection info */
        struct m0_pi_hdr hdr;
        /*pointer to access specific pi structure fields*/
        void *t_pi;
};

/* seed values for calculating checksum */
struct m0_pi_seed {
        struct m0_fid obj_id;
        /* offset within motr object */
        m0_bindex_t data_unit_offset;
};

/**
 * Calculate checksum/protection info for data/KV
 *
 * @param pi  pi struct m0_md5_inc_context_pi
 *            This function will calculate the checksum and set
 *            pi_value field of struct m0_md5_inc_context_pi.
 * @param seed seed value (obj_id+data_unit_offset) required to calculate
 *             the checksum. If this pointer is NULL that means either
 *             this checksum calculation is meant for KV or user does
 *             not want seeding.
 * @param m0_bufvec - Set of buffers for which checksum is computed.
 * @param flag if flag is M0_PI_CALC_UNIT_ZERO, it means this api is called for
 *             first data unit and MD5_Init should be invoked.
 * @param[out] curr_context context of data unit N, will be required to calculate checksum for
 *                         next data unit, N+1. Curre_context is calculated and set in this func.
 * @param[out] pi_value_without_seed - Caller may need checksum value without seed and with seed.
 *                                     With seed checksum is set in pi_value of PI type struct.
 *                                     Without seed checksum is set in this field.
 */

M0_INTERNAL int m0_calculate_md5_inc_context(
                struct m0_md5_inc_context_pi *pi,
                struct m0_pi_seed *seed,
                struct m0_bufvec *bvec,
                enum m0_pi_calc_flag flag,
                unsigned char *curr_context,
                unsigned char *pi_value_without_seed);

/**
 * Calculate checksum/protection info for data/KV
 *
 * @param[IN/OUT] pi  Caller will pass Generic pi struct, which will be typecasted to
 *                    specific PI type struct. API will calculate the checksum and set
 *                    pi_value of PI type struct. In case of context, caller will send
 *                    data unit N-1 unit's context via prev_context field in PI type struct.
 *                    This api will calculate unit N's context and set value in curr_context.
 *                    IN values - pi_type, pi_size, prev_context
 *                    OUT values - pi_value, prev_context for first data unit.
 * @param[IN] seed seed value (obj_id+data_unit_offset) required to calculate
 *                 the checksum. If this pointer is NULL that means either
 *                 this checksum calculation is meant for KV or user does
 *                 not want seeding.
 *                 NOTE: seed is always NULL, non-null value sent at the last chunk of motr unit
 * @param[IN] m0_bufvec Set of buffers for which checksum is computed. Normally
 *                      this set of vectors will make one data unit. It can be NULL as well.
 * @param[IN] flag If flag is M0_PI_CALC_UNIT_ZERO, it means this api is called for
 *                 first data unit and init functionality should be invoked such as MD5_Init.
 * @param[OUT] curr_context context of data unit N, will be required to calculate checksum for
 *                         next data unit, N+1. This api will calculate and set value for this field.
 *                         NOTE: curr_context always have unseeded and non finalised context value
 *                         and sending this parameter is mandatory.
 * @param[OUT] pi_value_without_seed Caller may need checksum value without seed and with seed.
 *                                   With seed checksum is set in pi_value of PI type struct.
 *                                   Without seed checksum is set in this field.
 *                                   Caller has to allocate memory for this filed.
 */

int m0_client_calculate_pi(struct m0_generic_pi *pi,
                struct m0_pi_seed *seed,
                struct m0_bufvec *bvec,
                enum m0_pi_calc_flag flag,
                unsigned char *curr_context,
                unsigned char *pi_value_without_seed);


/**
 * Calculates checksum for data sent and compare it with checksum value sent.
 * If newly calculated checksum on data and checksum sent matches, return
 * true else return false.
 * @param[IN] pi This ia an already calculated checksum value structure.
 * @param[IN] seed This seed is required to calculate checksum
 * @param[IN] bvec buffer vector which contains pointers to data on which
 *                 checksum is to be calculated.
 */
bool m0_calc_verify_cksum_one_unit(struct m0_generic_pi *pi,
                                   struct m0_pi_seed *seed,
                                   struct m0_bufvec *bvec);

#endif /* __MOTR_CKSUM_H__ */
