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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "ha/msg_queue.h"

#include "lib/memory.h"         /* M0_ALLOC_PTR */

M0_TL_DESCR_DEFINE(ha_mq, "m0_ha_msg_queue::mq_queue", static,
		   struct m0_ha_msg_qitem, hmq_link, hmq_magic,
		   M0_HA_MSG_QITEM_MAGIC, M0_HA_MSG_QUEUE_HEAD_MAGIC);
M0_TL_DEFINE(ha_mq, static, struct m0_ha_msg_qitem);

M0_INTERNAL void m0_ha_msg_queue_init(struct m0_ha_msg_queue     *mq,
                                      struct m0_ha_msg_queue_cfg *cfg)
{
	ha_mq_tlist_init(&mq->mq_queue);
}

M0_INTERNAL void m0_ha_msg_queue_fini(struct m0_ha_msg_queue *mq)
{
	ha_mq_tlist_fini(&mq->mq_queue);
}

M0_INTERNAL struct m0_ha_msg_qitem *
m0_ha_msg_queue_alloc(struct m0_ha_msg_queue *mq)
{
	struct m0_ha_msg_qitem *qitem;

	M0_ALLOC_PTR(qitem);
	return qitem;
}

M0_INTERNAL void m0_ha_msg_queue_free(struct m0_ha_msg_queue *mq,
                                      struct m0_ha_msg_qitem *qitem)
{
	m0_free(qitem);
}

M0_INTERNAL void m0_ha_msg_queue_enqueue(struct m0_ha_msg_queue *mq,
                                         struct m0_ha_msg_qitem *qitem)
{
	ha_mq_tlink_init_at_tail(qitem, &mq->mq_queue);
}

M0_INTERNAL struct m0_ha_msg_qitem *
m0_ha_msg_queue_dequeue(struct m0_ha_msg_queue *mq)
{
	struct m0_ha_msg_qitem *qitem;

	qitem = ha_mq_tlist_pop(&mq->mq_queue);
	if (qitem != NULL)
		ha_mq_tlink_fini(qitem);
	return qitem;
}

M0_INTERNAL void m0_ha_msg_queue_push_front(struct m0_ha_msg_queue *mq,
                                            struct m0_ha_msg_qitem *qitem)
{
	ha_mq_tlink_init_at(qitem, &mq->mq_queue);
}

M0_INTERNAL bool m0_ha_msg_queue_is_empty(struct m0_ha_msg_queue *mq)
{
	return ha_mq_tlist_is_empty(&mq->mq_queue);
}

M0_INTERNAL struct m0_ha_msg_qitem *
m0_ha_msg_queue_find(struct m0_ha_msg_queue *mq,
                     uint64_t                tag)
{
	return m0_tl_find(ha_mq, qitem, &mq->mq_queue,
	                  m0_ha_msg_tag(&qitem->hmq_msg) == tag);
}

M0_INTERNAL struct m0_ha_msg_qitem *
m0_ha_msg_queue_next(struct m0_ha_msg_queue *mq,
                     const struct m0_ha_msg_qitem *cur)
{
	M0_PRE(cur != NULL);
	return ha_mq_tlist_next(&mq->mq_queue, cur);
}

M0_INTERNAL struct m0_ha_msg_qitem *
m0_ha_msg_queue_prev(struct m0_ha_msg_queue *mq,
		     const struct m0_ha_msg_qitem *cur)
{
	M0_PRE(cur != NULL);
	return ha_mq_tlist_prev(&mq->mq_queue, cur);
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
