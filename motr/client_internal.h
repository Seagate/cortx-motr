/* -*- C -*- */
/*
 * Copyright (c) 2016-2021 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_CLIENT_INTERNAL_H__
#define __MOTR_CLIENT_INTERNAL_H__

#ifdef __KERNEL__
#define M0_CLIENT_THREAD_ENTER M0_THREAD_ENTER
#else
#define M0_CLIENT_THREAD_ENTER
#endif

#define OP_OBJ2CODE(op_obj) op_obj->oo_oc.oc_op.op_code

#define MOCK
#define CLIENT_FOR_M0T1FS

#include "module/instance.h"
#include "motr/init.h"

#include "ioservice/io_fops.h"  /* m0_io_fop_{init,fini,release} */
#include "conf/schema.h"        /* m0_conf_service_type */
#include "conf/confc.h"         /* m0_confc */
#include "layout/pdclust.h"     /* struct m0_pdclust_attr */
#include "pool/pool.h"          /* struct m0_pool */
#include "reqh/reqh.h"          /* struct m0_reqh */
#include "rm/rm.h"              /* stuct m0_rm_owner */
#include "rm/rm_rwlock.h"       /* enum m0_rm_rwlock_req_type */
#include "lib/refs.h"
#include "lib/hash.h"
#include "file/file.h"          /* struct m0_file */
#include "motr/ha.h"            /* m0_motr_ha */
#include "addb2/identifier.h"

/** @todo: remove this - its part of the test framework */
#include "be/ut/helper.h"       /* struct m0_be_ut_backend */

#include "motr/client.h"      /* m0_* */
#include "motr/idx.h"  /* m0_idx_* */
#include "motr/pg.h"          /* nwxfer and friends */
#include "motr/sync.h"        /* sync_request */
#include "fop/fop.h"

struct m0_idx_service_ctx;
struct m0_dtm0_service;
struct m0_dtx;

#ifdef CLIENT_FOR_M0T1FS
/**
 * Maximum length for an object's name.
 */
enum {
	M0_OBJ_NAME_MAX_LEN = 64
};
#endif
/**
 * Number of buckets for m0_::m0_rm_ctxs hash-table.
 */
enum {
	M0_RM_HBUCKET_NR = 100
};

enum m0__entity_states {
	M0_ES_INIT = 1,
	M0_ES_CREATING,
	M0_ES_DELETING,
	M0_ES_OPENING,
	M0_ES_OPEN,
	M0_ES_CLOSING,
	M0_ES_FAILED
};

/**
 * Parity buffers used for addressing an IO request.
 */
enum  m0_pbuf_type {
	/**
	 * Explicitly allocated buffers. This is done during:
	 * i.  Read operation in parity-verify mode (independent of the layout).
	 * ii. Write operation when the layout is not replicated.
	 */
	M0_PBUF_DIR,
	/**
	 * Hold a pointer to data buffer. It's required for write IO on an
	 * object with the replicated layout.
	 */
	M0_PBUF_IND,
	/**
	 * Parity units are not required. Used for read IO without parity
	 * verify mode.
	 */
	M0_PBUF_NONE
};

M0_INTERNAL bool entity_invariant_full(struct m0_entity *ent);
M0_INTERNAL bool entity_invariant_locked(const struct m0_entity *ent);

void m0_op_fini(struct m0_op *op);

struct m0_ast_rc {
	struct m0_sm_ast        ar_ast;
	int                     ar_rc;
	uint64_t                ar_magic;

};

