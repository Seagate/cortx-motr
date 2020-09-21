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



/**
 * @addtogroup ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "ha/msg_queue.h"
#include "ut/ut.h"

#include "lib/misc.h"    /* M0_SET0 */
#include "lib/arith.h"   /* m0_rnd64 */
#include "lib/memory.h"  /* M0_ALLOC_ARR */

enum {
	HA_UT_MSG_QUEUE_NR       = 0x1000,
};

void m0_ha_ut_msg_queue(void)
{
	struct m0_ha_msg_queue      mq = {};
	struct m0_ha_msg_queue_cfg  mq_cfg = {};
	struct m0_ha_msg_qitem     *qitem;
	int                         i;
	uint64_t                    seed = 42;
	uint64_t                   *tags;

	m0_ha_msg_queue_init(&mq, &mq_cfg);
	m0_ha_msg_queue_fini(&mq);

	M0_SET0(&mq);
	m0_ha_msg_queue_init(&mq, &mq_cfg);
	qitem = m0_ha_msg_queue_dequeue(&mq);
	M0_UT_ASSERT(qitem == NULL);
	M0_ALLOC_ARR(tags, HA_UT_MSG_QUEUE_NR);
	M0_UT_ASSERT(tags != NULL);
	for (i = 0; i < HA_UT_MSG_QUEUE_NR; ++i) {
		qitem = m0_ha_msg_queue_alloc(&mq);
		M0_UT_ASSERT(qitem != NULL);
		tags[i] = m0_rnd64(&seed);
		qitem->hmq_msg.hm_tag = tags[i];
		m0_ha_msg_queue_enqueue(&mq, qitem);
	}
	for (i = 0; i < HA_UT_MSG_QUEUE_NR; ++i) {
		qitem = m0_ha_msg_queue_dequeue(&mq);
		M0_UT_ASSERT(qitem != NULL);
		M0_UT_ASSERT(m0_ha_msg_tag(&qitem->hmq_msg) == tags[i]);
		m0_ha_msg_queue_free(&mq, qitem);
	}
	m0_free(tags);
	qitem = m0_ha_msg_queue_dequeue(&mq);
	M0_UT_ASSERT(qitem == NULL);
	m0_ha_msg_queue_fini(&mq);
}


#undef M0_TRACE_SUBSYSTEM

/** @} end of ha group */

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
