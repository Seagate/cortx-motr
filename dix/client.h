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

#ifndef __MOTR_DIX_CLIENT_H__
#define __MOTR_DIX_CLIENT_H__

/**
 * @addtogroup dix
 *
 * @{
 *
 * Distributed index client provides an interface to access and modify
 * distributed indices. It uses CAS client internally and extends its
 * functionality by providing indices distribution.
 *
 * Please refer to HLD of the distributed indexing for high-level overview.
 *
 * Dependencies
 * ------------
 * - CAS client to make requests to CAS service.
 * - @ref m0_pools_common to locate RPC connections to CAS services in a pool,
 *   where distributed index is stored.
 * - Motr layout functionality to determine destination CAS services for CAS
 *   requests.
 * - Pool machine state for correct operation in degraded mode.
 *
 * Index meta-data
 * ---------------
 * There are three meta-indices that are used by client internally and can be
 * manipulated through appropriate interfaces externally:
 * - Root index. Top-level index to find all other indices. Its pool version
 *   is stored in cluster configuration as one of file system parameters (@ref
 *   m0_conf_root::rt_imeta_pver). Other layout parameters are
 *   hard-coded. Root index contains exactly two records for now with layouts of
 *   'layout' and 'layout-descr' meta-indices. Please note, that pool version
 *   for root index should contain only storage devices controlled by CAS
 *   services, no IOS storage devices are allowed.
 *
 * - Layout index. Holds layouts of "normal" indices. Layouts are stored in the
 *   form of index layout descriptor or index layout identifier.
 *
 * - Layout-descr index. Maps index layout identifiers to full-fledged
 *   index layout descriptors.
 *   User is responsible for index layout identifiers allocation and
 *   populates layout-descr index explicitly with m0_dix_ldescr_put().
 *   Similarly, m0_dix_ldescr_del() deletes mapping between layout identifier
 *   and layout descriptor.
 *
 * Meta-data is global for the file system and normally is created during
 * cluster provisioning via m0_dix_meta_create(). Meta-data can be destroyed via
 * m0_dix_meta_destroy(). Also user is able to check whether meta-data is
 * accessible and is correct via m0_dix_meta_check(). Distributed index
 * meta-data is mandatory and should always be present in the filesystem.
 *
 * Initialisation and start
 * ------------------------
 * Client is initialised with m0_dix_cli_init(). The main argument is a pool
 * version fid of the root index. All subsequent operations use this fid to
 * locate indices in the cluster.
 *
 * User should start the client in order to make DIX requests through the
 * client. Client start procedure is executed through m0_dix_cli_start() or
 * m0_dix_cli_start_sync(). The start procedure involves reading root index to
 * retrieve layouts of 'layout' and 'layout-descr' meta-indeces.
 *
 * There is a special client mode called "bootstrap" mode. In that mode the only
 * request allowed is creating meta-indices in cluster (m0_dix_meta_create()).
 * A client can be moved to this mode just after initialisation using
 * m0_dix_cli_bootstrap() call. It's useful at cluster provisioning state,
 * because client can't start successfully without meta-indices being created in
 * motr file system. After meta indices creation is done, client can be started
 * as usual or finalised.
 *
 * Operation in degraded mode
 * --------------------------
 * DIX client relies on HA notifications to detect device failures. In order to
 * receive HA notifications, the process where DIX client resides should be
 * added to the cluster configuration. On receiving device failure notification
 * local pool machines for all affected pool versions are updated accordingly.
 *
 * DIX client is said to perform an operation over distributed index in degraded
 * mode if at least one device in index pool version has failed or under
 * repair/re-balance. Note, that offline device (i.e. the one with
 * M0_PNDS_OFFLINE state) doesn't imply degraded mode. DIX client doesn't treat
 * offline devices in any special way and the user is likely to get an error
 * during record put/delete operation.
 *
 * DIX client uses component catalogues with spare units instead of component
 * catalogues stored on failed drives during operation in degraded mode. Below
 * is a table showing how DIX client treats target component catalogue for
 * different operations depending on the disk state storing this catalogue.
 *
 * Disk state    |  GET           PUT             DEL            NEXT
 * --------------|---------------------------------------------------
 * FAILED        | Skip        Use spare        Skip             Skip
 *               |
 * REPAIRING     | Skip        Use spare        Use spare        Skip
 *               |
 * REPAIRED      | Use spare   Use spare        Use spare        Skip
 *               |
 * REBALANCING   | Use spare   Use it + spare   Use it + spare   Skip
 * ------------------------------------------------------------------
 *
 * DIX client relies on the fact that device will transit from FAILED to
 * REPAIRING state, and it's not possible to transit from FAILED to ONLINE
 * state. Otherwise, replicas written to spares when the device was FAILED
 * won't be taken into account when disk returns to ONLINE state.
 *
 * NEXT operation always ignores non-online disks, because this operation
 * queries all online disks in a pool version and a layout guarantees that at
 * least one correct replica for every record is available.
 *
 * There is a possible race, where repair/re-balance service sends the old value
 * to the spares/re-balance targets concurrently with a DIX client update. In
 * order to avoid it DEL operation is executed in 2 phases. The process slightly
 * differs for repair and re-balance.
 *   - During repair DIX client updates correct replicas, gets reply, and after
 *     that updates spares. Getting reply for all correct replicas guarantees
 *     that repair service either already processed this record and the spares
 *     are updated or record is not processed at all and successfully deleted.
 *   - During re-balance DIX client updates correct replicas (including spares),
 *     gets reply, and after that updates re-balance target.
 *
 * Client sets special COF_DEL_LOCK flag in CAS request to make a hint to CAS
 * service that special global "delete" lock should be taken for catalogue store
 * prior to deletion. That lock guarantees that CAS service will do record
 * deletion when the record is either already repaired/re-balanced or
 * repair/re-balance process for this record is not started yet.
 *
 * References:
 * - HLD of the distributed indexing :
 *   For documentation links, please refer to this file :
 *   doc/motr-design-doc-list.rst
 */

