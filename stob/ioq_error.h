/* -*- C -*- */
/*
 * Copyright (c) 2016-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_STOB_IOQ_ERROR_H__
#define __MOTR_STOB_IOQ_ERROR_H__

#include "lib/types.h"  /* m0_bindex_t */
#include "fid/fid.h"    /* m0_fid */
#include "stob/stob.h"  /* m0_stob_id */
#include "stob/io.h"    /* m0_stob_io_opcode */
#include "stob/stob_xc.h"       /* XXX workaround */

/**
 * @defgroup stob
 *
 * @{
 */

struct m0_stob_ioq_error {

	/* stob info */

	/** m0_stob_linux::sl_conf_sdev */
	struct m0_fid          sie_conf_sdev;
	/** m0_stob::so_id */
	struct m0_stob_id      sie_stob_id;
	/** m0_stob_linux::sl_fd */
	int64_t                sie_fd;

	/* I/O info */

	/* enum m0_stob_io_opcode */
	int64_t                sie_opcode;
	int64_t                sie_rc;
	m0_bindex_t            sie_offset;
	m0_bcount_t            sie_size;
	uint32_t               sie_bshift;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** @} end of stob group */
#endif /* __MOTR_STOB_IOQ_ERROR_H__ */

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
