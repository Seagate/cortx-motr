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
 */

#pragma once

#ifndef __MOTR_ISCSERVICE_DEMO_LIBDEMO_H__
#define __MOTR_ISCSERVICE_DEMO_LIBDEMO_H__

#include "lib/types.h"
#include "lib/buf.h"
#include "lib/buf_xc.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"
#include "lib/vec.h"
#include "lib/vec_xc.h"
#include "xcode/xcode_attr.h"

/**
 * Holds the result of min and max computations.
 *
 * The left and right cuts of the values which cross
 * the units boundaries should be glued by the client code
 * and included in the final computation also.
 */
struct mm_result {
	/** Index of the resulting element. */
	uint64_t      mr_idx;
	/** Total number of elements. */
	uint64_t      mr_nr;
	/** The resulting value of the computation. */
	double        mr_val;
	/** Right cut of the boundary value on the left side of unit. */
	struct m0_buf mr_lbuf;
	/** Left cut of the boundary value on the right side of unit. */
	struct m0_buf mr_rbuf;
} M0_XCA_RECORD;

/** Arguments to the target ISC service. */
struct isc_targs {
	struct m0_fid         ist_cob;
	struct m0_io_indexvec ist_ioiv;
} M0_XCA_RECORD;

#endif /* __MOTR_ISCSERVICE_DEMO_LIBDEMO_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