/*
 * Client has a number of nested structures, all of which are passed to the
 * application as a 'struct m0_op'. The application may in fact
 * allocate some of these structures, without knowing what they are, or how
 * big they should be. 'struct m0_op' is always the first member, and
 * contains all the fields the application is permitted to change.
 *
 * A 'struct m0_op' is always contained in a
 * 'struct m0_op_common'. This contains the fields that are common to
 * all operations, namely the launch/executed/finalise callbacks. The
 * application is not permitted to change (or even see) these.
 *
 * Operations then have a different 'super type' depending on whether they are
 * operating on an object, index or realm.
 *
 * Object operations always have a 'struct m0_op_obj', this represents
 * the namespace/metadata aspects of the object, such as its layout. Operations
 * such as create/delete will use this struct as the root 'type' of their work,
 * as they don't need IO buffers etc.
 *
 * 'struct m0_op_io' is the last (and biggest/highest) type, it contains
 * the databuf and paritybuf arrays for reading/writing object data.
 *
 *                   +---m0_op_common-----------+
 *                   |                          |
 *                   |    +-m0_op--------+      |
 *                   |    |              |      |
 *                   |    +--------------+      |
 *                   |                          |
 *                   +--------------------------+
 *
 *                  \/_                        _\/
 *
 * +m0_op_io---------------+      +m0_op_idx--------------+
 * | +m0_op_obj---------+  |      |                       |
 * | |                  |  |      | [m0_op_common]        |
 * | |  [m0_op_common]  |  |      |                       |
 * | |                  |  |      +-----------------------+
 * | +------------------+  |
 * |                       |
 * +-----------------------+
 *
 *  +m0_op_md---------------+     +m0_op_sync-------------+
 *  |                       |     |                       |
 *  | [m0_op_common]        |     | [m0_op_common]        |
 *  |                       |     |                       |
 *  +-----------------------+     +-----------------------+
 *
 */
struct m0_op_common {
	struct m0_op           oc_op;
	uint64_t               oc_magic;

#ifdef MOCK
	/* Timer used to move sm between states*/
	struct m0_sm_timer     oc_sm_timer;
#endif

	void                 (*oc_cb_launch)(struct m0_op_common *oc);
	void                 (*oc_cb_replied)(struct m0_op_common *oc);
	void                 (*oc_cb_cancel)(struct m0_op_common *oc);
	void                 (*oc_cb_fini)(struct m0_op_common *oc);
	void                 (*oc_cb_free)(struct m0_op_common *oc);

	/* Callback operations for states*/
	void                 (*oc_cb_executed)(void *args);
	void                 (*oc_cb_stable)(void *args);
	void                 (*oc_cb_failed)(void *args);
};

/**
 * An index operation.
 */
struct m0_op_idx {
	struct m0_op_common  oi_oc;
	uint64_t             oi_magic;

	struct m0_idx       *oi_idx;

	/* K-V pairs */
	struct m0_bufvec    *oi_keys;
	struct m0_bufvec    *oi_vals;

	/* Hold per key-value query return code. */
	int32_t             *oi_rcs;

	/* Number of queries sent to index*/
	int32_t              oi_nr_queries;
	bool                 oi_query_rc;

	struct m0_sm_group  *oi_sm_grp;
	struct m0_ast_rc     oi_ar;

	/* A bit-mask of m0_op_idx_flags. */
	uint32_t             oi_flags;

	/** ast to cancel index op */
	struct m0_sm_ast    oi_ast;
	/** dix_req for op cancellation */
	struct dix_req     *oi_dix_req;
	/** To know dix req in completion callback */
	bool                oi_in_completion;

	/** Distributed transaction associated with the operation */
	struct m0_dtx      *oi_dtx;
};

/**
 * Generic operation on a client object.
 */
struct m0_op_obj {
	struct m0_op_common         oo_oc;
	uint64_t                    oo_magic;

	struct m0_sm_group         *oo_sm_grp;
	struct m0_ast_rc            oo_ar;

	struct m0_fid               oo_fid;
#ifdef CLIENT_FOR_M0T1FS
	struct m0_fid               oo_pfid;
	struct m0_buf               oo_name;
#endif
	struct m0_fid               oo_pver;     /* cob pool version */
	struct m0_layout_instance  *oo_layout_instance;

	/* MDS fop */
	struct m0_fop              *oo_mds_fop;
};

/**
 * An IO operation on a client object.
 */
