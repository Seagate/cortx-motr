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

#ifndef __MOTR_MOTR_MOTR_SETUP_H__
#define __MOTR_MOTR_MOTR_SETUP_H__
#ifndef __KERNEL__
#include <stdio.h> /* FILE */
#endif
#include "lib/tlist.h"
#include "lib/types.h"        /* m0_uint128 */
#include "reqh/reqh_service.h"
#include "stob/stob.h"
#include "net/lnet/lnet.h"    /* M0_NET_LNET_XEP_ADDR_LEN */
#include "net/buffer_pool.h"
#include "mdstore/mdstore.h"  /* m0_mdstore */
#include "fol/fol.h"          /* m0_fol */
#include "reqh/reqh.h"        /* m0_reqh */
#ifndef __KERNEL__
#include "yaml.h" 
#endif            /* yaml_document_t */

#include "be/ut/helper.h"     /* m0_be_ut_backend_seg_add2 */
#include "pool/pool.h"        /* m0_pools_common */
#include "ha/ha.h"            /* m0_ha */
#include "motr/ha.h"          /* m0_motr_ha */
#include "module/module.h"    /* m0_module */

/**
   @defgroup m0d Motr Setup

   Motr setup program configures a user space motr context
   on a node in a cluster.
   There exist a list of network transports supported by a node,
   which is used to initialise corresponding network domains per motr
   context, so there exist a network domain per network transport.
   There can exist multiple request handlers per motr context.
   Every motr context configures one or more request handler
   contexts, one per request handler, each containing a storage domain,
   data base, cob domain, fol and request handler to be initialised.
   Every request handler contains a list of rpc machines, each configured
   per given endpoint per network domain.
   Network domains are shared between multiple request handlers in a
   motr context.
   There exist multiple services within a motr context.
   Each service identifies a particular set of operations that can be
   executed on a particular node.
   Services are registered with the request handler which performs the
   execution of requests directed to a particular service. Thus the
   services run under request handler context.

   Motr setup can be done internally through motr code or externally
   through cli using m0d program. As motr setup configures
   the server it should be used in server side initialisation. if done
   through code, Following has to be done to configure a motr context:

   - Initialise motr context:
     For this you have to first define an array of network transports
     to be used in the motr context and pass it along with the array
     size to the initialisation routine.

   @note Also user should pass a output file descriptor to which the error
         messages will be directed.
   @code
   struct m0_motr motr_ctx;
   static struct m0_net_xprt *xprts[] = {
        &m0_net_lnet_xprt,
	...
    };

   m0_cs_init(&motr_ctx, xprts, ARRAY_SIZE(xprts), outfile);
   @endcode

   Define parameters for motr setup and setup environment as below,

   @code
   static char *cmd[] = { "m0d", "-T", "AD",
                   "-D", "cs_db", "-S", "cs_stob",
                   "-e", "lnet:172.18.50.40@o2ib1:12345:34:1",
                   "-s", "dummy"};

    m0_cs_setup_env(&motr_ctx, ARRAY_SIZE(cs_cmd), cs_cmd);
    @endcode

    Once the environment is setup successfully, the services can be started
    as below,
    @code
    m0_cs_start(&srv_motr_ctx);
    @endcode

    @note The specified services to be started should be registered before
          startup.

    Failure handling for m0d is done as follows,
    - As mentioned above, user must follow the sequence of m0_cs_init(),
      m0_cs_setup_env(), and m0_cs_start() in-order to setup m0_motr instance
      programmatically. If m0_cs_init() fails, user need not invoke
      m0_cs_fini(), although if m0_cs_init() succeeds and if further calls to
      m0d routines fail i.e m0_cs_setup_env() or cs_cs_start(), then user must
      invoke m0_cs_fini() corresponding to m0_cs_init().

    Similarly, to setup motr externally, using m0d program along
    with parameters specified as above.
    e.g. ./m0d -T linux -D dbpath -S stobfile \
           -e xport:172.18.50.40@o2ib1:12345:34:1 -s 'service:ServiceFID'

    Below image gives an overview of entire motr context.
    @note This image is borrowed from the "New developer guide for motr"
	  document in section "Starting Motr services".

    @image html "../../motr/DS-Reqh.gif"

   @{
 */

enum {
	M0_SETUP_DEFAULT_POOL_WIDTH = 10
};

enum {
	M0_AD_STOB_DOM_KEY_DEFAULT = 0x1,
	M0_ADDB2_STOB_DOM_KEY      = 0xaddbf11e, /* ADDB file */
	M0_BE_SEG_SIZE_DEFAULT	   = 128 * 1024 * 1024ULL,
};

enum stob_type {
	M0_LINUX_STOB,
	M0_AD_STOB,
	M0_STOB_TYPE_NR
};

