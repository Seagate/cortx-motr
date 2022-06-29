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

#ifndef __MOTR_HA_HA_H__
#define __MOTR_HA_HA_H__

/**
 * @defgroup design
 *
 * Groundwork
 * ----------
 *
 * Every piece of hardware used by Motr is a part of Motr configuration, i.e.
 * conf ojbect. Proccess, service and others are conf objects too.
 * HA task is to support the hierarcy and notify Motr about the changes in that
 * hierarcy.
 * HA communicates with the system admin.
 * HA has sensors and actuators.
 * Sensors -- a way to perceive the informnation from the outside world.
 * It could be software or human; systemd, k8s and so on.
 * Actuator -- a way to affer the outside world. It includes restarts of
 * processes/pods/etc, communication with sys admin.
 * Sensors for Motr: IO error, RPC error, etc.
 * Actuators for Motr: SNS trigger and so on.
 *
 * What HA does? it makes decision about every conf object in the cluster.
 * Why HA does it? its purpose is to bring the cluster to the state desired
 * by the system administrator. Something has to be powered on? it powers it on.
 * Something has to be recovered? HA initiates recovery and ensures that the
 * thing has been recovered.
 *
 * Decision-making process
 * -----------------------
 *
 * If HA knows that the cluster is not in the desired state then HA decides
 * what is the next step to bring the cluster to the desired state and
 * what actions have to be done and what states have to be changed.
 *
 * The set of actions and state changes make an epoch. The state changes
 * are sent to Motr, the actions are initiated by the HA. Then the HA makes
 * decision about the next step. To decide what is the next step, the HA takes
 * into account the information about the current state and the infromation
 * taken from the sensors.
 *
 * State updates are sent to Motr in form of HA messages. Each message consists
 * of the list of pairs (FID, state) for every configuration object which state
 * has been changed. Each message also has "epoch" that describes the
 * corresponding epoch. Epochs are monotonically increasing.
 *
 * HA keeps those decisions in the decision log. After the process is added
 * to the configuration HA starts keeping track of what messages it sends to
 * the process, and for every message the process could send reply indicating
 * that the particular epoch is no longer needed.
 *
 * If all the processes sent this "no-longer-needed" message for a particular
 * epoch and the epochs "before" then the decision log could be pruned for
 * this message and the "previous" messages in the log.
 *
 * Delivery of state changes
 * -------------------------
 *
 * Each conf object has state machine defined. HA moves the state of a conf
 * object as per the definition of the corresponding machine.
 * Inital state -- TRANSIENT. During bootstrap every object has T state.
 * Semantic of T state: we MUST NOT interact with that object but we should
 * keep in mind that we may need to interact with that object in future.
 * Semantic of O state: if there is a need to interact with the entity that
 * the conf object represents then the inteaction MUST be continued until
 * the object goes out of the ONLINE state.
 * Semantic of F state: it means that there MUST be no interaction with
 * the entity that the object represents. FAILED is a final state.
 *
 *
 * SNS/DIX repair/rebalance/direct-rebalance workflow
 * --------------------------------------------------
 *
 * Simple case: sdev in a pool goes to F state. HA initiates SNS repair for
 * this storage device. HA waits for the storage device to be replaces, and
 * after it is replaces and SNS repair is complete, HA initiates SNS rebalance.
 * Storage devices have REPAIRING and REBALANCING states. REPAIRING means
 * there is neither read nor write availability, REBALANCING means there is
 * write availability but there is no read availability.
 * MKFS means the lack of read and write availability.
 *
 * States: O, T, F, RB, RP, MKFS.
 *
 * State  Availability
 * T      x
 * O      READ and WRITE
 * F      x
 * MKFS   x
 * RP     x
 * RB     WRITE
 *
 * Transtions:
 *   O -> T: transient failure;
 *   T -> RP: HA makes decision to start SNS/DIX repair;
 *   RP -> O: HA makes decision that the device functions normaly and the spare
 *   units created during RP could be cleaned up.
 *   RP -> T: Repair is finished
 *   RP -> T -> F: HA makes decision that this storage device has failed
 *   permanently, and all data stored there is not going to be available at all;
 *   T -> MKFS: mkfs for the new device (after replacement);
 *   MKFS -> RB: HA initiates SNS/DIX rebalance.
 *   RB -> O: rebalance is complete.
 *
 * Sdev state transtions during process restart:
 *   O -> T -> O;
 *   RP -> T -> RP;
 *   RB -> T -> RB;
 *   F;
 *
 * Sdev state transitions during bootstrap:
 *   1. Separate MKFS phase:
 *      T -> MKFS -> T -> O (failure-free case);
 *            m0mkfs      m0d
 *   2. No separate MKFS phase:
 *      T -> MKFS -> O;
 *
 * Sdev state transitions when process starts:
 *   Before initiating startup of the process, HA moves the coresponding objects
 *   to the desired states:
 *	OS process: HA starts the process;
 *	process -> HA: connects (RPC, halink);
 *	process -> HA: entrypoint request
 *	IF process IN conf: process state on HA:
 *	  - then: T -> O
 *	  - else: add to conf, T, T -> O
 *	  Broadcast the state changes (as usual)
 *	HA -> process: ennrypoint reply;
 *	process -> HA: process starting;
 *	for each service:
 *		process -> HA: service starting;
 *		process -> HA: service started;
 *		service states on HA: T -> O (broadcasted by HA);
 *	endfor
 *	process -> HA: process started;
 *   Storage device state is not related to the process state: one device
 *   may be related more than one process. Because of that, we cannot
 *   unconditionally mark sdev as T when the process is T.
 *   The same thing with the F state: when process is F then it does not mean
 *   that sdev is F because the sdev may be "attached" to another process;
 *
 *  States when process stops:
 *    process -> HA: process stopping;
 *    for each service:
 *       service -> HA: stopping
 *       service state on HA: * -> T;
 *       service -> HA: stopped
 *    endfor
 *    process -> HA: process stopped;
 *    process state on HA: * -> T;
 *
 *
 * @section tbd
 * How mkfs, dtm recovery, SNS/DIX repair/rebalance/direct rebalance is
 * triggered?
 *
 * HA sets storage device state which defines the desired operation.
 * Motr starts the operation.
 * Motr reports process with the interval defined by HA in the entrypoint
 * reply.
 * The report is formed as "X of Y is done". The last such message is "Y of Y
 * is done" which means the operations has been succesfully finished by Motr.
 * X and Y are monotonically non-decreasing, X <= Y. The first message is "0 of
 * Y" and it means that the operation has started.
 * Currently we send failvec as a separate message. With epochs implemented,
 * failvec can be determined using HA state history and stored permanently by
 * the process, thus eliminating the need to send failvec separately.
 * TODO: add BE recovery to this list if it takes too much time.
 *
 * @secion tbd
 * TODO conf object out of make repair/rebalance/direct rebalance
 *
 * Sdev states: W, R, F, T, O.
 * RP/RB/DRB: separate conf object;
 * Its states: T (initial), RP, RB, DRB.
 * Transtions for that kind conf objects:
 *   T -> *: initiate repair;
 *   O -> T: transient failure;
 *   T -> RP: HA makes decision to start SNS/DIX repair;
 *   RP -> O: HA makes decision that the device functions normaly and the spare
 *   units created during RP could be cleaned up.
 *   RP -> T: Repair is finished
 *   RP -> T -> F: HA makes decision that this storage device has failed
 *   permanently, and all data stored there is not going to be available at all;
 *   T -> MKFS: mkfs for the new device (after replacement);
 *   MKFS -> RB: HA initiates SNS/DIX rebalance.
 *   RB -> O: rebalance is complete.
 * MKFS as a command: MKFS has no meaning for the external obervers
 * (the device is considered to be in T state). Because of that, it is enough
 * to send MKFS as a command from HA. Then, when mkfs is done, Motr sends back
 * to HA a notification about that. While mkfs is in progress, Motr sends
 * "keep-avalive" messages that contain the status of the operation completion
 * (x of y is done).
 *
 * DTM recovery
 * ------------
 *
 * It is launched automatically.
 *
 *
 * @secion tbd
 * Configuration objects outside of processes
 * When such conf object enters T then the sub-tree goes T.
 * When such conf object enters F then the sub-tree goes to T and some items
 * may go to F (similiar to the sdev case).
 *
 *
 * @defgroup ha
 *
 * - TODO 2 cases TRANSIENT -> ONLINE: if m0d reconnected or it's restarted
 * - TODO can m0d receive TRANSIENT or FAILED about itself
 * - TODO race between FAILED about process and systemd restart
 * - TODO check types of process fid
 *
 * State transition rules
 *
 * I. Processes and Services
 *
 * I.a Process and Service States
 * 1. There are 3 process/service states: FAILED, TRANSIENT and ONLINE.
 * 2. The only allowed state transitions for processes/services are
 *    FAILED <-> TRANSIENT <-> ONLINE, ONLINE -> FAILED.
 * 3. At the start every process/service is in FAILED state.
 * 4. When process/service starts it's moved to TRANSIENT state and then to
 *    ONLINE state.
 * 5. When there are signs that process/service is not ONLINE then it's moved to
 *    TRANSIENT state.
 * 6. When the decision is made that process/service is dead it's moved to
 *    FAILED state.
 * 7. When a process is permanently removed from the cluster it's going to
 *    FAILED state before it's removed from the configuration.
 *
 * I.b Events
 * 1. Processes and services send event messages when they are starting and
 *    stopping.
 * 2. Before process/service is started it sends STARTING event.
 * 3. After process/service is started it sends STARTED event.
 * 4. Before process/service initiates stop sequence it sends STOPPING event.
 * 5. After process/service initiates stop sequence it sends STOPPED event.
 * 6. Events for the same process/service are always sent in the same order:
 *    STARTING -> STARTED -> STOPPING -> STOPPED.
 *
 * I.c Correlation between events and states
 *
 * 1. Before process/service is started it should be in FAILED state.
 * 2. If process/service dies it's moved to FAILED state.
 * 3. After <...> event is sent the process/service is moved to <...> state:
 *    STARTING - TRANSIENT
 *    STARTED  - ONLINE
 *    STOPPING - TRANSIENT
 *    STOPPED  - FAILED.
 *
 * I.d Use cases
 * 1. Process/service start
 *            STARTING              STARTED
 *    FAILED ----------> TRANSIENT --------> ONLINE
 * 2. Process/service stop
 *               STOPPING             STOPPED
 *    ONLINE ------------> TRANSIENT ----------> FAILED
 * 3. Process/service crash
 *            the decision has been made
 *            that the process crashed
 *    ONLINE ----------------------------> FAILED
 * 4. Temporary process/service timeout
 *             decision that process             decision that process
 *                 may be dead                         is alive
 *    ONLINE -----------------------> TRANSIENT -----------------------> ONLINE
 * 5. Permanent process/servise timeout
 *             decision that process             decision that process
 *                 may be dead                          is dead
 *    ONLINE -----------------------> TRANSIENT -----------------------> FAILED
 *
 * I.e Handling process/service state transitions in rpc
 * 1. If a process/service is ONLINE the connect timeout is M0_TIME_NEVER and
 *    the number of resends is unlimited.
 * 2. If a process/service is in TRANSIENT state the connect timeout and the
 *    number of resends are limited.
 * 3. If a process/service is in FAILED state then no attempt is made to
 *    communicate with the process/service and all existing rpc
 *    sessions/connections are dropped without timeout.
 *
 * Reconnect protocol
 *
 * - Legend
 *   - HA1 - side with outgoging link (m0_ha with HA link from connect());
 *   - HA2 - side with incoming link (m0_ha with HA link from entrypoint
 *     server);
 *   - Cx - case #x
 *   - Sx - step #x
 *   - CxSy - case #x, step #y
 * - C1 HA2 is started, HA1 starts (first start after HA2 had started):
 *   - S1 HA1 sends entrypoint request with first_request flag set
 *   - S2 HA2 receives the request
 *   - S3 HA2 looks at the flag and makes new HA link
 *   - S4 HA2 sends entrypoint reply
 *   - S5 HA1 makes new link with the parameters from reply
 * - C2 HA1 and HA2 restart
 *   - the same as C1
 * - C3 HA1 restarts, HA2 is alive
 *   - the same as C1. Existing link is totally ignored in C1S3
 * - C4 HA2 restarts, HA1 is alive
 *   - S1 HA1 considers HA2 dead
 *   - S2 HA1 starts HA link reconnect
 *   - S3 HA1 tries to send entrypoint request to HA2 in infinite loop
 *   - S4 HA1 sends entrypoint request
 *   - S5 HA2 receives entrypoint request
 *   - S7 HA2 sees first_request flag is not set
 *   - S8 HA2 makes HA link with parameters from request
 *   - S9 HA2 sends entrypoint reply
 *   - S10 HA1 ends HA link reconnect
 * - C5 HA1 is alive, HA2 is alive, HA2 considers HA1 dead due to transient
 *   network failure
 *   - S1 C4S1, C4S2, C4S3
 *   - S2 HA2 terminates the process with HA1
 *   - S3 HA2 ensures that process is terminated
 *   - S4 C3
 *
 * Source structure
 * - ha/entrypoint_fops.h - entrypoint fops + req <-> req_fop, rep <-> rep_fop
 * - ha/entrypoint.h      - entrypoint client & server (transport)
 *
 * entrypoint request handling
 * - func Motr send entrypoint request
 * - cb   HA   accept entrypoint request
 * - func HA   reply to the entrypoint request
 * - cb   Motr receive entrypoint reply
 *
 * m0_ha_link management
 * - func Motr connect to HA
 * - cb   HA   accept incoming connection
 * - func Motr disconnect from HA
 * - cb   HA   node disconnected
 * - func HA   disconnect from Motr
 * - cb   HA   node is dead
 *
 * m0_ha_msg handling
 * - func both send msg
 * - cb   both recv msg
 * - func both msg delivered
 * - cb   both msg is delivered
 * - cb   HA   undelivered msg
 *
 * error handling
 * - cb   both ENOMEM
 * - cb   both connection failed
 *
 * use cases
 * - normal loop
 *   - Motr entrypoint request
 *   - HA   accept request, send reply
 *   - HA   reply to the request
 *   - Motr receive entrypoint reply
 *   - Motr connect to HA
 *   - HA   accept incoming connection
 *   - main loop
 *     - both send msg
 *     - both recv msg
 *     - both msg delivered
 *   - Motr disconnect from HA
 *   - HA   node disconnected
 * - HA restart
 *   - Motr entrypoint request, reply
 *   - Motr connect to HA
 *   - HA   accept incoming connection
 *   - < HA restarts >
 *   - < Motr considers HA dead >
 *   - Motr disconnect from HA
 *   - goto normal loop (but the same incarnation in the entrypoint request)
 * - Motr restart
 *   - Motr entrypoint request, reply
 *   - Motr connect to HA
 *   - HA   accept incoming connection
 *   - < Motr restarts >
 *   - < HA considers Motr dead >
 *   - HA   send msg to local link that Motr is dead
 *   - HA   receive msg from local link that Motr is dead
 *   - HA   (cb) node is dead
 *   - HA   disconnect from Motr
 *   goto normal loop
 * @{
 */