struct m0_op_io {
	struct m0_op_obj                  ioo_oo;
	uint64_t                          ioo_magic;

	struct m0_obj                    *ioo_obj;
	struct m0_indexvec                ioo_ext;
	struct m0_bufvec                  ioo_data;
	struct m0_bufvec                  ioo_attr;
	uint64_t                          ioo_attr_mask;
	/** A bit-mask of m0_op_obj_flags. */
	uint32_t                          ioo_flags;
	/** Object's pool version */
	struct m0_fid                     ioo_pver;

	/** @todo: remove this */
	int32_t                           ioo_rc;

	/**
	 * Array of struct pargrp_iomap pointers.
	 * Each pargrp_iomap structure describes the part of parity group
	 * spanned by segments from ::ioo_ext.
	 */
	struct pargrp_iomap             **ioo_iomaps;

	/** Number of pargrp_iomap structures. */
	uint64_t                          ioo_iomap_nr;

	/** Indicates whether data buffers be replicated or not. */
	enum m0_pbuf_type                 ioo_pbuf_type;
	/** Number of pages to read in RMW */
	uint64_t                          ioo_rmw_read_pages;

	/** State machine for this io operation */
	struct m0_sm                      ioo_sm;

	/** Operations for moving along state transitions */
	const struct m0_op_io_ops        *ioo_ops;

	/**
	 * flock here is used to get DI details for a file. When a better way
	 * is found, remove it completely. See cob_init.
	 */
	struct m0_file                    ioo_flock;

	/** Network transfer request */
	struct nw_xfer_request            ioo_nwxfer;

	/**
	* State of SNS repair process with respect to
	* file_to_fid(io_request::ir_file).
	* There are only 2 states possible since Motr client IO path
	* involves a file-level distributed lock on global fid.
	*  - either SNS repair is still due on associated global fid.
	*  - or SNS repair has completed on associated global fid.
	*/
	enum sns_repair_state             ioo_sns_state;

	/**
	 * An array holding ids of failed sessions. The vacant entries are
	 * marked as ~(uint64_t)0.
	 * XXX This is a temporary solution. Sould be removed once
	 * MOTR-899 lands into dev.
	 */
	uint64_t                        *ioo_failed_session;

	/**
	* Total number of parity-maps associated with this request that are in
	* degraded mode.
	*/
	uint32_t                         ioo_dgmap_nr;
	bool                             ioo_dgmode_io_sent;

	/**
	 * Used by copy_{to,from}_application to indicate progress in
	 * log messages
	 */
	uint64_t                         ioo_copied_nr;

	/** Cached map index value from ioreq_iosm_handle_* functions */
	uint64_t                         ioo_map_idx;

	/** Ast for scheduling the 'next' callback */
	struct m0_sm_ast                 ioo_ast;

	/**
	 * Ast for moving state to READ/WRITE COMPLETE and to launch
	 * iosm_handle_executed.
	 */
	struct m0_sm_ast                 ioo_done_ast;

	/** Clink for waiting on another state machine */
	struct m0_clink                  ioo_clink;

	/** Channel to wait for this operation to be finalised */
	struct m0_chan                   ioo_completion;

	/**
	 * In case of a replicated layout indicates whether there is any
	 * corrupted parity group that needs to be rectified.
	 */
	bool                             ioo_rect_needed;

	/**
	 * XXX: get rid of this kludge!
	 * Relying on this to remove duplicate mapping for the same nxfer_req
	 */
	int                              ioo_addb2_mapped;
};

struct m0_io_args {
	struct m0_obj      *ia_obj;
	enum m0_obj_opcode  ia_opcode;
	struct m0_indexvec *ia_ext;
	struct m0_bufvec   *ia_data;
	struct m0_bufvec   *ia_attr;
	uint64_t            ia_mask;
	uint32_t            ia_flags;
};

struct m0_op_md {
	struct m0_op_common mdo_oc;
	struct m0_bufvec    mdo_key;
	struct m0_bufvec    mdo_val;
	struct m0_bufvec    mdo_chk;
};