/** String representations corresponding to the stob types. */
M0_EXTERN const char *m0_cs_stypes[M0_STOB_TYPE_NR];

/**
 * Auxiliary structure used to pass command line arguments to cs_parse_args().
 */
struct cs_args {
	int    ca_argc;
	int    ca_argc_max;
	char **ca_argv;
};

/**
   Contains extracted network endpoint and transport from motr endpoint.
 */
struct cs_endpoint_and_xprt {
	/**
	   motr endpoint specified as argument.
	 */
	const char      *ex_cep;
	/**
	   4-tuple network layer endpoint address.
	   e.g. 172.18.50.40@o2ib1:12345:34:1
	 */
	const char      *ex_endpoint;
	/** Supported network transport. */
	const char      *ex_xprt;
	/**
	   Scratch buffer for endpoint and transport extraction.
	 */
	char            *ex_scrbuf;
	uint64_t         ex_magix;
	/** Linkage into reqh context endpoint list, m0_reqh_context::rc_eps */
	struct m0_tlink  ex_linkage;
	/**
	   Unique Colour to be assigned to each TM.
	   @see m0_net_transfer_mc::ntm_pool_colour.
	 */
	uint32_t	 ex_tm_colour;
};

/**
 * Represent devices configuration file in form of yaml document.
 * @note This is temporary implementation in-order to configure device as
 *       a stob. This may change when confc implementation lands into dev.
 * @todo XXX FIXME: confc has landed ages ago.
 */
struct cs_stob_file {
	bool            sf_is_initialised;
#ifndef __KERNEL__
	yaml_document_t sf_document;
#endif
};

/**
 * Structure which encapsulates backing store domain for IO storage devices
 * and structure for IO storage devices configuration file.
 */
struct cs_stobs {
	/** Linux storage domain. */
	struct m0_stob_domain *s_sdom;
	/** Devices configuration. */
	struct cs_stob_file    s_sfile;
	/** Initialise AD disk storage. */
	bool                   s_ad_disks_init;
};

/** States of m0_motr::cc_reqh_ctx. */
enum cs_reqh_ctx_states {
	RC_UNINITIALISED,
	RC_REQH_INITIALISED,
	RC_INITIALISED
};

/**
   Represents a request handler environment.
   It contains configuration information about the various global entities
   to be configured and their corresponding instances that are needed to be
   initialised before the request handler is started, which by itself is
   contained in the same structure.
 */
struct m0_reqh_context {
	/** Storage path for request handler context. */
	const char                  *rc_stpath;

	/** ADDB Storage location for request handler ADDB machine */
	const char                  *rc_addb_stlocation;

	/** Path to device configuration file. */
	const char                  *rc_dfilepath;

	/** Type of storage to be initialised. */
	const char                  *rc_stype;

	/** BE environment path for request handler context. */
	const char                  *rc_bepath;

	/** Services running in request handler context. */
	char                       **rc_services;

	/** Service fids */
	struct m0_fid               *rc_service_fids;

	/** Number of services configured in request handler context. */
	uint32_t                     rc_nr_services;

	/** Endpoints and xprts per request handler context. */
	struct m0_tl                 rc_eps;

	/**
	    State of a request handler context, i.e. RC_INITIALISED or
	    RC_UNINTIALISED.
	 */
	enum cs_reqh_ctx_states      rc_state;

	/** Storage domain for a request handler */
	struct cs_stobs              rc_stob;

	/** BE env and segment used by the request handler */
	struct m0_be_ut_backend      rc_be;
	struct m0_be_seg            *rc_beseg;

	/**
	 * Path to BE log, seg0 and primary segment.
	 * File in BE domain stob domain is used if for those are NULL.
	 */
	const char		    *rc_be_log_path;
	const char		    *rc_be_seg0_path;
	const char		    *rc_be_seg_path;
	/** BE primary segment size for m0mkfs. */
	m0_bcount_t		     rc_be_seg_size;
	m0_bcount_t		     rc_be_log_size;
	m0_bcount_t                  rc_be_tx_group_tx_nr_max;
	m0_bcount_t                  rc_be_tx_group_reg_nr_max;
	m0_bcount_t                  rc_be_tx_group_reg_size_max;
	m0_bcount_t                  rc_be_tx_group_payload_size_max;
	m0_bcount_t                  rc_be_tx_reg_nr_max;
	m0_bcount_t                  rc_be_tx_reg_size_max;
	m0_bcount_t                  rc_be_tx_payload_size_max;
	m0_time_t                    rc_be_tx_group_freeze_timeout_min;
	m0_time_t                    rc_be_tx_group_freeze_timeout_max;

	/**
	 * Default path to the configuration database.
	 *
	 * If confd is started by "sss" service (dynamically) and
	 * m0_sssservice_req::ss_param is not empty, then the value of
	 * m0_sssservice_req::ss_param will be used as conf DB path.
	 *
	 * @see m0_reqh_service::rs_ss_param
	 */
	const char                  *rc_confdb;