#include "lib/tlist.h"          /* m0_tl */
#include "lib/types.h"          /* uint64_t */
#include "lib/mutex.h"          /* m0_mutex */

#include "fid/fid.h"            /* m0_fid */
#include "module/module.h"      /* m0_module */
#include "ha/entrypoint.h"      /* m0_ha_entrypoint_client */
#include "ha/cookie.h"          /* m0_ha_cookie */

struct m0_uint128;
struct m0_rpc_machine;
struct m0_reqh;
struct m0_ha;
struct m0_ha_msg;
struct m0_ha_link;
struct m0_ha_entrypoint_req;

enum m0_ha_level {
	M0_HA_LEVEL_ASSIGNS,
	M0_HA_LEVEL_ADDR_STRDUP,
	M0_HA_LEVEL_LINK_SERVICE,
	M0_HA_LEVEL_ENTRYPOINT_SERVER_INIT,
	M0_HA_LEVEL_ENTRYPOINT_CLIENT_INIT,
	M0_HA_LEVEL_INIT,
	M0_HA_LEVEL_ENTRYPOINT_SERVER_START,
	M0_HA_LEVEL_INCOMING_LINKS,
	M0_HA_LEVEL_START,
	M0_HA_LEVEL_LINK_CTX_ALLOC,
	M0_HA_LEVEL_LINK_CTX_INIT,
	M0_HA_LEVEL_ENTRYPOINT_CLIENT_START,
	M0_HA_LEVEL_ENTRYPOINT_CLIENT_WAIT,
	M0_HA_LEVEL_LINK_ASSIGN,
	M0_HA_LEVEL_CONNECT,
};