bool m0_op_md_invariant(const struct m0_op_md *mop);

union m0_max_size_op {
	struct m0_op_io io;
	struct m0_op_md md;
};

/**
 * SYNC operation and related data structures.
 */
struct m0_op_sync {
	struct m0_op_common  os_oc;
	uint64_t             os_magic;

	struct m0_sm_group  *os_sm_grp;
	struct m0_ast_rc     os_ar;

	struct sync_request *os_req;

	/**
	 * Mode to set the fsync fop (m0_fop_fsync::ff_fsync_mode).
	 * mdservice/fsync_fops.h defines 2 modes: M0_FSYNC_MODE_ACTIVE and
	 * M0_FSYNC_MODE_PASSIVE. In passive mode the fsync fom merely
	 * waits for the transactions to become committed, in active mode it
	 * uses m0_be_tx_force(), to cause the transactions to make progress
	 * more quickly than they otherwise would.
	 */
	int32_t              os_mode;
};

/**
 * SM states of component object (COB) request.
 */
enum m0_cob_req_states {
	COB_REQ_ACTIVE,
	COB_REQ_SENDING,
	COB_REQ_DONE,
} M0_XCA_ENUM;

/**
 * Request to ioservice component object (COB).
 */
struct m0_ios_cob_req {
	struct m0_op_obj *icr_oo;
	uint32_t          icr_index;
	struct m0_ast_rc  icr_ar;
	uint64_t          icr_magic;
};

struct m0_client_layout_ops {
	int  (*lo_alloc) (struct m0_client_layout **);
	int  (*lo_get) (struct m0_client_layout *);
	void (*lo_put) (struct m0_client_layout *);
	/** Function to construct IO for an object. */
	int  (*lo_io_build)(struct m0_io_args *io_args, struct m0_op **op);
};

/** miscallaneous constants */
enum {
	/*  4K, typical linux/intel page size */
#ifdef CONFIG_X86_64
	M0_DEFAULT_BUF_SHIFT        = 12,
#elif defined CONFIG_AARCH64 /*aarch64*/
	M0_DEFAULT_BUF_SHIFT        = 16,
#endif
	/* 512, typical disk sector */
	M0_MIN_BUF_SHIFT            = 9,

	/* RPC */
	M0_RPC_TIMEOUT              = 60, /* Seconds */
	M0_RPC_MAX_RETRIES          = 60,
	M0_RPC_RESEND_INTERVAL      = M0_MKTIME(M0_RPC_TIMEOUT, 0) /
					  M0_RPC_MAX_RETRIES,
	M0_MAX_NR_RPC_IN_FLIGHT     = 100,

	M0_AST_THREAD_TIMEOUT       = 10,
	M0_MAX_NR_CONTAINERS        = 1024,

	M0_MAX_NR_IOS               = 128,
	M0_MD_REDUNDANCY            = 3,

	/*
	 * These constants are used to create buffers acceptable to the
	 * network code.
	 */
	
#ifdef CONFIG_X86_64
	M0_NETBUF_MASK              = 4096 - 1,
	M0_NETBUF_SHIFT             = 12,
#elif defined CONFIG_AARCH64 /*aarch64*/
	M0_NETBUF_MASK              = 65536 - 1,
	M0_NETBUF_SHIFT             = 16,
#endif
};

/**
 * The initlift state machine moves in one of these two directions.
 */
enum initlift_direction {
	SHUTDOWN = -1,
	STARTUP = 1,
};

/**
 * m0_ represents a client 'instance', a connection to a motr cluster.
 * It is initalised by m0_client_init, and finalised with m0_client_fini.
 * Any operation to open a realm requires the client instance to be specified,
 * allowing an application to work with multiple motr clusters.
 *
 * The prefix m0c is used over 'ci', to avoid confusion with colibri inode.
 */
struct m0_client {
	uint64_t                                m0c_magic;

	/** Motr instance */
	struct m0                              *m0c_motr;

