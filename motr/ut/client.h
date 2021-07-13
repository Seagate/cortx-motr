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

#ifndef __MOTR_UT_H__
#define __MOTR_UT_H__

#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "motr/client.h"
#include "layout/layout.h"      /* m0_layout */

// UT Checksum size
enum {
	CKSUM_SIZE = 16,
};

extern struct m0_config default_config;
#define M0_DEFAULT_EP          "0@lo:12345:45:101"
#define M0_DEFAULT_HA_ADDR     "0@lo:12345:66:1"
#define M0_DEFAULT_PROFILE     "<0x7000000000000001:0>"
#define M0_DEFAULT_PROC_FID    "<0x7200000000000000:0>"
#define SET_DEFAULT_CONFIG() \
	do { \
		struct m0_config *confp = &default_config; \
									 \
		confp->mc_is_oostore            = false; \
		confp->mc_is_read_verify        = false; \
		confp->mc_layout_id             = 1; \
		confp->mc_local_addr            = M0_DEFAULT_EP;\
		confp->mc_ha_addr               = M0_DEFAULT_HA_ADDR;\
		confp->mc_profile               = M0_DEFAULT_PROFILE; \
		confp->mc_process_fid           = M0_DEFAULT_PROC_FID; \
		confp->mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;\
		confp->mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE; \
	} while(0);

static inline int do_init(struct m0_client **instance)
{
	SET_DEFAULT_CONFIG();
	return m0_client_init(instance, &default_config, false);
}

#define INIT(instance) do_init(instance)

/* These values were extracted from m0t1fs */
#define M0T1FS_LAYOUT_P 4
#define M0T1FS_LAYOUT_N 2
#define M0T1FS_LAYOUT_K 1
#define M0T1FS_LAYOUT_S 1
/*Some dummy values to help tests */
#define DUMMY_PTR 0xdeafdead
#define UT_DEFAULT_BLOCK_SIZE (1ULL << M0_DEFAULT_BUF_SHIFT)

/* for layout and instance*/
extern struct m0_layout_enum ut_layout_enum;
extern struct m0_layout_instance_ops ut_layout_instance_ops;

/**
 * Initialises the client UT environment.
 */
M0_INTERNAL int ut_init(void);


/**
 * Finalises the client UT environment.
 */
M0_INTERNAL int ut_fini(void);

/** Fake setup for a realm and entity. */
M0_INTERNAL void ut_realm_entity_setup(struct m0_realm *realm,
				       struct m0_entity *ent,
				       struct m0_client *cinst);

/**
 * A version of m0_client_init for use in unit tests.
 * This will initialise client as far as we can in this environment.
 *
 * @param instance A pointer to where the instance should be stored.
 * @return The value of m0_client_init.
 */
M0_INTERNAL int ut_m0_client_init(struct m0_client **instance);

/**
 * A version of m0_client_fini for use in unit tests.
 * This will finalise whatever was done in ut_m0_client_init.
 *
 * @param instance A pointer to where the instance should be stored.
 */
M0_INTERNAL void ut_m0_client_fini(struct m0_client **instance);

/**
 * A trick to force the UTs to run in random order every time. This allows the
 * tester to discover hidden dependencies among tests (bonus score!).
 */
M0_INTERNAL void ut_shuffle_test_order(struct m0_ut_suite *suite);


/**
 * Fills the layout_domain of a m0_ instance, so it contains only
 * a layout's id.
 * This only guarantees m0_layout_find(M0_DEFAULT_LAYOUT_ID) returns something.
 *
 * @param layout layout to be added to the domain.
 * @param cinst client instance.
 * @remark This might be seen as a workaround to reduce dependencies with other
 * motr components.
 * @remark cinst must have been successfully initialised at least until the
 * IL_HA_STATE level.
 */
void ut_layout_domain_fill(struct m0_client *cinst);
/**
 * Empties a layout domain that has been filled via
 * ut_layout_domain_fill().
 *
 * @param cinst client instance.
 */
void ut_layout_domain_empty(struct m0_client *cinst);
//XXX doxygen
M0_INTERNAL void
ut_striped_layout_fini(struct m0_striped_layout *stl,
			      struct m0_layout_domain *dom);

M0_INTERNAL void
ut_striped_layout_init(struct m0_striped_layout *stl,
		       struct m0_layout_domain *dom);

/* Helper functions for client unit tests*/

#include "motr/pg.h"

extern const struct pargrp_iomap_ops mock_iomap_ops;

/**
 * Creates a UT dummy m0_obj.
 * This allows passing some invariants() forced by the lower layers of Motr.
 */
M0_INTERNAL struct m0_obj *ut_dummy_obj_create(void);

/**
 * Deletes a UT dummy m0_obj.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void ut_dummy_obj_delete(struct m0_obj *obj);

/**
 * Creates a UT dummy m0_pdclust_layout.
 * This allows passing some invariants() forced by the lower layers of Motr.
 */
M0_INTERNAL struct m0_pdclust_layout *
ut_dummy_pdclust_layout_create(struct m0_client *instance);

/**
 * Deletes a UT dummy pdclust_layout.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void
ut_dummy_pdclust_layout_delete(struct m0_pdclust_layout *pl,
			       struct m0_client *instance);

/**
 * Creates a UT dummy m0_pdclust_instance.
 * This allows passing some invariants() forced by the lower layers of Motr.
 */
