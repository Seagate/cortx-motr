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

#ifndef __MOTR_CAS_INDEX_GC_H__
#define __MOTR_CAS_INDEX_GC_H__

/* Import */
struct m0_reqh;
struct m0_be_op;

/** Initialises index garbage collector. */
M0_INTERNAL void m0_cas_gc_init(void);

/** Finalises index garbage collector. */
M0_INTERNAL void m0_cas_gc_fini(void);

/**
 * Instructs index garbage collector to destroy all indices referenced in "dead
 * index" catalogue.
 *
 * After all records in "dead index" catalogue are processed a garbage collector
 * stops. In other words, garbage collector doesn't monitor the contents of
 * "dead index" catalogue and this function should be called every time when
 * user decides to destroy catalogues referenced in "dead index" catalogue.
 *
 * Can be called when garbage collector is already started. In this case garbage
 * collector will not stop after destroying catalogues and will recheck "dead
 * index" contents for new records.
 */
M0_INTERNAL void m0_cas_gc_start(struct m0_reqh_service *service);

/**
 * Asynchronously waits until garbage collector stops.
 *
 * Provided 'beop' will be moved to M0_BOS_DONE state once garbage collector
 * stops.
 */
M0_INTERNAL void m0_cas_gc_wait_async(struct m0_be_op *beop);

/**
 * Synchronously waits until garbage collector stops.
 */
M0_INTERNAL void m0_cas_gc_wait_sync(void);

#endif /* __MOTR_CAS_INDEX_GC_H__ */

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