#include "lib/chan.h"   /* m0_clink */
#include "sm/sm.h"      /* m0_sm */
#include "dix/layout.h" /* m0_dix_ldesc */
#include "dix/meta.h"   /* m0_dix_meta_req */

/* Import */
struct m0_pools_common;
struct m0_layout_domain;
struct m0_rpc_session;
struct m0_be_tx_remid;
struct m0_dix_req;
struct m0_pool_version;
struct m0_fid;
struct m0_dtm0_service;

enum m0_dix_cli_state {
        DIXCLI_INVALID,
        DIXCLI_INIT,
        DIXCLI_BOOTSTRAP,
        DIXCLI_STARTING,
        DIXCLI_READY,
        DIXCLI_FINAL,
        DIXCLI_FAILURE,
};

struct m0_dix_cli {
	struct m0_sm             dx_sm;
	struct m0_clink          dx_clink;
	/** Meta-request to initialise meta-indices during startup. */
	struct m0_dix_meta_req   dx_mreq;
	struct m0_pools_common  *dx_pc;
	struct m0_layout_domain *dx_ldom;
	/** Pool version of the root index. */
	struct m0_pool_version  *dx_pver;
	struct m0_sm_ast         dx_ast;
	struct m0_dix_ldesc      dx_root;
	struct m0_dix_ldesc      dx_layout;
	struct m0_dix_ldesc      dx_ldescr;
	struct m0_dtm0_service  *dx_dtms;

	/**
	 * The callback function is triggerred to update FSYNC records
	 * in Client when an reply fop to an index's update operation
	 * is received.
	 */
	void  (*dx_sync_rec_update)(struct m0_dix_req *,
				    struct m0_rpc_session *,
				    struct m0_be_tx_remid *);
};