struct m0_ha_ops {
	void (*hao_entrypoint_request)
		(struct m0_ha                      *ha,
		 const struct m0_ha_entrypoint_req *req,
		 const struct m0_uint128           *req_id);
	void (*hao_entrypoint_replied)(struct m0_ha                *ha,
	                               struct m0_ha_entrypoint_rep *rep);
	void (*hao_msg_received)(struct m0_ha      *ha,
	                         struct m0_ha_link *hl,
	                         struct m0_ha_msg  *msg,
	                         uint64_t           tag);
	void (*hao_msg_is_delivered)(struct m0_ha      *ha,
	                             struct m0_ha_link *hl,
	                             uint64_t           tag);
	void (*hao_msg_is_not_delivered)(struct m0_ha      *ha,
	                                 struct m0_ha_link *hl,
	                                 uint64_t           tag);
	void (*hao_link_connected)(struct m0_ha            *ha,
	                           const struct m0_uint128 *req_id,
	                           struct m0_ha_link       *hl);
	void (*hao_link_reused)(struct m0_ha            *ha,
	                        const struct m0_uint128 *req_id,
	                        struct m0_ha_link       *hl);
	void (*hao_link_absent)(struct m0_ha            *ha,
	                        const struct m0_uint128 *req_id);
	void (*hao_link_is_disconnecting)(struct m0_ha      *ha,
	                                  struct m0_ha_link *hl);
	void (*hao_link_disconnected)(struct m0_ha      *ha,
	                              struct m0_ha_link *hl);
	/* not implemented yet */
	void (*hao_error_no_memory)(struct m0_ha *ha, int unused);
};