	/** Cob domain to be used by the request handler */
	struct m0_mdstore            rc_mdstore;

	struct m0_cob_domain_id      rc_cdom_id;

	/** File operation log for a request handler */
	struct m0_fol               *rc_fol;

	/** Request handler instance to be initialised */
	struct m0_reqh               rc_reqh;

	/** Reqh context magic */
	uint64_t                     rc_magix;

	/** Backlink to struct m0_motr. */
	struct m0_motr              *rc_motr;

	/**
	 * Minimum number of buffers in TM receive queue.
	 * Default is set to m0_motr::cc_recv_queue_min_length
	 */
	uint32_t                     rc_recv_queue_min_length;

	/**
	 * Maximum RPC message size.
	 * Default value is set to m0_motr::cc_max_rpc_msg_size
	 * If value of cc_max_rpc_msg_size is zero then value from
	 * m0_net_domain_get_max_buffer_size() is used.
	 */
	uint32_t                     rc_max_rpc_msg_size;

	/** Preallocate an entire stob for db emulation BE segment */
	bool                         rc_be_seg_preallocate;

	/** Process FID */
	struct m0_fid                rc_fid;

	/** Disable direct I/O for data from clients */
	bool                         rc_disable_direct_io;

	/** Enable Fault Injection Service */
	bool                         rc_fis_enabled;

	/** ADDB Record Max record size in bytes */
	m0_bcount_t                  rc_addb_record_file_size;
};

/**
   Defines "Motr context" structure, which contains information on
   network transports, network domains and a request handler.
 */
struct m0_motr {
	/** Resources shared between multiple pools. */
	struct m0_pools_common      cc_pools_common;

	/** Protects access to m0_motr members. */
	struct m0_rwlock            cc_rwlock;

	struct m0_reqh_context      cc_reqh_ctx;

	/** Array of network transports supported in a motr context. */
	struct m0_net_xprt        **cc_xprts;

	/** Size of cc_xprts array. */
	size_t                      cc_xprts_nr;

	/**
	   List of network domain per motr context.

	   @see m0_net_domain::nd_app_linkage
	 */
	struct m0_tl                cc_ndoms;

	/**
	   File to which the output is written.
	   This is set to stdout by default if no output file
	   is specified.
	   Default is set to stdout.
	   @see m0_cs_init()
	 */
#ifndef __KERNEL__
	FILE                       *cc_outfile;
#endif
	/**
	 * List of buffer pools in motr context.
	 * @see cs_buffer_pool::cs_bp_linkage
	 */
	struct m0_tl                cc_buffer_pools;

	/**
	 * Minimum number of buffers in TM receive queue.
	 * @see m0_net_transfer_mc:ntm_recv_queue_length
	 * Default is set to M0_NET_TM_RECV_QUEUE_DEF_LEN.
	 */
	size_t                      cc_recv_queue_min_length;

	size_t                      cc_max_rpc_msg_size;

	/** "stats" service endpoint. */
	struct cs_endpoint_and_xprt cc_stats_svc_epx;

	/** List of ioservice end points. */
	struct m0_tl                cc_ios_eps;

	/** List of mdservice end points. */
	struct m0_tl                cc_mds_eps;

	uint32_t                    cc_pool_width;

	struct m0_motr_ha           cc_motr_ha;
	bool                        cc_ha_is_started;
	char                       *cc_ha_addr; /**< HA endpoint address     */

	/** Run as a daemon? */
	bool                        cc_daemon;

	/** Run from mkfs? */
	bool                        cc_mkfs;

        /** Force to override found filesystem during mkfs. */
	bool                        cc_force;

	/** Skip BE initialization when unneeded (like in m0rpcping).*/
	bool                        cc_no_storage;

	/** Skip Conf initialization when unneeded (like in m0rpcping).*/
	bool                        cc_no_conf;

	/** Skip all-to-all connection init. Useful in dummy HA */
	bool                        cc_no_all2all_connections;

	/** Enables fault injection during m0d bootup. */
	bool                        cc_enable_finj;

	/** Command line arguments. */
	struct cs_args		    cc_args;

	/** Number of buffers in incoming/outgoing copy machine pools. */
	m0_bcount_t                 cc_sns_buf_nr;

	/**
	 * Used for step-by-step initialisation and finalisation in
	 * m0_cs_init(), m0_cs_setup_env(), m0_cs_start(), m0_cs_fini().
	 */
	struct m0_module            cc_module;

	/**
	 * argc/argv, passed to m0_cs_setup_env().
	 * Not the same as ca_argc/ca_argv in cc_args.
	 */
	int                         cc_setup_env_argc;
	/** @see cc_setup_env_argc */
	char                      **cc_setup_env_argv;

