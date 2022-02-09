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
#ifndef __MOTR_CONF_PVERS_H__
#define __MOTR_CONF_PVERS_H__

#include "conf/obj.h"   /* m0_conf_pver_kind */
#include "conf/confc.h" /* m0_confc */

/**
 * @defgroup conf-pvers
 *
 * There are 3 kinds of m0_conf_pver objects: actual, formulaic, virtual.
 * Of those only actual and formulaic pvers are 1) linked to the DAG of
 * conf objects, 2) specified in configuration strings (conf.xc files),
 * and 3) exist in memory of confd service.
 *
 * When HA state of a device[1] changes, flset_hw_obj_failure_cb() updates
 * recd[2] vectors of the actual pvers, whose objvs refer to this device.
 *
 *   [1] Here "device" is m0_conf_{rack,enclosure,controller,disk},
 *       i.e., an object which m0_conf_objv::cv_real may refer to.
 *
 *   [2] "recd" is acronym of "racks, enclosures, controllers, disks".
 *
 * The attributes of an actual pver:
 *   - pvs_recd[]: number of failed (i.e., non-M0_NC_ONLINE) devices
 *     at each level of this pver's subtree;
 *   - pvs_sitevs;
 *   - pvs_attr;
 *   - pvs_tolerance[]: this attribute is used by failure domains code
 *     (fd/fd.c) and is opaque for conf code.
 *
 * Formulaic pvers are "leaves" of conf DAG --- they do not have a subtree
 * of objvs growing from them.  The attributes of a formulaic pver are:
 *   - pvf_id: cluster-unique identifier;
 *   - pvf_base: reference to the actual pver, the subtree of which is used
 *     to create/restore virtual pver subtree;
 *   - pvf_allowance[]: number of non-M0_NC_ONLINE objects at each level of
 *     base pver subtree.
 *
 * When configuration consumer needs a pool version, it calls
 * m0_conf_pver_find(), passing m0_conf_pool as an argument. The function
 * searches for a "clean" pver among m0_conf_pool::pl_pvers ---
 * the one for which m0_conf_pver_is_clean() returns true.
 *
 * An actual pver is clean iff all its objvs refer to M0_NC_ONLINE objects.
 *
 * A formulaic pver is clean iff the number of failed devices at level L
 * of the base pver's subtree is equal to L-th element of the allowance
 * vector, where L = 0..M0_CONF_PVER_LVL_DRIVES.
 *
 * Clean actual pver is returned (by m0_conf_pver_find()) to user as is.
 * Clean formulaic pver is used to find or create virtual pver;
 * see conf_pver_formulate().
 *
 * Virtual pver resembles actual pver in that it also has a subtree of objvs
 * growing from it.  Differences:
 *   - virtual pver is not linked to the DAG of conf objects (i.e., its
 *     m0_conf_obj::co_parent is NULL);
 *   - virtual pver and its children (m0_conf_dirs and m0_conf_objvs)
 *     exist in local conf cache only.  They are never transferred over
 *     network, are not represented in conf strings, do not exist in confd's
 *     conf cache.
 *
 * Fid of virtual pver contains all the information necessary to
 * restore virtual pver subtree (e.g., in case of node restart).
 * m0_conf_pver_find_by_fid() finds or re-creates virtual pool version
 * by its fid.
 *
 * To generate or parse m0_conf_pver fid, use m0_conf_pver_fid() and
 * m0_conf_pver_fid_read(), correspondingly.
 *
 * <hr> <!----------------------------------------------------------->
 * @section conf-pvers-assume Design Assumptions
 *
 * m0_conf_pver_find() assumes that all conf objects of the subtree,
 * originating at `pool' argument, exist in the conf cache and are
 * M0_CS_READY.
 *
 * m0_conf_pver_find_by_fid() assumes M0_CS_READY-ness of all conf objects,
 * whose type is not m0_conf_{node,process,service,sdev}.
 *
 * <hr> <!----------------------------------------------------------->
 * @section conf-pvers-refs References
 *
 * @see doc/formulaic-pvers.org
 *
 * @{
 */

/**
 * State is determined by counting the number of failures in the cluster.
 */
