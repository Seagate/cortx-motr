/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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
#ifndef __MOTR_BE_BE_H__
#define __MOTR_BE_BE_H__

/**
 * @defgroup be Meta-data back-end
 *
 * This file contains BE details visible to the user.
 *
 * Definitions
 * - BE - metadata backend. Implemented as recoverable virtual memory (RVM) with
 *   write-ahead logging without undo;
 * - Segment - part of virtual memory backed (using BE) by stob;
 * - Region - continuous part of segment. Is defined by address of the first
 *   byte of the region and region size;
 * - Transaction - recoverable set of segment changes;
 * - Capturing - process of saving region contents into transaction's private
 *   memory buffer;
 * - Operation - some code that may or may not need to wait for I/O;
 * - m0_be_op - synchronization primitive that delivers notification about
 *   completion of asynchronous operation;
 *
 * Interfaces available for BE user
 * - m0_be_domain    (be/domain.h);
 * - m0_be_tx        (be/tx.h);
 * - m0_be_tx_credit (be/tx_credit.h);
 * - m0_be_op        (be/op.h);
 * - m0_be_allocator (be/alloc.h);
 * - m0_btree        (btree/btree.h);
 * - m0_be_list      (be/list.h);
 * - m0_be_emap      (be/extmap.h);
 * - m0_be_0type     (be/seg0.h);
 * - m0_be_obj       (be/obj.h).
 *
 * @{
 */

/* These two are called from motr/init.c. */
M0_INTERNAL int  m0_backend_init(void);
M0_INTERNAL void m0_backend_fini(void);

/** @} end of be group */
#endif /* __MOTR_BE_BE_H__ */

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