	/** State machine group used for all operations and entities. */
	struct m0_sm_group                      m0c_sm_group;

	/** Request handler for the instance*/
	struct m0_reqh                          m0c_reqh;

	struct m0_clink                         m0c_conf_exp;
	struct m0_clink                         m0c_conf_ready;
	struct m0_clink                         m0c_conf_ready_async;

	struct m0_fid                           m0c_process_fid;
	struct m0_fid                           m0c_profile_fid;

	/**
	 * The following fields picture the pools in motr.
	 * m0_pools_common: details about all pools in motr.
	 * m0c_pool: current pool used by this client instance
	 */
	struct m0_pools_common                  m0c_pools_common;

	/** HA service context. */
	struct m0_reqh_service_ctx             *m0c_ha_rsctx;

	struct m0_motr_ha                       m0c_motr_ha;

	/** Index service context. */
	struct m0_idx_service_ctx               m0c_idx_svc_ctx;

	/**
	 * Instantaneous count of pending io requests.
	 * Every io request increments this value while initializing
	 * and decrements it while finalizing.
	 */
	struct m0_atomic64                      m0c_pending_io_nr;

	/** Indicates the state of confc.  */
	struct m0_confc_update_state            m0c_confc_state;

	/** Channel on which motr internal data structures refreshed
	 *  as per new configurtion event brodcast.
	 */
	struct m0_chan                          m0c_conf_ready_chan;

	/**
	 * Reference counter of this configuration instance users.
	 * When it drops to zero, all data structures using configuration
	 * cache can be refreshed.
	 * [idx_op|obj_io]_cb_launch get ref using m0__io_ref_get
	 * and idx_op_complete & ioreq_iosm_handle_executed put
	 * ref using m0__io_ref_put.
	 */
	struct m0_ref                           m0c_ongoing_io;

	/** Special thread which runs ASTs from io requests. */
	/* Also required for confc to connect! */
	struct m0_thread                        m0c_astthread;

	/** flag used to make the ast thread exit */
	bool                                    m0c_astthread_active;

	/** Channel on which io waiters can wait. */
	struct m0_chan                          m0c_io_wait;

#ifdef CLIENT_FOR_M0T1FS
	/** Root fid, retrieved from mdservice in mount time. */
	struct m0_fid                           m0c_root_fid;

	/** Maximal allowed namelen (retrived from mdservice) */
	int                                     m0_namelen;
#endif
	/** local endpoint address module parameter */
	char                                   *m0c_laddr;
	struct m0_net_xprt                     *m0c_xprt;
	struct m0_net_domain                    m0c_ndom;
	struct m0_net_buffer_pool               m0c_buffer_pool;
	struct m0_rpc_machine                   m0c_rpc_machine;

	/** Client configuration, it takes place of m0t1fs mount options*/
	struct m0_config                       *m0c_config;

	/**
	 * m0c_initlift_xxx fields control the progress of init/fini a
	 * client instance.
	 *  - sm: the state machine for initialising this client instance
	 *  - direction: up or down (init/fini)
	 *  - rc: the first failure value when aborting initialisation.
	 */
	struct m0_sm                            m0c_initlift_sm;
	enum initlift_direction                 m0c_initlift_direction;
	int                                     m0c_initlift_rc;

#ifdef MOCK
	struct m0_htable                        m0c_mock_entities;
#endif

	struct m0_htable                        m0c_rm_ctxs;

	struct m0_dtm0_service                 *m0c_dtms;
};

/** CPUs semaphore - to control CPUs usage by parity calcs. */
extern struct m0_semaphore cpus_sem;

/**
 * Represents the context needed for the RM to lock and unlock
 * object resources (here motr objects).
 */