//M0_INTERNAL struct m0_layout_instance *
M0_INTERNAL struct m0_pdclust_instance *
ut_dummy_pdclust_instance_create(struct m0_pdclust_layout *pdl);

/**
 * Deletes a UT dummy m0_pdclust_instance.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void
ut_dummy_pdclust_instance_delete(struct m0_pdclust_instance *pdi);
//ut_dummy_pdclust_instance_delete(struct m0_layout_instance *layout_inst);

/**
 * Initialises a UT dummy nw_xfer_request.
 */
M0_INTERNAL void ut_dummy_xfer_req_init(struct nw_xfer_request *xfer);

/**
 * Creates a UT dummy nw_xfer_request.
 * This allows passing some invariants() forced by the lower layers of Motr.
 */
M0_INTERNAL struct nw_xfer_request *ut_dummy_xfer_req_create(void);

/**
 * Finalises a UT dummy nw_xfer_request.
 */
M0_INTERNAL void ut_dummy_xfer_req_fini(struct nw_xfer_request *xfer);

/**
 * Deletes a UT dummy nw_xfer_request.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void ut_dummy_xfer_req_delete(struct nw_xfer_request *xfer);

/**
 * Creates a UT dummy data_buf.
 * This allows passing some invariants() forced by the lower layers of Motr.
 */
M0_INTERNAL struct data_buf *
ut_dummy_data_buf_create(void);

/**
 * Deletes a UT dummy data_buf.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void ut_dummy_data_buf_delete(struct data_buf *db);

/**
 * Initialises a UT dummy data_buf.
 */
M0_INTERNAL void ut_dummy_data_buf_init(struct data_buf *db);

/**
 * Finalises a UT dummy data_buf.
 */
M0_INTERNAL void ut_dummy_data_buf_fini(struct data_buf *db);

/*
 * Creates dummy buf's for parity units
 *
 * @param map The parity group
 * @param do_alloc A flag to control whether allocate and initialise
 *                 data buf's.
 */
M0_INTERNAL void ut_dummy_paritybufs_create(struct pargrp_iomap *map,
					    bool do_alloc);
/*
 * Frees dummy buf's for parity units
 *
 * @param map The parity group
 * @param do_free A flag to control whether data buf structures are freed.
 */
M0_INTERNAL void ut_dummy_paritybufs_delete(struct pargrp_iomap *map,
					    bool do_free);

/**
 * Allocate an iomap structure, to be freed by
 * ut_dummy_pargrp_iomap_delete.
 *
 * @param instance The client instance to use.
 * @param num_blocks The number of 4K blocks you will read/write with this map.
 *                   This value must be <= M0T1FS_LAYOUT_N;
 * @return the allocated structure
 * @remark Need to set the pargrp_iomap's pi_ioo to point to a real ioo if
 * invariant(pargrp_iomap) has to pass.
 */
M0_INTERNAL struct pargrp_iomap *
ut_dummy_pargrp_iomap_create(struct m0_client *instance, int num_blocks);

/**
 * Deletes a UT dummy pargrp_iomap.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void
ut_dummy_pargrp_iomap_delete(struct pargrp_iomap *map,
			     struct m0_client *instance);

/**
 * Creates a UT dummy m0_op_io.
 * This allows passing some invariants() forced by the lower layers of Motr.
 */
M0_INTERNAL struct m0_op_io *
ut_dummy_ioo_create(struct m0_client *instance, int num_io_maps);

/**
 * Deletes a UT dummy m0_op_io.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void ut_dummy_ioo_delete(struct m0_op_io *ioo,
				     struct m0_client *instance);

/**
 * Returns the pdclust_layout of an ioo.
 */
M0_INTERNAL struct m0_pdclust_layout *
ut_get_pdclust_layout_from_ioo(struct m0_op_io *ioo);

/**
 * Callback for a ioreq_fop.
 * Executed when an rpc reply is received.
 */
void dummy_ioreq_fop_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast);

/**
 * Creates a UT dummy ioreq_fop.
 * This allows passing some invariants() forced by the lower layers of Motr.
 */
M0_INTERNAL struct ioreq_fop *ut_dummy_ioreq_fop_create(void);

/**
 * Deletes a UT dummy ioreq_fop.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void ut_dummy_ioreq_fop_delete(struct ioreq_fop *fop);

/**
 * Deletes a UT dummy ioreq_fop.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void ut_dummy_ioreq_fop_delete(struct ioreq_fop *fop);


/**
 * Creates a UT dummy target_ioreq.
 * This allows passing some invariants() forced by the lower layers of Motr.
 */
M0_INTERNAL struct target_ioreq *ut_dummy_target_ioreq_create(void);

/**
 * Deletes a UT dummy target_ioreq.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void ut_dummy_target_ioreq_delete(struct target_ioreq *ti);

/**
 * Creates a UT dummy pool machine.
 */
M0_INTERNAL int ut_dummy_poolmach_create(struct m0_pool_version *pv);

/**
 * Deletes a UT dummy pool machine.
 */
M0_INTERNAL void ut_dummy_poolmach_delete(struct m0_pool_version *pv);

M0_INTERNAL void ut_set_device_state(struct m0_poolmach *pm, int dev,
				     enum m0_pool_nd_state state);
#endif /* __MOTR_UT_H__ */

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