/**
 * Initialises DIX client.
 *
 * @param cli      DIX client.
 * @param sm_group SM group for DIX client state machine. Asynchronous
 *                 operations like m0_dix_cli_start() are executed in this SM
 *                 group. Also, this SM group is locked/unlocked in
 *                 m0_dix_lock()/m0_dix_unlock().
 * @param pc       Pools common structure where pool versions of index layouts
 *                 are looked up. Also destination CAS services structures are
 *                 looked up at pc->pc_dev2svc.
 * @param ldom     Layout domain where layout structures for parity math are
 *                 created.
 * @param pver     Pool version of the root index (usually
 *                 m0_conf_root::rt_imeta_pver).
 */
M0_INTERNAL int m0_dix_cli_init(struct m0_dix_cli       *cli,
				struct m0_sm_group      *sm_group,
				struct m0_pools_common  *pc,
			        struct m0_layout_domain *ldom,
				const struct m0_fid     *pver);

/** Locks DIX client SM group. */
M0_INTERNAL void m0_dix_cli_lock(struct m0_dix_cli *cli);

/** Checks whether DIX client SM group is locked. */
M0_INTERNAL bool m0_dix_cli_is_locked(const struct m0_dix_cli *cli);

/** Unlocks DIX client SM group. */
M0_INTERNAL void m0_dix_cli_unlock(struct m0_dix_cli *cli);

/**
 * Starts DIX client asynchronously.
 *
 * DIX client moves its SM (cli->dx_sm) to DIXCLI_READY or DIXCLI_FAILURE state
 * on start procedure finish. If result state is DIXCLI_READY, then DIX client
 * is ready to send DIX requests (see dix/meta.h and dix/req.h).
 *
 * @pre DIX client is initialised or in a "bootstrap" mode.
 */
M0_INTERNAL void m0_dix_cli_start(struct m0_dix_cli *cli);

/**
 * Starts DIX client synchronously.
 *
 * @note Locks DIX SM group internally.
 */
M0_INTERNAL int  m0_dix_cli_start_sync(struct m0_dix_cli *cli);

/**
 * Moves DIX client to special "bootstrap" mode.
 *
 * In that mode the only request allowed is creating meta-indices in cluster
 * (m0_dix_meta_create()). It's the only way to create meta-indices in the
 * cluster, because in normal mode DIX client returns error if meta-indices are
 * not found in the cluster.
 *
 * @pre DIX client is initialised, but not started.
 * @pre m0_dix_cli_is_locked(cli)
 */
M0_INTERNAL void m0_dix_cli_bootstrap(struct m0_dix_cli *cli);

/**
 * The same as m0_dix_cli_bootstrap(), but locks DIX client internally.
 *
 * @pre DIX client is initialised, but not started.
 * @pre !m0_dix_cli_is_locked(cli)
 */
M0_INTERNAL void m0_dix_cli_bootstrap_lock(struct m0_dix_cli *cli);

/**
 * Stops DIX client synchronously.
 *
 * @pre m0_dix_cli_is_locked(cli)
 */
M0_INTERNAL void m0_dix_cli_stop(struct m0_dix_cli *cli);

/**
 * The same as m0_dix_cli_stop(), but locks DIX client internally.
 */
M0_INTERNAL void m0_dix_cli_stop_lock(struct m0_dix_cli *cli);

/**
 * Finalises DIX client.
 *
 * @pre m0_dix_cli_is_locked(cli)
 */
M0_INTERNAL void m0_dix_cli_fini(struct m0_dix_cli *cli);

/**
 * The same as m0_dix_cli_fini(), but locks DIX client internally.
 */
M0_INTERNAL void m0_dix_cli_fini_lock(struct m0_dix_cli *cli);

/** @} end of dix group */

#endif /* __MOTR_DIX_CLIENT_H__ */

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
