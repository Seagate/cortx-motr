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

#ifndef __MOTR_IDX_H__
#define __MOTR_IDX_H__

#include "motr/client.h" /* m0_entity_opcode */
#include "dix/layout.h"    /* m0_dix_ldesc */

/**
 * A client index is a key-value store.
 *
 * For detailed description and usage of the client key-value store, please
 * read motr/client.h.
 *
 * @todo Currently, the user can specify only one start key for NEXT operation,
 * not a set.
 *
 * Generally, the client implementation doesn't guarantee that concurrent calls
 * to the same index are linearizable. But in some configurations there is such
 * a guarantee, e.g. for non-distributed indices with Motr DIX service.
 * See https://en.wikipedia.org/wiki/Linearizability.
 *
 * Client doesn't assume any relationship between index and container. A
 * container can have any number of indices and have to manage them itself.
 * In some cases, multiple containers can even share index.
 *
 * Actual key-value storage functionality is provided by client index back-end
 * service. All existing services are enumerated in m0_idx_service_type.
 */

/** Imported. */
struct m0_op_idx;

/** Types of index services supported by Client. */
enum m0_idx_service_type {
	/** Simple service without persistent storage. */
	M0_IDX_MOCK,
	/**
	 * Service based on Motr distributed indexing component.
	 * Two types of indices are supported:
	 * - distributed index, which is distributed over multiple storage
	 *   devices and network nodes in the cluster for performance,
	 *   scalability and fault tolerance.
	 * - non-distributed index, which is stored on a single node. Client
	 *   user can't choose the node, the first CAS service from Motr
	 *   configuration is used.
	 */
	M0_IDX_DIX,
	/** Service using Cassandra database as persistent storage. */
	M0_IDX_CASS,
	M0_IDX_MAX_SERVICE_ID
};

/** Values of index operation flags supported by Client index operation. */
enum m0_op_idx_flags {
	/**
	 * For M0_IC_PUT operation, instructs it to silently overwrite
	 * existing record with the same key, if any.
	 */
        M0_OIF_OVERWRITE         = 1 << 0,
	/**
	 * For M0_IC_NEXT operation, instructs it to skip record with the
	 * given start key.
	 */
        M0_OIF_EXCLUDE_START_KEY = 1 << 1,
	/**
	 * For M0_IC_PUT/M0_IC_DEL operation, instructs it to
	 * delay the reply until data is persisted.
	 */
        M0_OIF_SYNC_WAIT = 1 << 2
};

/**
 * Query operations for an index service. The operations in this data
 * structure can be divided into 2 groups:
 * (a) Operations over indices: iqo_namei_create/delete/lookup/list.
 * (b) Queries on a specific index: get/put/del/next, see the comments above for
 *     details.
 *
 * Returned value of query operations:
 *     = 0: the query is executed synchronously and returns successfully.
 *     < 0: the query fails.
 *     = 1: the driver successes in launching the query asynchronously.
 *
 * idx_op_ast_complete()/fail() must be called correspondingly when an
 * index operation is completed successfully or fails. This gives Client
 * a chance to take back control and move operation's state forward.
 */
struct m0_idx_query_ops {
	/* Index operations. */
	int  (*iqo_namei_create)(struct m0_op_idx *oi);
	int  (*iqo_namei_delete)(struct m0_op_idx *oi);
	int  (*iqo_namei_lookup)(struct m0_op_idx *oi);
	int  (*iqo_namei_list)(struct m0_op_idx *oi);

	/* Query operations. */
	int  (*iqo_get)(struct m0_op_idx *oi);
	int  (*iqo_put)(struct m0_op_idx *oi);
	int  (*iqo_del)(struct m0_op_idx *oi);
	int  (*iqo_next)(struct m0_op_idx *oi);
};

/** Initialisation and finalisation functions for an index service. */
struct m0_idx_service_ops {
	int (*iso_init) (void *svc);
	int (*iso_fini) (void *svc);
};

/**
 * Client separates the definitions of index service and its instances(ctx)
 * to allow a Client instance to have its own kind of index service.
 */
struct m0_idx_service {
	struct m0_idx_service_ops *is_svc_ops;
	struct m0_idx_query_ops   *is_query_ops;
};

struct m0_idx_service_ctx {
	struct m0_idx_service *isc_service;

	/**
	 * isc_config: service specific configurations.
	 * isc_conn  : connection to the index service
	 */
	void                         *isc_svc_conf;
	void                         *isc_svc_inst;
};

/* Configurations for Cassandra cluster. */
struct m0_idx_cass_config {
	char *cc_cluster_ep;   /* Contact point for a Cassandra cluster. */
	char *cc_keyspace;
	int   cc_max_column_family_num;
};

/**
 * Configuration for Motr DIX (distributed indices) index service.
 */
struct m0_idx_dix_config {
	/**
	 * Indicates whether distributed index meta-data should be created in
	 * file system during service initialisation. Meta-data is global for
	 * the file system and normally is created during cluster provisioning,
	 * so this flag is unset usually. Layouts of 'layout' and 'layout-descr'
	 * indices are provided by kc_layout_ldesc and kc_ldescr_ldesc fields.
	 *
	 * Setting this flag is useful for unit tests.
	 *
	 * See dix/client.h for more information.
	 */
	bool                kc_create_meta;

	/**
	 * Layout of 'layout' meta-index.
	 * Ignored if kc_create_meta is unset.
	 */
	struct m0_dix_ldesc kc_layout_ldesc;

	/**
	 * Layout of 'layout-descr' meta-index.
	 * Ignored if kc_create_meta is unset.
	 */
	struct m0_dix_ldesc kc_ldescr_ldesc;

};

/* BOB types */
extern const struct m0_bob_type oi_bobtype;
M0_BOB_DECLARE(M0_INTERNAL, m0_op_idx);

M0_INTERNAL bool m0__idx_op_invariant(struct m0_op_idx *oi);
M0_INTERNAL void idx_op_ast_complete(struct m0_sm_group *grp,
				     struct m0_sm_ast   *ast);
M0_INTERNAL void idx_op_ast_executed(struct m0_sm_group *grp,
				     struct m0_sm_ast   *ast);
M0_INTERNAL void idx_op_ast_stable(struct m0_sm_group *grp,
				   struct m0_sm_ast   *ast);
M0_INTERNAL void idx_op_ast_fail(struct m0_sm_group *grp,
				 struct m0_sm_ast *ast);


M0_INTERNAL int m0_idx_op_namei(struct m0_entity *entity,
				struct m0_op **op,
				enum m0_entity_opcode opcode);

M0_INTERNAL void m0_idx_service_config(struct m0_client *m0c,
				       int svc_id, void *svc_conf);
M0_INTERNAL void m0_idx_service_register(int svc_id,
				         struct m0_idx_service_ops *sops,
				         struct m0_idx_query_ops   *qops);
M0_INTERNAL void m0_idx_services_register(void);

M0_INTERNAL void m0_idx_mock_register(void);

#ifdef MOTR_IDX_STORE_CASS
M0_INTERNAL void m0_idx_cass_register(void);
#endif

M0_INTERNAL void m0_idx_dix_register(void);

#endif /* __MOTR_IDX_H__ */

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
