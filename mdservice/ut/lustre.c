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


#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/misc.h"              /* M0_SET0 */
#include "lib/bitstring.h"

#include "fop/fop.h"
#include "cob/cob.h"
#include "mdstore/mdstore.h"
#include "mdservice/md_fops.h"
#include "mdservice/md_fops_xc.h"
#include "mdservice/ut/lustre.h"

typedef int (*fop_translate_t)(struct m0_fop *fop, void *data);

static void lustre_copy_fid(struct m0_fid *bf,
                            const struct m0_md_lustre_fid *cf)
{
        m0_fid_set(bf, cf->f_seq, cf->f_oid);
}

static int lustre_copy_name(struct m0_fop_str *n,
                            const struct m0_md_lustre_logrec *rec)
{
        n->s_buf = m0_alloc(rec->cr_namelen);
        if (n->s_buf == NULL)
                return -ENOMEM;
        n->s_len = rec->cr_namelen;
        memcpy(n->s_buf, rec->cr_name, rec->cr_namelen);
        return 0;
}

enum lustre_la_valid {
        M0_LA_ATIME   = 1 << 0,
        M0_LA_MTIME   = 1 << 1,
        M0_LA_CTIME   = 1 << 2,
        M0_LA_SIZE    = 1 << 3,
        M0_LA_MODE    = 1 << 4,
        M0_LA_UID     = 1 << 5,
        M0_LA_GID     = 1 << 6,
        M0_LA_BLOCKS  = 1 << 7,
        M0_LA_TYPE    = 1 << 8,
        M0_LA_FLAGS   = 1 << 9,
        M0_LA_NLINK   = 1 << 10,
        M0_LA_RDEV    = 1 << 11,
        M0_LA_BLKSIZE = 1 << 12
};

static uint16_t lustre_get_valid(uint16_t valid)
{
        uint16_t result = 0;

        if (valid & M0_LA_ATIME)
                result |= M0_COB_ATIME;
        if (valid & M0_LA_MTIME)
                result |= M0_COB_MTIME;
        if (valid & M0_LA_CTIME)
                result |= M0_COB_CTIME;
        if (valid & M0_LA_SIZE)
                result |= M0_COB_SIZE;
        if (valid & M0_LA_MODE)
                result |= M0_COB_MODE;
        if (valid & M0_LA_UID)
                result |= M0_COB_UID;
        if (valid & M0_LA_GID)
                result |= M0_COB_GID;
        if (valid & M0_LA_BLOCKS)
                result |= M0_COB_BLOCKS;
        if (valid & M0_LA_TYPE)
                result |= M0_COB_TYPE;
        if (valid & M0_LA_FLAGS)
                result |= M0_COB_FLAGS;
        if (valid & M0_LA_NLINK)
                result |= M0_COB_NLINK;
        if (valid & M0_LA_RDEV)
                result |= M0_COB_RDEV;
        if (valid & M0_LA_BLKSIZE)
                result |= M0_COB_BLKSIZE;
        return result;
}

static void lustre_copy_body(struct m0_fop_cob *body,
                             const struct m0_md_lustre_logrec *rec)
{
        struct m0_md_lustre_fid fid;

        body->b_index = rec->cr_index;
        if (rec->cr_valid & M0_LA_SIZE)
                body->b_size = rec->cr_size;
        if (rec->cr_valid & M0_LA_BLKSIZE)
                body->b_blksize = rec->cr_blksize;
        if (rec->cr_valid & M0_LA_BLOCKS)
                body->b_blocks = rec->cr_blocks;
        if (rec->cr_valid & M0_LA_UID)
                body->b_uid = rec->cr_uid;
        if (rec->cr_valid & M0_LA_GID)
                body->b_gid = rec->cr_gid;
        if (rec->cr_valid & M0_LA_ATIME)
                body->b_atime = rec->cr_atime;
        if (rec->cr_valid & M0_LA_MTIME)
                body->b_mtime = rec->cr_mtime;
        if (rec->cr_valid & M0_LA_CTIME)
                body->b_ctime = rec->cr_ctime;
        if (rec->cr_valid & M0_LA_NLINK)
                body->b_nlink = rec->cr_nlink;
        if (rec->cr_valid & M0_LA_MODE)
                body->b_mode = rec->cr_mode;
        body->b_sid = rec->cr_sid;
        body->b_nid = rec->cr_clnid;
        body->b_version = rec->cr_version;
        body->b_flags = rec->cr_flags;
        body->b_valid = lustre_get_valid(rec->cr_valid);
        fid = rec->cr_tfid;
        lustre_copy_fid(&body->b_tfid, &fid);
        fid = rec->cr_pfid;
        lustre_copy_fid(&body->b_pfid, &fid);
}