struct m0_ha_cfg {
	struct m0_ha_ops                    hcf_ops;
	struct m0_rpc_machine              *hcf_rpc_machine;
	struct m0_reqh                     *hcf_reqh;
	/** Remote address for m0_ha_connect(). */
	const char                         *hcf_addr;
	/** Fid of local process. */
	struct m0_fid                       hcf_process_fid;

	/* m0_ha is resposible for the next fields */

	struct m0_ha_entrypoint_client_cfg  hcf_entrypoint_client_cfg;
	struct m0_ha_entrypoint_server_cfg  hcf_entrypoint_server_cfg;
};

struct ha_link_ctx;

struct m0_ha {
	struct m0_ha_cfg                h_cfg;
	struct m0_module                h_module;
	struct m0_mutex                 h_lock;
	struct m0_tl                    h_links_incoming;
	struct m0_tl                    h_links_outgoing;
	/** Contains disconnecting incoming ha_links to avoid re-using. */
	struct m0_tl                    h_links_stopping;
	/** Primary outgoing link. */
	struct m0_ha_link              *h_link;
	/** Struct ha_link_ctx for h_link. */
	struct ha_link_ctx             *h_link_ctx;
	bool                            h_link_started;
	struct m0_reqh_service         *h_hl_service;
	struct m0_ha_entrypoint_client  h_entrypoint_client;
	struct m0_ha_entrypoint_server  h_entrypoint_server;
	struct m0_clink                 h_clink;
	uint64_t                        h_link_id_counter;
	uint64_t                        h_generation_counter;
	bool                            h_warn_local_link_disconnect;
	/*
	 * A cookie that belongs to this m0_ha.
	 * It's sent in the m0_ha_entrypoint_rep::hae_cookie_actual to every
	 * m0_ha that requests entrypoint from this one.
	 * It's compared with m0_ha_entrypoint_req::heq_cookie_expected to see
	 * if the requester expects exactly this instance of m0_ha.
	 */
	struct m0_ha_cookie             h_cookie_local;
	/*
	 * The expected cookie of the remote end of m0_ha::h_link.
	 * It's sent in m0_ha_entrypoint_req::heq_cookie_expected to tell about
	 * the expectations.
	 * It's compared with the cookie from the
	 * m0_ha_entrypoint_rep::hae_cookie_actual to detect remote restart.
	 */
	struct m0_ha_cookie             h_cookie_remote;
};