struct m0_rm_lock_ctx {
	/**
	 * Locking mechanism provided by RM, rmc_rw_file::rwl_fid contains
	 * fid of gob.
	 */
	struct m0_rw_lockable   rmc_rw_file;
	/** An owner for maintaining file locks. */
	struct m0_rm_owner      rmc_owner;
	/** Remote portal for requesting resource from creditor. */
	struct m0_rm_remote     rmc_creditor;
	/** Key for the hash-table */
	struct m0_fid           rmc_key;
	/** Fid for resource owner */
	struct m0_fid           rmc_own_fid;
	/**
	 * Reference counter to book keep how many operations are using
	 * this rm_ctx.
	 */
	struct m0_ref           rmc_ref;
	/** back pointer to hash-table where this rm_ctx will be stored. */
	struct m0_htable       *rmc_htable;
	/** A linkage in hash-table for storing RM lock contexts. */
	struct m0_hlink         rmc_hlink;
	uint64_t                rmc_magic;
	/** A generation count for cookie associated with this ctx. */
	uint64_t                rmc_gen;
};

/** Methods for hash-table holding rm_ctx for RM locks */
M0_HT_DECLARE(rm_ctx, M0_INTERNAL, struct m0_rm_lock_ctx, struct m0_fid);

/**
 * A wrapper structure over m0_rm_incoming.
 * It represents a request to borrow/sublet resource
 * form remote RM creditor and lock/unlock the object.
 */
struct m0_rm_lock_req {
	struct m0_rm_incoming rlr_in;
	struct m0_mutex       rlr_mutex;
	int32_t               rlr_rc;
	struct m0_chan        rlr_chan;
};

/**
 * Acquires the RM lock for the object asynchronously.
 *
 * This function requests RM creditor (remote or local) to acquire the rights
 * to use a resource, attaches a clink to the lock_req channel and returns.
 * The clink will be signalled when the resource has been granted, hence the
 * application should wait on the clink before executing any code which
 * absolutely requires the object to be locked.
*/
M0_INTERNAL int m0_obj_lock_get(struct m0_obj *obj,
				struct m0_rm_lock_req *req,
				struct m0_clink *clink,
				enum m0_rm_rwlock_req_type rw_type);

/**
 * Acquires the RM lock for the object.
 * This is a blocking function.
 */
M0_INTERNAL int m0_obj_lock_get_sync(struct m0_obj *obj,
				     struct m0_rm_lock_req *req,
				     enum m0_rm_rwlock_req_type rw_type);

/**
 * Bob's for shared data structures in files
 */
extern const struct m0_bob_type oc_bobtype;
extern const struct m0_bob_type oo_bobtype;
extern const struct m0_bob_type op_bobtype;
extern const struct m0_bob_type ar_bobtype;

M0_BOB_DECLARE(M0_INTERNAL, m0_op_common);
M0_BOB_DECLARE(M0_INTERNAL, m0_op_obj);
M0_BOB_DECLARE(M0_INTERNAL, m0_op);
M0_BOB_DECLARE(M0_INTERNAL, m0_ast_rc);

/** global init/fini, used by motr/init.c */
M0_INTERNAL int m0_client_global_init(void);
M0_INTERNAL void m0_client_global_fini(void);

/**
 * Gets the confc from client instance.
 *
 * @param m0c client instance.
 * @return the confc used by this client instance.
 */
M0_INTERNAL struct m0_confc* m0_confc(struct m0_client *m0c);

M0_INTERNAL int m0_op_executed(struct m0_op *op);
M0_INTERNAL int m0_op_stable(struct m0_op *op);
M0_INTERNAL int m0_op_failed(struct m0_op *op);
M0_INTERNAL int m0_op_get(struct m0_op **op, size_t size);

/**
 * Returns the m0_client client instance, found from the provided operation.
 *
 * @param op The Operation to find the instance for.
 * @return A pointer to the m0_ instance.
 */
M0_INTERNAL struct m0_client *
m0__entity_instance(const struct m0_entity *entity);

/**
 * Returns the m0_ client instance, found from the provided operation.
 *
 * @param op The Operation to find the instance for.
 * @return A pointer to the m0_ instance.
 */
M0_INTERNAL struct m0_client *
m0__op_instance(const struct m0_op *op);

/**
 * Returns generic client op from io op.
 */