static int lustre_create_fop(struct m0_fop *fop, void *data)
{
        struct m0_fop_create *d = m0_fop_data(fop);
        struct m0_md_lustre_logrec *rec = data;

        lustre_copy_body(&d->c_body, rec);
        return lustre_copy_name(&d->c_name, rec);
}

static int lustre_link_fop(struct m0_fop *fop, void *data)
{
        struct m0_fop_link *d = m0_fop_data(fop);
        struct m0_md_lustre_logrec *rec = data;

        lustre_copy_body(&d->l_body, rec);
        return lustre_copy_name(&d->l_name, rec);
}

static int lustre_unlink_fop(struct m0_fop *fop, void *data)
{
        struct m0_fop_unlink *d = m0_fop_data(fop);
        struct m0_md_lustre_logrec *rec = data;

        lustre_copy_body(&d->u_body, rec);
        return lustre_copy_name(&d->u_name, rec);
}

static int lustre_open_fop(struct m0_fop *fop, void *data)
{
        struct m0_fop_open *d = m0_fop_data(fop);
        struct m0_md_lustre_logrec *rec = data;
        lustre_copy_body(&d->o_body, rec);
        return 0;
}

static int lustre_close_fop(struct m0_fop *fop, void *data)
{
        struct m0_fop_close *d = m0_fop_data(fop);
        struct m0_md_lustre_logrec *rec = data;
        lustre_copy_body(&d->c_body, rec);
        return 0;
}

static int lustre_setattr_fop(struct m0_fop *fop, void *data)
{
        struct m0_fop_setattr *d = m0_fop_data(fop);
        struct m0_md_lustre_logrec *rec = data;
        lustre_copy_body(&d->s_body, rec);
        return 0;
}

static int lustre_rename_fop(struct m0_fop *fop, void *data)
{
        struct m0_fop_rename *d = m0_fop_data(fop);
        struct m0_md_lustre_logrec *rec = data;

        if (rec->cr_type == RT_RENAME) {
                lustre_copy_body(&d->r_sbody, rec);
                return lustre_copy_name(&d->r_sname, rec);
        } else {
                lustre_copy_body(&d->r_tbody, rec);
                return lustre_copy_name(&d->r_tname, rec);
        }
}

int m0_md_lustre_fop_alloc(struct m0_fop **fop, void *data,
			   struct m0_rpc_machine *mach)
{
        struct m0_md_lustre_logrec *rec = data;
        fop_translate_t translate = NULL;
        struct m0_fop_type *fopt = NULL;
        int rc1, rc = 0;

        switch (rec->cr_type) {
        case RT_MARK:
        case RT_IOCTL:
        case RT_TRUNC:
        case RT_HSM:
        case RT_XATTR:
                return -EOPNOTSUPP;
        case RT_CREATE:
        case RT_MKDIR:
        case RT_MKNOD:
        case RT_SOFTLINK:
                fopt = &m0_fop_create_fopt;
                translate = lustre_create_fop;
                break;
        case RT_HARDLINK:
                fopt = &m0_fop_link_fopt;
                translate = lustre_link_fop;
                break;
        case RT_UNLINK:
        case RT_RMDIR:
                fopt = &m0_fop_unlink_fopt;
                translate = lustre_unlink_fop;
                break;
        case RT_RENAME:
                fopt = &m0_fop_rename_fopt;
                translate = lustre_rename_fop;
                rc = -EAGAIN;
                break;
        case RT_EXT:
                M0_ASSERT(*fop != NULL);
                translate = lustre_rename_fop;
                break;
        case RT_OPEN:
                fopt = &m0_fop_open_fopt;
                translate = lustre_open_fop;
                break;
        case RT_CLOSE:
                fopt = &m0_fop_close_fopt;
                translate = lustre_close_fop;
                break;
        case RT_SETATTR:
        case RT_MTIME:
        case RT_CTIME:
        case RT_ATIME:
                fopt = &m0_fop_setattr_fopt;
                translate = lustre_setattr_fop;
                break;
        default:
                return -EINVAL;
        }

        if (*fop == NULL) {
                *fop = m0_fop_alloc(fopt, NULL, mach);
                if (*fop == NULL)
                        return -ENOMEM;
        }

        M0_ASSERT(translate != NULL);
        rc1 = translate(*fop, rec);
        if (rc1 != 0) {
		m0_fop_fini(*fop);
		m0_free(*fop);
                *fop = NULL;
                rc = rc1;
        }

        return rc;
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