M0_INTERNAL int m0_ha_init(struct m0_ha *ha, struct m0_ha_cfg *ha_cfg);
M0_INTERNAL int m0_ha_start(struct m0_ha *ha);
M0_INTERNAL void m0_ha_stop(struct m0_ha *ha);
M0_INTERNAL void m0_ha_fini(struct m0_ha *ha);

M0_INTERNAL void
m0_ha_entrypoint_reply(struct m0_ha                       *ha,
                       const struct m0_uint128            *req_id,
                       const struct m0_ha_entrypoint_rep  *rep,
                       struct m0_ha_link                 **hl_ptr);

M0_INTERNAL struct m0_ha_link *m0_ha_connect(struct m0_ha *ha);
M0_INTERNAL void m0_ha_disconnect(struct m0_ha *ha);

M0_INTERNAL void m0_ha_disconnect_incoming(struct m0_ha      *ha,
                                           struct m0_ha_link *hl);

M0_INTERNAL void m0_ha_send(struct m0_ha           *ha,
                            struct m0_ha_link      *hl,
                            const struct m0_ha_msg *msg,
                            uint64_t               *tag);
M0_INTERNAL void m0_ha_delivered(struct m0_ha      *ha,
                                 struct m0_ha_link *hl,
                                 struct m0_ha_msg  *msg);

M0_INTERNAL void m0_ha_flush(struct m0_ha      *ha,
			     struct m0_ha_link *hl);

M0_INTERNAL void m0_ha_process_failed(struct m0_ha        *ha,
                                      const struct m0_fid *process_fid);

M0_INTERNAL struct m0_ha_link *m0_ha_outgoing_link(struct m0_ha *ha);
M0_INTERNAL struct m0_rpc_session *m0_ha_outgoing_session(struct m0_ha *ha);

M0_INTERNAL void m0_ha_rpc_endpoint(struct m0_ha      *ha,
                                    struct m0_ha_link *hl,
                                    char              *buf,
                                    m0_bcount_t        buf_len);

M0_INTERNAL int  m0_ha_mod_init(void);
M0_INTERNAL void m0_ha_mod_fini(void);

/** @} end of ha group */
#endif /* __MOTR_HA_HA_H__ */

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