M0_INTERNAL struct m0_op *
m0__ioo_to_op(struct m0_op_io *ioo);

/**
 * Returns the m0_ client instance, found from the provided object.
 *
 * @param obj The object to find the instance for.
 * @return A pointer to the m0_ instance.
 */
M0_INTERNAL struct m0_client *
m0__obj_instance(const struct m0_obj *obj);

/**
 * Returns the client instance associated to an object operation.
 *
 * @param oo object operation pointing to the instance.
 * @return a pointer to the client instance associated to the entity.
 */
M0_INTERNAL struct m0_client*
m0__oo_instance(struct m0_op_obj *oo);

/**
 * Returns if client instance is operating under oostore mode.
 */
M0_INTERNAL bool m0__is_oostore(struct m0_client *instance);

/* sm conf that needs registering by m0_client_init */
extern struct m0_sm_conf m0_op_conf;
extern struct m0_sm_conf entity_conf;

/* used by the entity code to create operations */
M0_INTERNAL int m0_op_alloc(struct m0_op **op, size_t op_size);

M0_INTERNAL int m0_op_init(struct m0_op *op,
                           const struct m0_sm_conf *conf,
			   struct m0_entity *entity);

/* XXX juan: add doxygen */
M0_INTERNAL void m0_client_init_io_op(void);

/**
 * Checks the data struct holding the AST information  is not malformed
 * or corrupted.
 *
 * @param ar The pointer to AST information.
 * @return true if the operation is not malformed or false if some error
 * was detected.
 */
M0_INTERNAL bool m0_op_obj_ast_rc_invariant(struct m0_ast_rc *ar);

/**
 * Checks an object operation is not malformed or corrupted.
 *
 * @param oo object operation to be checked.
 * @return true if the operation is not malformed or false if some error
 * was detected.
 */
M0_INTERNAL bool m0_op_obj_invariant(struct m0_op_obj *oo);

/**
 * Checks an object's IO operation is not malformed or corrupted.
 *
 * @param iop object's IO operation to be checked.
 * @return true if the operation is not malformed or false if some error
 * was detected.
 */
M0_INTERNAL bool m0_op_io_invariant(const struct m0_op_io *iop);

/**
 * Retrieves the ios session corresponding to a container_id. The ioservice
 * for an object is calculated from the container id.
 *
 * @param cinst client instance.
 * @param container_id container ID.
 * @return the session associated to the
 * @remark container_id == 0 is not valid.
 */
M0_INTERNAL struct m0_rpc_session *
m0_obj_container_id_to_session(struct m0_pool_version *pv,
			       uint64_t container_id);

/**
 * Selects a locality for an operation.
 *
 * @param m0c The client instance we are working with.
 * @return the pointer to assigned locality for success, NULL otherwise.
 */
M0_INTERNAL struct m0_locality *
m0__locality_pick(struct m0_client *cinst);

/**
 * Checks object's cached pool version is valid.
 *
 * @param obj The object to be checked.
 * @return true for valid pool version, false otherwise.
 */
M0_INTERNAL bool
m0__obj_poolversion_is_valid(const struct m0_obj *obj);

/**
 * Sends COB fops to mdservices or ioservices depending on COB operation's
 * protocol.
 *
 * @param oo object operation being processed.
 * @return 0 if success or an error code otherwise.
 */
M0_INTERNAL int m0__obj_namei_send(struct m0_op_obj *oo);

/**
 * Cancels fops sent during namei launch operation
 *
 * @param op operation to be cancelled
 * @return 0 if success ot an error code otherwise
 */
M0_INTERNAL int m0__obj_namei_cancel(struct m0_op *op);

/**
 * Cancels fops sent during dix index operation
 *
 * @param op idx to be cancelled
 * @return 0 if success ot an error code otherwise
 */
M0_INTERNAL int m0__idx_cancel(struct m0_op_idx *oi);

/**
 * Get object's attributes from services synchronously.
 *
 * @param obj object to be queried for.
 * @return 0 if success or an error code otherwise.
 */