	/** Magic for m0_bob_type */
	uint64_t                    cc_magic;

	/** Is used only during m0_cs_start(). */
	struct m0_conf_root        *cc_conf_root;

	/**
	 * XXX Some strange mode.
	 * TODO eliminate it if (and when) possible.
	 */
	bool                        cc_skip_pools_and_ha_update;

	/** XXX A kludge for finalisation purposes. */
	bool                        cc_ha_was_started;
};

enum {
	CS_MAX_EP_ADDR_LEN = 86 /* "lnet:" + M0_NET_LNET_XEP_ADDR_LEN */
};
M0_BASSERT(CS_MAX_EP_ADDR_LEN >= sizeof "lnet:" + M0_NET_LNET_XEP_ADDR_LEN);

struct cs_ad_stob {
	/** Allocation data storage domain.*/
	struct m0_stob_domain *as_dom;
	/** Back end storage object. */
	struct m0_stob        *as_stob_back;
	uint64_t               as_magix;
	struct m0_tlink        as_linkage;
};

/**
   Initialises motr context.

   @param cs_motr Represents a motr context
   @param xprts Array or network transports supported in a motr context
   @param xprts_nr Size of xprts array
   @param out File descriptor to which output is written
   @param should the storage be prepared just like mkfs does?
 */
#ifndef __KERNEL__
int m0_cs_init(struct m0_motr *cs_motr,
	       struct m0_net_xprt **xprts, size_t xprts_nr, FILE *out, bool mkfs);
#endif
/**
   Finalises motr context.
 */
void m0_cs_fini(struct m0_motr *cs_motr);

/**
   Configures motr context before starting the services.
   Parses the given arguments and allocates request handler contexts.
   Validates allocated request handler contexts which includes validation
   of given arguments and their values.
   Once all the arguments are validated, initialises network domains, creates
   and initialises request handler contexts, configures rpc machines each per
   request handler end point.

   @param cs_motr Motr context to be initialised
 */
int m0_cs_setup_env(struct m0_motr *cs_motr, int argc, char **argv);

/**
   Starts all the specified services in the motr context.
   Only once the motr environment is configured with network domains,
   request handlers and rpc machines, specified services are started.

   @param cs_motr Motr context in which services are started
 */
int m0_cs_start(struct m0_motr *cs_motr);

M0_INTERNAL struct m0_rpc_machine *m0_motr_to_rmach(struct m0_motr *motr);

M0_INTERNAL struct m0_confc *m0_motr2confc(struct m0_motr *motr);

/**
 * Accesses the request handler.
 *
 * @note Returned pointer is never NULL.
 */
struct m0_reqh *m0_cs_reqh_get(struct m0_motr *cctx);

/**
 * Returns instance of struct m0_motr given a
 * request handler instance.
 * @pre reqh != NULL.
 */
M0_INTERNAL struct m0_motr *m0_cs_ctx_get(struct m0_reqh *reqh);

/**
 * Returns instance of struct m0_reqh_context given a
 * request handler instance.
 * @pre reqh != NULL.
 */
M0_INTERNAL struct m0_reqh_context *m0_cs_reqh_context(struct m0_reqh *reqh);

/**
 * Returns m0_storage_devs object from m0 instance. Returns NULL if
 * the object is not initialised.
 */
M0_INTERNAL struct m0_storage_devs *m0_cs_storage_devs_get(void);

/**
 * Finds network domain for specified network transport in a given motr
 * context.
 *
 * @pre cctx != NULL && xprt_name != NULL
 */
M0_INTERNAL struct m0_net_domain *m0_cs_net_domain_locate(struct m0_motr *cctx,
							  const char *xprtname);

/**
 * Extract network layer endpoint and network transport from end point string.
 *
 * @pre ep != NULL
 */
M0_INTERNAL int m0_ep_and_xprt_extract(struct cs_endpoint_and_xprt *epx,
				       const char *ep);

/**
 * Finalise previously extracted endpoint and network transport in
 * m0_ep_and_xprt_extract.
 */
M0_INTERNAL void m0_ep_and_xprt_fini(struct cs_endpoint_and_xprt *epx);

M0_TL_DESCR_DECLARE(cs_eps, extern);
M0_TL_DECLARE(cs_eps, M0_INTERNAL, struct cs_endpoint_and_xprt);
M0_BOB_DECLARE(M0_INTERNAL, cs_endpoint_and_xprt);

/**
 * Extract the path of the provided dev_id from the config file, create stob id
 * for it and call m0_stob_linux_reopen() to reopen the stob.
 */
M0_INTERNAL int m0_motr_stob_reopen(struct m0_reqh *reqh,
				    struct m0_poolmach *pm,
				    uint32_t dev_id);

/** @} endgroup m0d */

#endif /* __MOTR_MOTR_MOTR_SETUP_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
