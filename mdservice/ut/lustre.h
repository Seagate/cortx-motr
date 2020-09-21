/* -*- C -*- */
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


#pragma once

#ifndef __MOTR_MDSERVICE_UT_LUSTRE_H__
#define __MOTR_MDSERVICE_UT_LUSTRE_H__

/**
   These two structures used for testing mdstore functionality. To do
   so we use changelog dump created by dump.changelog program, parse
   it, convert to fops and feed to test program in fops form.
*/
struct m0_md_lustre_fid {
        uint64_t f_seq;
        uint32_t f_oid;
        uint32_t f_ver;
};

struct m0_md_lustre_logrec {
        uint16_t                 cr_namelen;
        uint16_t                 cr_flags;
        uint16_t                 cr_valid;
        uint32_t                 cr_mode;
        uint8_t                  cr_type;
        uint64_t                 cr_index;
        uint64_t                 cr_time;
        uint64_t                 cr_atime;
        uint64_t                 cr_ctime;
        uint64_t                 cr_mtime;
        uint32_t                 cr_nlink;
        uint32_t                 cr_rdev;
        uint64_t                 cr_version;
        uint64_t                 cr_size;
        uint64_t                 cr_blocks;
        uint64_t                 cr_blksize;
        uint32_t                 cr_uid;
        uint32_t                 cr_gid;
        uint32_t                 cr_sid;
        uint64_t                 cr_clnid;
        struct m0_md_lustre_fid  cr_tfid;
        struct m0_md_lustre_fid  cr_pfid;
        char                     cr_name[0];
} __attribute__((packed));

enum m0_md_lustre_logrec_type {
        RT_MARK     = 0,
        RT_CREATE   = 1,
        RT_MKDIR    = 2,
        RT_HARDLINK = 3,
        RT_SOFTLINK = 4,
        RT_MKNOD    = 5,
        RT_UNLINK   = 6,
        RT_RMDIR    = 7,
        RT_RENAME   = 8,
        RT_EXT      = 9,
        RT_OPEN     = 10,
        RT_CLOSE    = 11,
        RT_IOCTL    = 12,
        RT_TRUNC    = 13,
        RT_SETATTR  = 14,
        RT_XATTR    = 15,
        RT_HSM      = 16,
        RT_MTIME    = 17,
        RT_CTIME    = 18,
        RT_ATIME    = 19,
        RT_LAST
};

#endif