M0_INTERNAL int m0__obj_attr_get_sync(struct m0_obj *obj);

/**
 * Reads the specified layout from the mds.
 *
 * @param m0c The client instance we are working with, contains the layout db.
 * @param lid The layout identifier to read.
 * @param l_out Where to store the resultant layout.
 * @return 0 for success, an error code otherwise.
 */
M0_INTERNAL int m0_layout_mds_lookup(struct m0_client         *m0c,
				     uint64_t                  lid,
				     struct m0_client_layout **l_out);

/**
 * Initialises an entity.
 *
 * @param entity Entity to be initialised.
 * @param parent Parent realm of the entity.
 * @param id Identifier of the entity.
 * @param type Type of the entity.
 */
M0_INTERNAL void m0_entity_init(struct m0_entity         *entity,
				struct m0_realm          *parent,
				const struct m0_uint128  *id,
				const enum m0_entity_type type);
/**
 * Gets current valid pool version from client instance.
 *
 * @param instance The client instance containing information of pool and pool
 *                 versions.
 * @param pv The returned pool version.
 */
M0_INTERNAL int
m0__obj_pool_version_get(struct m0_obj *obj,
			 struct m0_pool_version **pv);

/**
 * Gets the default layout identifier from confd.
 *
 * @param instance The client instance containing information of confd.
 * @return Default layout id.
 */
M0_INTERNAL uint64_t
m0__obj_layout_id_get(struct m0_op_obj *oo);

/**
 * Builds a layout instance using the supplied layout.
 *
 * @param cinst client instance.
 * @param layout_id ID of the layout.
 * @param fid (global) fid of the object this instance is associated to.
 * @param[out] linst new layout instance.
 * @return 0 if the operation succeeds or an error code (<0) otherwise.
 * @remark This function might trigger network traffic.
 */
M0_INTERNAL int
m0__obj_layout_instance_build(struct m0_client           *cinst,
			      const uint64_t              layout_id,
			      const struct m0_fid        *fid,
			      struct m0_layout_instance **linst);

/**
 * Fetches the pool version of supplied object and stores as an object
 * attribute.
 *
 * @param obj object whose pool version needs to be found.
 * @return 0 if the operation succeeds or an error code (<0) otherwise.
 */
M0_INTERNAL int m0__cob_poolversion_get(struct m0_obj *obj);

M0_INTERNAL int obj_fid_make_name(char *name, size_t name_len,
				  const struct m0_fid *fid);
/**
 * TODO: doxygen
 */
M0_INTERNAL struct m0_obj*
m0__obj_entity(struct m0_entity *entity);
M0_INTERNAL uint64_t m0__obj_lid(struct m0_obj *obj);
M0_INTERNAL enum m0_client_layout_type
m0__obj_layout_type(struct m0_obj *obj);
M0_INTERNAL struct m0_fid m0__obj_pver(struct m0_obj *obj);
M0_INTERNAL void m0__obj_attr_set(struct m0_obj *obj,
				  struct m0_fid  pver,
				  uint64_t       lid);
M0_INTERNAL bool
m0__obj_pool_version_is_valid(const struct m0_obj *obj);
M0_INTERNAL int m0__obj_io_build(struct m0_io_args *args,
				 struct m0_op     **op);
M0_INTERNAL void m0__obj_op_done(struct m0_op *op);

M0_INTERNAL bool m0__is_read_op(struct m0_op *op);
M0_INTERNAL bool m0__is_update_op(struct m0_op *op);

M0_INTERNAL int m0__io_ref_get(struct m0_client *m0c);
M0_INTERNAL void m0__io_ref_put(struct m0_client *m0c);
M0_INTERNAL struct m0_file *m0_client_fop_to_file(struct m0_fop *fop);
M0_INTERNAL bool entity_id_is_valid(const struct m0_uint128 *id);

/** @} end of client group */

#endif /* __MOTR_CLIENT_INTERNAL_H__ */

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
