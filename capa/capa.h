/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_CAPA_CAPA_H__
#define __MOTR_CAPA_CAPA_H__

#include "lib/atomic.h"
#include "net/net.h"

/**
   @defgroup capa Motr Capability

Motr Capabilities are an implementation of Capability-based security as
described here:
http://en.wikipedia.org/wiki/Capability-based_security

The idea is that an authority managing some object (e.g., a lock, a file, a
layout, etc., basically, a resource), issues a capability together with this
object. Other parties can verify that a capability was issued by the authority
but cannot forge capabilities. A typical use case is that a client receives a
capability attached to some piece of file system state and then forwards the
capability together with the state to another node. For example, a capability
attached to a fid and sent back to the server which produced the fid and the
capability, can be used to deal with fid-guessing attack. Capabilities can be
forwarded to the nodes different from ones where they originated.

Capability HLD :
For documentation links, please refer to this file :
doc/motr-design-doc-list.rst

   @{
 */

/**
   Capability Protected Entity Type
*/
enum m0_capa_entity_type {
	M0_CAPA_ENTITY_OBJECT,
	M0_CAPA_ENTITY_LOCKS,
	M0_CAPA_ENTITY_LAYOUT,
};

/**
   Capability Operations
*/
enum m0_capa_operation {
	M0_CAPA_OP_DATA_READ,
	M0_CAPA_OP_DATA_WRITE,
};

enum {
	M0_CAPA_HMAC_MAX_LEN = 64
};

/**
   Capability issuer.
   @todo Use proper capability issuer
*/

struct m0_capa_issuer {

};

struct m0_capa_ctxt;
/**
   Motr Object Capability
*/
struct m0_object_capa {
	/** the context in which this capability is issued. */
	struct m0_capa_ctxt     *oc_ctxt;
	/** an authority who issues the capability */
	struct m0_capa_issuer   *oc_owner;
	enum m0_capa_entity_type oc_type;
	enum m0_capa_operation   oc_opcode;
	struct m0_atomic64       oc_ref;

	/** an entity protectd by this capability. Data type depends on the
	    type and operation.
	*/
	void 		        *oc_data;
	char			 oc_opaque[M0_CAPA_HMAC_MAX_LEN];
};

/**
   Motr Capability Context

   This is the context in which the capability credentials are issued,
   authorized, checked, etc.
*/
struct m0_capa_ctxt {
	/** more fields go here */
};

/**
   Init a Motr Capability Context

   @param ctxt the execution context
   @return 0 means success. Otherwise failure.
*/
M0_INTERNAL int m0_capa_ctxt_init(struct m0_capa_ctxt *ctxt);

/**
   Fini a Motr Capability Context

   @param ctxt the execution context
*/
M0_INTERNAL void m0_capa_ctxt_fini(struct m0_capa_ctxt *ctxt);

/**
   New Capability for an object for specified operation

   @param capa [in][out]result will be stored here.
   @param type [in] type of the capability.
   @param opcode [in] operation code.
   @param data [in] opaque object that this capability protects.
   @return 0 means success. Otherwise failure.

   Reference count will be initialzed to zero.
*/
M0_INTERNAL int m0_capa_new(struct m0_object_capa *capa,
			    enum m0_capa_entity_type type,
			    enum m0_capa_operation opcode, void *data);

/**
   Get Capability for an object for specified operation

   @param ctxt [in]the execution context.
   @param owner [in] owner of this capa.
   @param capa [in][out]result will be stored here.
   @return 0 means success. Otherwise failure.

   @pre m0_capa_new() should be called successfully.
   Reference count will be bumped.
*/
M0_INTERNAL int m0_capa_get(struct m0_capa_ctxt *ctxt,
			    struct m0_capa_issuer *owner,
			    struct m0_object_capa *capa);

/*
   Put Capability for an object

   @param ctxt [in]the execution context.
   @param capa [in]capa to be released.

   Reference count will be decreased. When reference count drops to zero,
   it will be finalized and can not be used any more.
*/
M0_INTERNAL void m0_capa_put(struct m0_capa_ctxt *ctxt,
			     struct m0_object_capa *capa);


/**
   Authenticate an operation

   @param ctxt [in]the execution context.
   @param capa [in]capability to be authenticated.
   @param op [in] target operation.
   @return 0 means permission is granted. -EPERM means access denied, and
           others mean error.
*/
M0_INTERNAL int m0_capa_auth(struct m0_capa_ctxt *ctxt,
			     struct m0_object_capa *capa,
			     enum m0_capa_operation op);


/** @} end group capa */

/* __MOTR_CAPA_CAPA_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