enum m0_conf_pver_state {
	M0_CPS_HEALTHY,      /* Failures == 0 */
	M0_CPS_DEGRADED,     /* Failures < K */
	M0_CPS_CRITICAL,     /* Failures == K */
	M0_CPS_DAMAGED,      /* Failures > K */
	M0_CPS_NR
};

/**
 * Contains current state of the pool version according to the number of
 * failures in the pool version and some attributes describing the pool version.
 */
struct m0_conf_pver_info {
	struct m0_fid            cpi_fid;

        /** Layout attributes associated with this pool version. */
        struct m0_pdclust_attr   cpi_attr;

	/** State of the pool version determined by number of failures */
	enum m0_conf_pver_state  cpi_state;
};

/**
 * Returns the status of the pool version according to number of failed srecd
 * objects in the pool version.
 *
 * @param  fid      fid of the pool version whose status is queried.
 * @param  out_info out parameter which will contain status along with 
 *                  some pdclust attributes of pool version.
 * @param  confc    configuration client for accessing the conf root.
 * @pre    fid != NULL
 * @pre    confc != NULL
 * @return rc of the function.
 */
int m0_conf_pver_status(struct m0_fid *fid,
			struct m0_confc *confc,
			struct m0_conf_pver_info *out_info);

/**
 * Returns a pool version with online elements only.
 *
 * If no such pool version exists, m0_conf_pver_find() will create
 * virtual pool version subtree.
 */
M0_INTERNAL int m0_conf_pver_find(const struct m0_conf_pool *pool,
				  const struct m0_fid *pver_to_skip,
				  struct m0_conf_pver **out);

/**
 * Finds or creates (restores) a pool version by its fid.
 *
 * @pre  m0_conf_fid_type(fid) == &M0_CONF_PVER_TYPE
 *
 * @note  m0_conf_pver_find_by_fid() does not pin `out' object
 *        (i.e., it does not increment m0_conf_obj::co_nrefs).
 *        XXX Isn't this a bug?
 */
M0_INTERNAL int m0_conf_pver_find_by_fid(const struct m0_fid *fid,
					 const struct m0_conf_root *root,
					 struct m0_conf_pver **out);

/** Finds formulaic pver by its m0_conf_pver_formulaic::pvf_id attribute. */
M0_INTERNAL int m0_conf_pver_formulaic_find(uint32_t fpver_id,
					    const struct m0_conf_root *root,
					    const struct m0_conf_pver **out);

/** Finds formulaic pool version which `virtual' was "formulated" from. */
M0_INTERNAL int
m0_conf_pver_formulaic_from_virtual(const struct m0_conf_pver *virtual,
				    const struct m0_conf_root *root,
				    const struct m0_conf_pver **out);

/**
 * Returns true iff pver consists of online elements only or can be
 * used to generate such a pver.
 *
 * @pre  pver is not virtual
 */
M0_INTERNAL bool m0_conf_pver_is_clean(const struct m0_conf_pver *pver);

/**
 * Returns m0_conf_pver fid.
 *
 * Interpretation of `container' and `key' arguments in case of virtual pver
 * (kind = M0_CONF_PVER_VIRTUAL):
 *   container = formulaic pver id;
 *   key = failure cid (the index of combination of failed devices in
 *         the ordered sequence of pver's devices).
 */
M0_INTERNAL struct m0_fid m0_conf_pver_fid(enum m0_conf_pver_kind kind,
					   uint64_t container, uint64_t key);

/**
 * Parses pver fid.
 *
 * Gets `kind', `container', `key' components.  Any of the output
 * pointers may be NULL.
 *
 * @see m0_conf_pver_fid() for the meaning of `container' and `key'
 *      in case of virtual pver.
 */
M0_INTERNAL int m0_conf_pver_fid_read(const struct m0_fid *fid,
				      enum m0_conf_pver_kind *kind,
				      uint64_t *container, uint64_t *key);

/**
 * Which level of m0_conf_pver_subtree does given object correspond to?
 *
 * @pre  Type of `obj' is one of m0_conf_{rack,enclosure,controller,disk},
 *       or m0_conf_objv.
 * @post M0_CONF_PVER_LVL_RACKS <= retval && retval < M0_CONF_PVER_HEIGHT
 */
M0_INTERNAL unsigned m0_conf_pver_level(const struct m0_conf_obj *obj);

/** @} conf-pvers */
#endif /* __MOTR_CONF_PVERS_H__ */
