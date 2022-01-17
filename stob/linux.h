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

#ifndef __MOTR_STOB_LINUX_H__
#define __MOTR_STOB_LINUX_H__

#include "sm/sm.h"              /* m0_sm_ast */
#include "stob/stob.h"		/* m0_stob_type */
#include "stob/domain.h"	/* m0_stob_domain */
#include "stob/ioq.h"		/* m0_stob_ioq */

/**
   @defgroup stoblinux Storage object based on Linux specific file system
   and block device interfaces.

   @see stob
   @{
 */

struct m0_stob_linux_domain_cfg {
	mode_t sldc_file_mode;
	int    sldc_file_flags;
	bool   sldc_use_directio;
};

struct m0_stob_linux_domain {
	struct m0_stob_domain		 sld_dom;
	struct m0_stob_ioq		 sld_ioq;
	/** parent directory to hold the objects  */
	char				*sld_path;
	/** @see m0_stob_type_ops::sto_domain_cfg_init_parse() */
	struct m0_stob_linux_domain_cfg	 sld_cfg;
};

struct m0_stob_linux {
	struct m0_stob		     sl_stob;
	struct m0_stob_linux_domain *sl_dom;
	/** fd from returned open(2) */
	int			     sl_fd;
	/** file mode as returned by stat(2) */
	mode_t			     sl_mode;
	/** fid of the corresponding m0_conf_sdev object */
	struct m0_fid                sl_conf_sdev;
	/** direct io attribute*/
	bool                         sl_direct_io;
};

M0_INTERNAL struct m0_stob_linux *m0_stob_linux_container(struct m0_stob *stob);
M0_INTERNAL struct m0_stob_linux_domain *
m0_stob_linux_domain_container(struct m0_stob_domain *dom);

/**
 * Reopen the stob to update it's file descriptor.
 * Find the stob from the provided stob_id and destroy it to get rid
 * of the stale fd. Create the stob with provided path to reopen the
 * underlying device, create will also update the stob with new fd.
 */
M0_INTERNAL int m0_stob_linux_reopen(struct m0_stob_id *stob_id,
				     const char *f_path);

/**
 * Associates linux stob with fid of a m0_conf_sdev object.
 * This fid is sent to HA when stob I/O error is reported.
 */
M0_INTERNAL void
m0_stob_linux_conf_sdev_associate(struct m0_stob      *stob,
                                  const struct m0_fid *conf_sdev);

/**
 * Obtains file descriptor of a file which is stored on the same local
 * filesystem where objects are.
 */
M0_INTERNAL int
m0_stob_linux_domain_fd_get(struct m0_stob_domain *dom, int *fd);

/** Closes previously obtained file descriptor. */
M0_INTERNAL int m0_stob_linux_domain_fd_put(struct m0_stob_domain *dom, int fd);

M0_INTERNAL bool m0_stob_linux_domain_directio(struct m0_stob_domain *dom);

extern const struct m0_stob_type m0_stob_linux_type;
M0_INTERNAL char *m0_stob_linux_file_stob(const char *path, const struct m0_fid *stob_fid);
/** @} end group stoblinux */
#endif /* __MOTR_STOB_LINUX_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
