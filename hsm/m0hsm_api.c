/*
 * Copyright (c) 2018-2020 Seagate Technology LLC and/or its Affiliates
 * COPYRIGHT 2017-2018 CEA[1] and SAGE partners
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
 * [1]Commissariat a l'energie atomique et aux energies alternatives
 *
 * Original author: Thomas Leibovici <thomas.leibovici@cea.fr>
 */

/* HSM invariants:
 * - There is always a writable layer with the highest priority
 * to protect object copies (source or target) to be modified while they are moved.
 * - Applications always read the last written data from composite objects,
 *   whereever it extents are located.
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <getopt.h>
#include <stdarg.h>

#include "conf/obj.h"
#include "fid/fid.h"
#include "motr/idx.h"
#include "motr/layout.h"

#include "m0hsm_api.h"

#define MAX_BLOCK_COUNT (200)

/** batch create + set_layout operation? */

/** How many extents we get in a batch from the extent index (arbitrary) */
#define HSM_EXTENT_SCAN_BATCH	8

/* global variables (set by m0hsm_init())*/
struct m0hsm_options options = {
	.trace_level = LOG_INFO,
	.op_timeout = 10,
	.log_stream = NULL, /* default will be set by m0hsm_init() */
};

static struct m0_client *m0_instance;
static struct m0_realm  *m0_uber_realm;

/* logging macros */
#define ERROR(_fmt, ...) if (options.trace_level >= LOG_ERROR) \
			fprintf(options.log_stream, _fmt, ##__VA_ARGS__)
#define INFO(_fmt, ...)	 if (options.trace_level >= LOG_INFO) \
			fprintf(options.log_stream, _fmt, ##__VA_ARGS__)
#define VERB(_fmt, ...) if (options.trace_level >= LOG_VERB) \
			fprintf(options.log_stream, _fmt, ##__VA_ARGS__)
#define DBG(_fmt, ...) if (options.trace_level >= LOG_DEBUG) \
			fprintf(options.log_stream, _fmt, ##__VA_ARGS__)

#define ENTRY DBG("> ENTERING %s()\n", __func__)
#define RETURN(_rc) do { DBG("< LEAVING %s() line %d, rc=%d\n", \
			         __func__, __LINE__, (_rc)); \
			 return (_rc); } while(0)

#define ARRAY_SIZE(a) ((sizeof (a)) / (sizeof (a)[0]))

/** internal type to handle extents */
struct extent {
	off_t off;
	size_t len;
};

/** largest extent possible */
static const struct extent EXT_FULLRANGE = {
	.off = 0,
	.len = M0_BCOUNT_MAX,
};

enum {
	MAX_LEN=128,
	MAX_POOLS = 16,
};

struct param {
	char name[MAX_LEN];
	char value[MAX_LEN];
};

static struct param  hsm_rc_params[128];
static struct m0_fid hsm_pools[MAX_POOLS] = {};

static int read_params(FILE *in, struct param *p, int max_params)
{
	int ln, n=0;
	char s[MAX_LEN];

	for (ln=1; max_params > 0 && fgets(s, MAX_LEN, in); ln++) {
		if (sscanf(s, " %[#\n\r]", p->name))
			continue; /* skip emty line or comment */
		if (sscanf(s, " %[a-z_A-Z0-9] = %[^#\n\r]",
		           p->name, p->value) < 2) {
			ERROR("m0hsm: %s: error at line %d: %s\n", __func__,
			      ln, s);
			return -1;
		}
		DBG("%s: %d: name='%s' value='%s'\n", __func__,
		      ln, p->name, p->value);
		p++, max_params--, n++;
	}

	RETURN(n);
}

static int hsm_pool_fid_set(struct param *p)
{
	int i;
	char pname[32];

	for (i = 0; i < MAX_POOLS; i++) {
		sprintf(pname, "M0_POOL_TIER%d", i + 1);
		if (strcmp(p->name, pname) == 0) {
			if (m0_fid_sscanf(p->value, hsm_pools + i) != 0) {
				ERROR("%s: failed to parse FID of %s\n",
				      __func__, pname);
				return -1;
			}
			return 1;
		}
	}

	return 0;
}

static int hsm_pools_fids_set(struct param p[], int n)
{
	int i, rc;

	for (i = 0; n > 0; n--, p++, i += rc) {
		rc = hsm_pool_fid_set(p);
		DBG("%s: rc=%d\n", __func__, rc);
		if (rc < 0)
			return rc;
	}

	if (i < 1) {
		ERROR("m0hsm: no pools configured\n");
		return -1;
	}

	return 0;
}

int m0hsm_init(struct m0_client *instance, struct m0_realm *uber_realm,
	       const struct m0hsm_options *in_options)
{
	int rc;

	options.log_stream = stderr; /* set default */

	/* set options */
	if (in_options)
		options = *in_options;

	if (!instance || !uber_realm) {
		ERROR("Missing instance or realm argument to %s()\n", __func__);
		return -EINVAL;
	}

	m0_instance   = instance;
	m0_uber_realm = uber_realm;

	if ((rc = read_params(options.rcfile, hsm_rc_params,
			ARRAY_SIZE(hsm_rc_params))) < 0) {
		ERROR("%s: failed to read parameters\n", __func__);
		return -EINVAL;
	}

	DBG("%s: read %d params\n", __func__, rc);

	if (hsm_pools_fids_set(hsm_rc_params, rc) < 0) {
		ERROR("%s: failed to configure pools\n", __func__);
		return -EINVAL;
	}

	return 0;
}

/** Special value meaning any tier. */
#define HSM_ANY_TIER	UINT8_MAX

/* Keep high 32 bits reserved for motr. Use 2^31 for HSM */
#define HSM_ID_MASK_HI	(1LL<<31)
static bool is_hsm_reserved(struct m0_uint128 id)
{
	return (id.u_hi & HSM_ID_MASK_HI);
}

/* HSM priority is composed of:
 * 24 bits for generation, 8 bits for tier priority
 * Priority is in descending order (the smaller the value,
 * the higher priority), so we must reverse generation
 * numbering (from the higher value down to 0).
 */

/**
 * Build layer priority from generation number and tier number
 */
static uint32_t hsm_prio(uint32_t generation, uint8_t tier_idx)
{
	/* generation prio is the opposite of generation */
	uint32_t gen_prio;

	/* generation must fit on 24bits */
	M0_ASSERT(generation <= 0x00FFFFFF);

	gen_prio = 0x00FFFFFF - generation;

	return (gen_prio << 8) | tier_idx;
}

/**
 * Build subobject id from parent object id, generation number and tier number.
 */
static struct m0_uint128 hsm_subobj_id(struct m0_uint128 id, uint32_t gen,
				       uint8_t tier)
{
	struct m0_uint128 newid = id;

	newid.u_hi <<= 32;
	newid.u_hi |= HSM_ID_MASK_HI;
	newid.u_hi |= hsm_prio(gen, tier);

	return newid;
}

/**
 * Extract generation number from layer priority.
 */
static uint32_t hsm_prio2gen(uint32_t priority)
{
	return 0x00FFFFFF - (priority >> 8);
}

/**
 * Extract tier index from layer priority.
 */
static uint8_t hsm_prio2tier(uint32_t priority)
{
	return priority & 0xFF;
}


/**
 * Returns a pointer to a pool fid for the given tier index.
 */
static struct m0_fid *hsm_tier2pool(uint8_t tier_idx)
{
	if (tier_idx < 1 || tier_idx > MAX_POOLS)
		return NULL;
	return hsm_pools + tier_idx - 1;
}

/** Helper to open an object entity */
static int open_entity(struct m0_entity *entity)
{
	struct m0_op *ops[1] = {NULL};
	int rc;
	ENTRY;

	m0_entity_open(entity, &ops[0]);
	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					      M0_OS_STABLE),
			      m0_time_from_now(options.op_timeout,0)) ?:
	     m0_rc(ops[0]);
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);

	RETURN(rc);
}


/**
 * Helper to create an object with the given id.
 * @param tier_idx defines the object location
 * 		   (HSM_ANY_TIER to leave it unspecified)
 */
static int create_obj(struct m0_uint128 id, struct m0_obj *obj,
		      bool close_entity, uint8_t tier_idx)
{
	struct m0_op *ops[1] = {NULL};
	struct m0_fid *pool = NULL;
	int rc;
	ENTRY;

	DBG("creating id=%"PRIx64":%"PRIx64"\n", id.u_hi, id.u_lo);

	/* first create the main object with a default layout */
	m0_obj_init(obj, m0_uber_realm, &id, 9 /* XXX: 1MB */);
			  /* m0_client_layout_id(m0_instance)); */

	rc = open_entity(&obj->ob_entity);
	if (rc == 0) {
		ERROR("Object %"PRIx64":%"PRIx64" already exists\n", id.u_hi,
		      id.u_lo);
		RETURN(-EEXIST);
	} else if (rc != -ENOENT) {
		ERROR("Failed to create object %"PRIx64":%"PRIx64": rc=%d\n",
		      id.u_hi, id.u_lo, rc);
		RETURN(rc);
	}

	if (tier_idx != HSM_ANY_TIER) {
		pool = hsm_tier2pool(tier_idx);
		DBG("%s: got pool "FID_F"\n", __func__, FID_P(pool));
		if (pool == NULL || !m0_fid_is_set(pool)) {
			ERROR("m0hsm: pool index %d is not configured\n", tier_idx);
			return -EINVAL;
		}
	}
	m0_entity_create(pool, &obj->ob_entity, &ops[0]);

	m0_op_launch(ops, ARRAY_SIZE(ops));
	rc = m0_op_wait(
		ops[0], M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
		m0_time_from_now(options.op_timeout,0));

	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);

	if (close_entity)
		m0_entity_fini(&obj->ob_entity);

	RETURN(rc);
}

/**
 * Helper to delete an object.
 * (actually unused because we batch delete of subobject
 * + update of parent layout in a single operation).
 */
static int delete_obj(struct m0_uint128 id) __attribute__((unused));
static int delete_obj(struct m0_uint128 id)
{
	struct m0_op *ops[1] = {NULL};
	struct m0_obj obj;
	int rc;
	ENTRY;

	memset(&obj, 0, sizeof(struct m0_obj));

	DBG("deleting id=%"PRIx64":%"PRIx64"\n", id.u_hi, id.u_lo);

	m0_obj_init(&obj, m0_uber_realm, &id, m0_client_layout_id(m0_instance));
	m0_entity_delete(&obj.ob_entity, &ops[0]);

	m0_op_launch(ops, ARRAY_SIZE(ops));
	rc = m0_op_wait(
		ops[0], M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
		m0_time_from_now(options.op_timeout,0));

	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);

	m0_entity_fini(&obj.ob_entity);

	RETURN(rc);
}

/**
 * Create an object with the given layout.
 */
static int create_obj_with_layout(struct m0_uint128 id,
				  struct m0_obj *obj,
				  struct m0_client_layout *layout,
				  bool close_entity)
{
	struct m0_op *ops[2] = {NULL};
	int rc;
	ENTRY;

	DBG("creating id=%"PRIx64":%"PRIx64"\n", id.u_hi, id.u_lo);

	/* first create the main object with a default layout */
	m0_obj_init(obj, m0_uber_realm, &id, m0_client_layout_id(m0_instance));

	/* set first operation of batch */
	m0_entity_create(NULL, &obj->ob_entity, &ops[0]);

	/* set second operation of batch */
	rc = m0_client_layout_op(obj, M0_EO_LAYOUT_SET, layout, &ops[1]);
	if (rc) {
		m0_op_free(ops[0]);
		RETURN(rc);
	}

	/* launch them both */
	m0_op_launch(ops, ARRAY_SIZE(ops));
	/* wait for first to complete */
	rc = m0_op_wait(
		ops[0], M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
		m0_time_from_now(options.op_timeout,0)) ?: m0_rc(ops[0]);
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);

	/* wait for second to complete */
	rc = rc ?: m0_op_wait(
		ops[1], M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
		m0_time_from_now(options.op_timeout,0)) ?: m0_rc(ops[1]);
	m0_op_fini(ops[1]);
	m0_op_free(ops[1]);

	if (close_entity)
		m0_entity_fini(&obj->ob_entity);

	RETURN(rc);
}

/** Combine a subobject deletion + setting parent layout */
static int delete_obj_set_parent_layout(struct m0_uint128 id,
					struct m0_uint128 parent_id,
					struct m0_client_layout *parent_layout)
{
	struct m0_op *ops[2] = {NULL};
	struct m0_obj parent_obj;
	struct m0_obj obj;
	int rc;

	ENTRY;

	DBG("deleting id=%"PRIx64":%"PRIx64"\n", id.u_hi, id.u_lo);

	memset(&obj, 0, sizeof(struct m0_obj));
	memset(&parent_obj, 0, sizeof(struct m0_obj));

	m0_obj_init(&obj, m0_uber_realm, &id,
			   m0_client_layout_id(m0_instance));
	m0_obj_init(&parent_obj, m0_uber_realm, &parent_id,
			   m0_client_layout_id(m0_instance));

	/* open the entities */
	rc = open_entity(&obj.ob_entity);
	if (rc)
		RETURN(rc);
	rc = open_entity(&parent_obj.ob_entity);
	if (rc)
		RETURN(rc);

	/* set first operation of batch */
	rc = m0_entity_delete(&obj.ob_entity, &ops[0]);
	if (rc)
		RETURN(rc);

	/* set second operation of batch */
	rc = m0_client_layout_op(&parent_obj, M0_EO_LAYOUT_SET,
				 parent_layout, &ops[1]);
	if (rc) {
		m0_op_free(ops[0]);
		RETURN(rc);
	}

	/* launch them both */
	m0_op_launch(ops, ARRAY_SIZE(ops));
	/* wait for first op to complete */
	rc = m0_op_wait(
		ops[0], M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
		m0_time_from_now(options.op_timeout,0)) ?: m0_rc(ops[0]);
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);

	/* wait for second op to complete */
	rc = rc ?: m0_op_wait(
		ops[1], M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
		m0_time_from_now(options.op_timeout,0)) ?: m0_rc(ops[1]);
	m0_op_fini(ops[1]);
	m0_op_free(ops[1]);

	/* close entities */
	m0_entity_fini(&obj.ob_entity);
	m0_entity_fini(&parent_obj.ob_entity);

	RETURN(rc);
}

/**
 * Get the layout of an object
 */
static int obj_layout_get(struct m0_obj *obj,
			  struct m0_client_layout **layout)
{
	struct m0_op *ops[1] = {NULL};
	int rc;
	ENTRY;

	*layout = m0_client_layout_alloc(M0_LT_COMPOSITE);
	if (*layout == NULL)
		RETURN(-ENOMEM);

	m0_client_layout_op(obj, M0_EO_LAYOUT_GET, *layout, &ops[0]);

	m0_op_launch(ops, 1);

	rc = m0_op_wait(
		ops[0], M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
		m0_time_from_now(options.op_timeout,0));
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);

	RETURN(rc);
}

/**
 * Get the layout of an object designated by its fid.
 */
static int layout_get(struct m0_uint128 id, struct m0_client_layout **layout)
{
	struct m0_obj obj;
	int rc;
	ENTRY;

	/* instanciate the object to be handled */
	M0_SET0(&obj);
	m0_obj_init(&obj, m0_uber_realm, &id, m0_client_layout_id(m0_instance));

	rc = open_entity(&obj.ob_entity);
	if (rc)
		RETURN(rc);

	rc = obj_layout_get(&obj, layout);

	/* close it */
	m0_entity_fini(&obj.ob_entity);
	RETURN(rc);
}

/** Set the layout of an object */
static int obj_layout_set(struct m0_obj *obj,
			  struct m0_client_layout *layout)
{
	struct m0_op *ops[1] = {NULL};
	int rc;
	ENTRY;

	rc = m0_client_layout_op(obj, M0_EO_LAYOUT_SET, layout, &ops[0]);
	if (rc)
		RETURN(rc);

	m0_op_launch(ops, 1);

	rc = m0_op_wait(
		ops[0], M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
		m0_time_from_now(options.op_timeout,0));

	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);

	RETURN(rc);
}

/** Set the layout of an object designated by its fid */
static int layout_set(struct m0_uint128 id, struct m0_client_layout *layout)
{
	struct m0_obj obj;
	int rc;
	ENTRY;

	/* open the object */
	memset(&obj, 0, sizeof(obj));
	m0_obj_init(&obj, m0_uber_realm, &id, m0_client_layout_id(m0_instance));

	rc = open_entity(&obj.ob_entity);
	if (rc)
		RETURN(rc);

	/* update its layout */
	rc = obj_layout_set(&obj, layout);

	/* close it */
	m0_entity_fini(&obj.ob_entity);
	RETURN(rc);
}

/* The definitions above allow iterating on layers and extent lists */
#include "motr/magic.h"
M0_TL_DESCR_DEFINE(clayer, "composite layout layers",
				   static, struct m0_composite_layer,
		   ccr_tlink, ccr_tlink_magic,
		   M0_CLAYER_TL_MAGIC, M0_CLAYER_TL_MAGIC);
M0_TL_DEFINE(clayer, static, struct m0_composite_layer);

M0_TL_DESCR_DEFINE(cext, "composite layout extents",
		   static, struct m0_composite_extent,
		   ce_tlink, ce_tlink_magic,
		   M0_CEXT_TL_MAGIC, M0_CEXT_TL_MAGIC);
M0_TL_DEFINE(cext, static, struct m0_composite_extent);


/** Wrap a single "get next" operation on the extent index
 * (read a batch of extents).
 */
static int get_next_extents(struct m0_idx *idx,
			    struct m0_bufvec *keys,
			    struct m0_bufvec *vals,
			    int *rc_list, int32_t flags)
{
	int                  rc;
	struct m0_op *ops[1] = {NULL};
	ENTRY;

	m0_idx_op(idx, M0_IC_NEXT,
			 keys, vals, rc_list, flags, &ops[0]);
	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0],
			       M0_BITS(M0_OS_FAILED,
				       M0_OS_STABLE),
			       m0_time_from_now(options.op_timeout,0));
	rc = rc ? rc : ops[0]->op_sm.sm_rc;

	/* fini and release */
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);
	RETURN(rc);
}


/**
 * Read returned keys and add them to the extent list
 * @return The number of read extents, or a negative error code.
 */
static int read_extent_keys(struct m0_uint128 subobjid,
			    struct m0_bufvec *keys,
			    struct m0_bufvec *vals,
			    int *rc_list,
			    struct m0_tl *ext_list,
			    struct m0_composite_layer_idx_key *last_key)
{
	struct m0_composite_layer_idx_key  key;
	struct m0_composite_layer_idx_val  val;
	struct m0_composite_extent        *ext;
	int i;
	ENTRY;

	for (i = 0; i < keys->ov_vec.v_nr; i++) {
		/* Reach the end of index. */
		if (keys->ov_buf[i] == NULL ||
		    vals->ov_buf[i] == NULL || rc_list[i] != 0)
			break;

		/* Have retrieved all kv pairs for a layer. */
		m0_composite_layer_idx_key_from_buf(
						&key, keys->ov_buf[i]);
		if (!m0_uint128_eq(&key.cek_layer_id, &subobjid))
			break;

		m0_composite_layer_idx_val_from_buf(
						&val, vals->ov_buf[i]);

		/* Add a new extent. */
		M0_ALLOC_PTR(ext);
		if (ext == NULL)
			RETURN(-ENOMEM);
		ext->ce_id = key.cek_layer_id;
		ext->ce_off = key.cek_off;
		ext->ce_len = val.cev_len;

		DBG("%s: extent %#"PRIx64":%#"PRIx64
		      " [%#"PRIx64"-%#"PRIx64"]\n", __func__,
		      ext->ce_id.u_hi, ext->ce_id.u_lo, key.cek_off,
		      key.cek_off + ext->ce_len - 1);

		/* The extents are in increasing order of offset. */
		cext_tlink_init_at_tail(ext, ext_list);
	}

	ext = cext_tlist_tail(ext_list);
	if (ext != NULL) {
		last_key->cek_layer_id = ext->ce_id;
		last_key->cek_off = ext->ce_off;
	}
	RETURN(i);
}

/** Free all extents in an extent list */
static void extent_list_free(struct m0_tl *ext_list)
{
	struct m0_composite_extent *ext;

	m0_tl_teardown(cext, ext_list, ext)
		m0_free(ext);
}

/** Reset a list of buffers */
static void reset_bufvec(struct m0_bufvec *keys, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		m0_free(keys->ov_buf[i]);
		keys->ov_buf[i] = NULL;
		keys->ov_vec.v_count[i] = 0;
	}
}

/**
 * Helper to load a list of extents for a given layer.
 * @param subobjid	Layer id
 * @param write		Whether to load the read or write extents
 *			(true for write).
 * @param ext_list	List of extents to be populated.
 */
static int layer_load_extent_list(struct m0_uint128 subobjid, bool write,
			          struct m0_tl *ext_list)
{
	struct m0_idx idx = {{0}};
	struct m0_composite_layer_idx_key  curr_key;
	struct m0_bufvec                          keys;
	struct m0_bufvec                          vals;
	int *rc_list = NULL;
	int32_t flags = 0;
	int rc, nb;
	ENTRY;

	if (!cext_tlist_is_empty(ext_list))
	/* already loaded */
		RETURN(0);

	/* Allocate argument parameters */
	rc = m0_bufvec_empty_alloc(&keys, HSM_EXTENT_SCAN_BATCH);
	if (rc)
		return rc;
	rc = m0_bufvec_empty_alloc(&vals, HSM_EXTENT_SCAN_BATCH);
	if (rc)
		goto out_free;

	M0_ALLOC_ARR(rc_list, HSM_EXTENT_SCAN_BATCH);
	if (rc_list == NULL) {
		rc = -ENOMEM;
		goto out_free;
	}

	curr_key.cek_layer_id = subobjid;
	curr_key.cek_off = 0;

	m0_composite_layer_idx(subobjid, write, &idx);

	while (true) {
		/* convert current key to idx buffer */
		rc = m0_composite_layer_idx_key_to_buf(
			&curr_key, &keys.ov_buf[0], &keys.ov_vec.v_count[0]);
		if (rc)
			goto out_free;

		rc = get_next_extents(&idx, &keys, &vals, rc_list, flags);
		if (rc)
			goto out_free;

		nb = read_extent_keys(subobjid, &keys, &vals, rc_list, ext_list,
				      &curr_key);
		if (nb < HSM_EXTENT_SCAN_BATCH)
			break;

		/* Reset keys and vals. */
		reset_bufvec(&keys, HSM_EXTENT_SCAN_BATCH);
		reset_bufvec(&vals, HSM_EXTENT_SCAN_BATCH);

		flags = M0_OIF_EXCLUDE_START_KEY;
	}

out_free:
	m0_idx_fini(&idx);
	m0_bufvec_free(&keys);
	m0_bufvec_free(&vals);
	m0_free0(&rc_list);
	RETURN(rc);
}

static void print_extents(FILE *stream, const struct m0_tl *ext_list,
			  bool details)
{
	struct m0_composite_extent *ext;
	bool is_first = true;

	m0_tl_for(cext, ext_list, ext) {
		if (details)
			fprintf(stream, "%s<%#"PRIx64":%#"PRIx64">:"
			        "[%#"PRIx64"->%#"PRIx64"]",
			        is_first ? "" : " ",
			        ext->ce_id.u_hi, ext->ce_id.u_lo,
			        ext->ce_off, ext->ce_off + ext->ce_len - 1);
		else
			fprintf(stream, "%s[%#"PRIx64"->%#"PRIx64"]",
			        is_first ? "" : " ",
			        ext->ce_off, ext->ce_off + ext->ce_len - 1);

		is_first = false;
	} m0_tl_endfor;
	if (details)
		fprintf(stream, "\n");
}

static void print_layer(FILE *stream, struct m0_composite_layer *layer,
			bool details)
{
	if (details) {
		fprintf(stream, "subobj=<%#"PRIx64":%#"PRIx64">\n",
		        layer->ccr_subobj.u_hi, layer->ccr_subobj.u_lo);
		fprintf(stream, "lid=%"PRIu64"\n", layer->ccr_lid);
		fprintf(stream, "priority=%#x (gen=%u, tier=%hhu)\n",
			layer->ccr_priority,
		        hsm_prio2gen(layer->ccr_priority),
		        hsm_prio2tier(layer->ccr_priority));
	} else {
		fprintf(stream, "gen %u, tier %hhu, extents: ",
			hsm_prio2gen(layer->ccr_priority),
			hsm_prio2tier(layer->ccr_priority));
	}

	/* load read extents for this layer */
	layer_load_extent_list(layer->ccr_subobj, false, &layer->ccr_rd_exts);
	/* load write extents for this layer */
	layer_load_extent_list(layer->ccr_subobj, true,
				&layer->ccr_wr_exts);
	if (details)
		fprintf(stream, "R extents:\n");
	print_extents(stream, &layer->ccr_rd_exts, details);

	if (details) {
		fprintf(stream, "W extents:\n");
		print_extents(stream, &layer->ccr_wr_exts, details);
	} else {
		if (!cext_tlist_is_empty(&layer->ccr_wr_exts))
			fprintf(stream, " (writable)\n");
		else
			fprintf(stream, "\n");
	}
}

static void print_layout(FILE *stream, const struct m0_client_layout  *layout,
			 bool details)
{
	struct m0_client_composite_layout *clayout;
	struct m0_composite_layer *layer;
	int i;

	clayout = M0_AMB(clayout, layout, ccl_layout);
	M0_ASSERT(clayout != NULL);

	if (details)
		fprintf(stream, "%"PRIu64" layers:\n", clayout->ccl_nr_layers);

	/* iterate on all layers and display their extents */
	i = 0;
	m0_tl_for(clayer, &clayout->ccl_layers, layer) {
		if (details)
			fprintf(stream, "==== layer #%d ====\n", i);
		else
			fprintf(stream, "  - ");
		print_layer(stream, layer, details);
		i++;
	} m0_tl_endfor;
}

int m0hsm_dump(FILE *stream, struct m0_uint128 id, bool details)
{
	struct m0_client_layout  *layout = NULL;
	int rc;
	ENTRY;

	if (is_hsm_reserved(id))
		RETURN(-EINVAL);

	rc = layout_get(id, &layout);
	if (rc)
		RETURN(rc);
	M0_ASSERT(layout != NULL);

	print_layout(stream, layout, details);
	RETURN(0);
}



/**
 * Add an extent to a layer.
 * @param subobjid  Layer id.
 * @param ext	    Extent to be added.
 * @param write	    Whether to set a read or write extents
 *		    (true for write).
 * @param overwrite Whether to overwrite a previous extent
 *		    starting at the same offset.
 */
static int layer_extent_add(struct m0_uint128 subobjid,
			    const struct extent *ext,
			    bool write, bool overwrite)
{
	struct m0_bufvec                          keys;
	struct m0_bufvec                          vals;
	struct m0_composite_layer_idx_key  key;
	struct m0_composite_layer_idx_val  val;
	int					 *rcs = NULL;
	struct m0_op *ops[1] = {NULL};
	struct m0_idx idx;
	int rc;
	ENTRY;

	rc = m0_bufvec_empty_alloc(&keys, 1);
	if (rc)
		RETURN(rc);
	rc = m0_bufvec_empty_alloc(&vals, 1);
	if (rc)
		goto free_keys;

	/* Set key and value. */
	key.cek_layer_id = subobjid;
	key.cek_off = ext->off;
	val.cev_len = ext->len;
	rc = m0_composite_layer_idx_key_to_buf(
		&key, &keys.ov_buf[0], &keys.ov_vec.v_count[0]);
	if (rc)
		goto free_vals;

	rc = m0_composite_layer_idx_val_to_buf(
		&val, &vals.ov_buf[0], &vals.ov_vec.v_count[0]);
	if (rc)
		return rc;

	/* now set the key/value */
	memset(&idx, 0, sizeof idx);
	M0_ALLOC_ARR(rcs, 1);
	if (rcs == NULL) {
		rc = -ENOMEM;
		goto out_free;
	}

	DBG("%s %s extent for <%"PRIx64":%"PRIx64">: "
		"[%#"PRIx64"-%#"PRIx64"]\n",
		overwrite ? "Changing" : "Adding",
		write ? "write" : "read",
		subobjid.u_hi, subobjid.u_lo, ext->off,
		ext->off + ext->len - 1);

	ops[0] = NULL;
	m0_composite_layer_idx(subobjid, write, &idx);
	m0_idx_op(&idx, M0_IC_PUT, &keys, &vals, rcs,
			 overwrite ? M0_OIF_OVERWRITE : 0, &ops[0]);
	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0],
		M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
		m0_time_from_now(options.op_timeout,0));
	if (rc)
		goto out_free;
	rc = ops[0]->op_sm.sm_rc;

	/* fini and release */
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);
	m0_entity_fini(&idx.in_entity);

out_free:
	m0_free0(&rcs);
free_vals:
	m0_bufvec_free(&vals);
free_keys:
	m0_bufvec_free(&keys);
	RETURN(rc);
}

static int layer_extent_del(struct m0_uint128 subobjid, off_t off, bool write)
{
	struct m0_bufvec                          keys;
	struct m0_composite_layer_idx_key  key;
	int					 *rcs = NULL;
	struct m0_op *ops[1] = {NULL};
	struct m0_idx idx;
	int rc;
	ENTRY;

	rc = m0_bufvec_empty_alloc(&keys, 1);
	if (rc)
		RETURN(rc);

	/* Set key and value. */
	key.cek_layer_id = subobjid;
	key.cek_off = off;
	rc = m0_composite_layer_idx_key_to_buf(
		&key, &keys.ov_buf[0], &keys.ov_vec.v_count[0]);
	if (rc)
		goto free_keys;

	/* now set the key/value */
	memset(&idx, 0, sizeof idx);
	M0_ALLOC_ARR(rcs, 1);
	if (rcs == NULL) {
		rc = -ENOMEM;
		goto out_free;
	}

	DBG("Dropping %s extent for <%"PRIx64":%"PRIx64"> at offset %#"PRIx64
		" (if it exists)\n", write ? "write" : "read",
		subobjid.u_hi, subobjid.u_lo, off);

	ops[0] = NULL;
	m0_composite_layer_idx(subobjid, write, &idx);
	m0_idx_op(&idx, M0_IC_DEL, &keys, NULL, rcs, 0, &ops[0]);
	m0_op_launch(ops, 1);
	rc = m0_op_wait(ops[0],
		M0_BITS(M0_OS_FAILED, M0_OS_STABLE),
		m0_time_from_now(options.op_timeout,0));
	if (rc)
		goto out_free;
	rc = ops[0]->op_sm.sm_rc;

	/* fini and release */
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);
	m0_entity_fini(&idx.in_entity);

out_free:
	m0_free0(&rcs);
free_keys:
	m0_bufvec_free(&keys);
	RETURN(rc);
}


/**
 * Retrieves the max generation number and the smallest
 * tier index from layers of composite layout.
 */
static void layout_top_prio(struct m0_client_layout *layout, int32_t *max_gen,
			    struct m0_uint128 *max_gen_id, uint8_t *top_tier)
{
	struct m0_client_composite_layout *clayout;
	struct m0_composite_layer *layer;

	*max_gen = -1;
	*top_tier = UINT8_MAX;

	/* get the max generation and create the new layer */
	clayout = M0_AMB(clayout, layout, ccl_layout);
	M0_ASSERT(clayout != NULL);

	/* iterate on all layers to get the max gen, top tier... */
	m0_tl_for(clayer, &clayout->ccl_layers, layer) {
		int32_t gen;
		uint8_t  tier;

		gen = hsm_prio2gen(layer->ccr_priority);
		tier = hsm_prio2tier(layer->ccr_priority);

		if (gen > *max_gen) {
			*max_gen = gen;
			*max_gen_id = layer->ccr_subobj;
		}
		if (top_tier != NULL && tier < *top_tier)
			*top_tier = tier;
	} m0_tl_endfor;
}

/**
 * Release and drop the layer.
 * It is expected that the subobject no longer have writable extent
 * before this function is called.
 */
static int layer_clean(struct m0_uint128 parent_id,
		       struct m0_client_layout *layout,
		       struct m0_composite_layer *layer)
{
	int rc;

	ENTRY;

	/* remove the subobj from the layout */
	m0_composite_layer_del(layout, layer->ccr_subobj);

	/* delete the subobject and update the parent layout */
	rc = delete_obj_set_parent_layout(layer->ccr_subobj, parent_id, layout);

	RETURN(rc);
}

/**
 * Check that the given layer no longer have readable extents.
 * If so, release and drop the layer.
 * It is expected that the subobject no longer have writable extent
 * before this function is called.
 */
static int layer_check_clean(struct m0_uint128 parent_id,
			     struct m0_client_layout *layout,
			     struct m0_composite_layer *layer)
{
	int rc;

	ENTRY;

	/* load read extents for this layer */
	rc = layer_load_extent_list(layer->ccr_subobj, false,
				    &layer->ccr_rd_exts);
	if (rc)
		RETURN(rc);

	/* does the subobject still has extent? */
	if (!cext_tlist_is_empty(&layer->ccr_rd_exts)) {
		DBG("Subobj %"PRIx64":%"PRIx64" still has read extents\n",
		      layer->ccr_subobj.u_hi, layer->ccr_subobj.u_lo);
		RETURN(-ENOTEMPTY);
	}

	rc = layer_clean(parent_id, layout, layer);

	RETURN(rc);
}

/**
 * Release and drop the layer with the given subobj_id in the layout
 * if it no longer has readable extents.
 * It is expected that the subobject no longer have writable extent
 * before this function is called.
 * If successfull, the function sets the layout.
 */
static int layout_layer_clean(struct m0_uint128 parent_id,
			      struct m0_client_layout *layout,
			      struct m0_uint128 subobj_id)
{
	struct m0_client_composite_layout *clayout;
	struct m0_composite_layer *layer;

	clayout = M0_AMB(clayout, layout, ccl_layout);
	M0_ASSERT(clayout != NULL);

	/* look for the given subobj id */
	m0_tl_for(clayer, &clayout->ccl_layers, layer) {
		if (m0_uint128_cmp(&layer->ccr_subobj, &subobj_id) != 0)
			continue;

		return layer_check_clean(parent_id, layout, layer);
	} m0_tl_endfor;

	/* subobj not found */
	return -ENOENT;
}

static int layout_add_top_layer(struct m0_uint128 id,
			        struct m0_client_layout *layout,
				uint8_t tier)
{
	struct m0_uint128 old_id;
	struct m0_obj subobj = {};
	int32_t gen;
	uint8_t top_tier;
	int rc;
	ENTRY;

	DBG("Adding new layer in tier %u to collect new writes\n", tier);

	layout_top_prio(layout, &gen, &old_id, &top_tier);
	if (gen == -1) {
		ERROR("No layers in composite object\n");
		RETURN(-EINVAL);
	}

	gen++;
	VERB("Creating new layer: gen=%d, tier=%hhu\n", gen, tier);

	rc = create_obj(hsm_subobj_id(id, gen, tier), &subobj, false, tier);
	if (rc != 0)
		RETURN(rc);

	/* add the new top layer */
	m0_composite_layer_add(layout, &subobj, hsm_prio(gen, tier));

	/* add write extent for the new layer */
	rc = layer_extent_add(subobj.ob_entity.en_id, &EXT_FULLRANGE, true,
			      false);
	if (rc)
		RETURN(rc);

	/* close the new sub-object */
	m0_entity_fini(&subobj.ob_entity);

	/* drop the previous writable layer */
	rc = layer_extent_del(old_id, 0, true);
	if (rc)
		RETURN(rc);

	/* Check if the layer where we just dropped the write extent
	 * still has a read extent: if not, the layer can be fully dropped
	 * (subobject deleted and removed from the composite layout). */
	rc = layout_layer_clean(id, layout, old_id);
	if (rc != 0 && rc != -ENOENT && rc != -ENOTEMPTY)
		RETURN(rc);

	/* set the layout in case it was not set above */
	if (rc != 0)
		rc = layout_set(id, layout);

	RETURN(rc);
}

static struct m0_composite_layer *
top_layer_get(struct m0_client_layout *layout)
{
	struct m0_client_composite_layout *clayout;

	clayout = M0_AMB(clayout, layout, ccl_layout);
	if (clayout == NULL)
		return NULL;

	return clayer_tlist_head(&clayout->ccl_layers);
}

int m0hsm_set_write_tier(struct m0_uint128 id, uint8_t tier_idx)
{
	struct m0_composite_layer *layer;
	struct m0_client_layout  *layout = NULL;
	int rc;
	ENTRY;

	if (is_hsm_reserved(id))
		RETURN(-EINVAL);

	rc = layout_get(id, &layout);
	if (rc)
		RETURN(rc);
	M0_ASSERT(layout != NULL);

	layer = top_layer_get(layout);
	if (layer != NULL && hsm_prio2tier(layer->ccr_priority) == tier_idx) {
		INFO("Writable layer is already in tier %u\n", tier_idx);
		RETURN(0);
	}

	/* If not, create one */
	rc = layout_add_top_layer(id, layout, tier_idx);
	RETURN(rc);
}

enum ext_match_code {
	EM_ERROR   = -1,	/* Match error */
	EM_NONE    = 0,		/* Extent has no match. */
	EM_PARTIAL = (1 << 0),  /* Extent partially matches (or contiguous,
				 * with merge mode). */
	EM_FULL    = (1 << 2),	/* Extent is fully included in existing
				 * extents. */
};

enum ext_match_type {
	EMT_INTERSECT,
	EMT_MERGE,
};

#define MAX(_x, _y) ((_x) > (_y) ? (_x) : (_y))
#define MIN(_x, _y) ((_x) < (_y) ? (_x) : (_y))

static enum ext_match_code ext_match(struct m0_uint128 layer_id,
				struct m0_tl *ext_list,
				const struct extent *ext_in,
				struct extent *match,
				enum ext_match_type mode,
				bool is_write,
				bool del_prev)
{
	struct m0_composite_extent *ext;
	off_t end = ext_in->off + ext_in->len - 1;
	bool is_merged = false;

	m0_tl_for(cext, ext_list, ext) {
		off_t ext_end = ext->ce_off + ext->ce_len - 1;

		// XXX assuming that READ extents are aggregated, so a single
		// extent should match

		/* extent is fully included in another */
		if (ext_in->off >= ext->ce_off &&
		    (ext_in->off + ext_in->len) <= ext->ce_off + ext->ce_len) {
			if (mode == EMT_INTERSECT) {
				/* match->off = max(in_offset, ext->ce_off)
				 * = in_offset */
				*match = *ext_in;
			} else { /* merge */
				/* match_off = min(offset, ext->ce_off) */
				match->off = ext->ce_off;
				/* extent is fully included: no change in
				 * previous length */
				match->len = ext->ce_len;
				/* No previous extent to be overwritten */
			}
			return EM_FULL;
		}

		/* Check partial match */
		if ((ext->ce_off >= ext_in->off && ext->ce_off <= end) ||
		    (ext_end >= ext_in->off && ext_end <= end)) {
			off_t m_end;
			if (mode == EMT_INTERSECT) {
				match->off = MAX(ext_in->off, ext->ce_off);
				m_end = MIN(ext_end, end);
				match->len = (m_end - match->off) + 1;
				return EM_PARTIAL;
			} else { /* merge */

				/* Check if it already merged with previous extents? */
				/* If so, keep the value in match_off
				 * (assuming extents are ordered by offset). */
				if (!is_merged)
					match->off = MIN(ext_in->off, ext->ce_off);

				m_end = MAX(ext_end, end);
				match->len = (m_end - match->off) + 1;

				/* Drop merged extent. */
				if (del_prev && ext->ce_off != match->off) {
					if (layer_extent_del(layer_id,
						ext->ce_off, is_write) != 0)
						return EM_ERROR;
					cext_tlist_del(ext);
				}

				is_merged = true;
				/* next overlapping extents should be merged too */
				continue;
			}
		}

		/* Check consecutive extent (merge mode only). */
		if (mode == EMT_MERGE) {
			if (ext->ce_off + ext->ce_len == ext_in->off) {
				/* contiguous extent before */
				match->off = ext->ce_off;
				match->len = ext->ce_len + ext_in->len;
				is_merged = true;

				DBG("Merge with previous extent "
				    "starting at %#"PRIx64"\n", ext->ce_off);

				/* Merged extent keeps the same offset
				 * (don't delete it from index)  */

			} else if (ext_in->off + ext_in->len == ext->ce_off) {
				/* contiguous extent after */
				if (!is_merged) {
					match->off = ext_in->off;
					match->len = ext->ce_len +
							 ext_in->len;
					is_merged = true;
				} else {
					/* Already merged with a previous extent,
					 * just extend size. */
					match->len += ext->ce_len;
				}

				DBG("Merge with next extent starting "
				    "at %#"PRIx64"\n", ext->ce_off);

				/* Merged extent offset is different.
				 * Delete it from index. */
				if (del_prev) {
					if (layer_extent_del(layer_id,
						ext->ce_off, is_write) != 0)
						return EM_ERROR;
					cext_tlist_del(ext);
				}
			}
			/* continue, as the next extent can overlap */
		}

	} m0_tl_endfor;

	return is_merged ? EM_PARTIAL : EM_NONE;
}

/**
 * Subtract an extent from existing extents.
 * This function is supposed to be called with an
 * intersecting extent, computed by ext_match.
 * So the only possible case is a full matching extent.
 */
static int ext_subtract(struct m0_uint128 layer_id,
			struct m0_tl *ext_list,
			const struct extent *ext_in,
			bool is_write,
			bool *layer_empty)
{
	/* possible cases:
	   orig ext: [---------]
		  -  [XXX]
		  =       [----]
		  -     [XXX]
		  =  [-]     [-]
		  -:	   [XXX]
		  =:  [---]
	*/
	struct m0_composite_extent *ext;
	int rc;

	/* counter to determine if the layer is finally empty */
	int remaining_extent = 0;

	m0_tl_for(cext, ext_list, ext) {
		struct extent new_ext;
		off_t ext_end = ext->ce_off + ext->ce_len - 1;

		remaining_extent ++;

		// XXX assuming that READ extents are aggregated, so a single
		// extent should match

		/* extent is expected to be fully included in another */
		if (ext_in->off < ext->ce_off ||
		    (ext_in->off + ext_in->len) > ext->ce_off + ext->ce_len)
			continue;

		/* If the extent start exactly matches, drop it
		 * and create the remaining extent (except if the
		 * subtracted region ends >= extent end) */
		if (ext->ce_off == ext_in->off) {
			rc = layer_extent_del(layer_id, ext->ce_off,
					      is_write);
			if (rc)
				return rc;
			cext_tlist_del(ext);

			remaining_extent --;

			/* create the remaining region expect if it
			 * was fully covered */
			if (ext_in->off + ext_in->len <
			    ext->ce_off + ext->ce_len) {
				new_ext.off = ext_in->off + ext_in->len;
				new_ext.len = ext->ce_len - ext_in->len;
				/* don't overwrite, the extent is not supposed
				 * to exist already */
				rc = layer_extent_add(layer_id, &new_ext,
						      is_write, false);
				if (rc)
					return rc;
				remaining_extent ++;
			}
			/* continue iterating to count remaining extents */
			/* next extents are not supposed to match */
			continue;
		}

		/* Else, shorten and/or split the original extent */
		/* 1) extent before */
		new_ext.off = ext->ce_off;
		new_ext.len = ext_in->off - ext->ce_off;
		rc = layer_extent_add(layer_id, &new_ext, is_write,
				      true);
		if (rc)
			return rc;
		remaining_extent ++;

		/* 2) extent after, if any */
		if (ext_in->off + ext_in->len <
		    ext->ce_off + ext->ce_len) {
			new_ext.off = ext_in->off + ext_in->len;
			new_ext.len = ext_end - new_ext.off + 1;
			/* don't overwrite, the extent is not
			 * supposed to exist already */
			rc = layer_extent_add(layer_id,
					      &new_ext,
					      is_write, false);
			if (rc)
				return rc;
			remaining_extent ++;
		}
	} m0_tl_endfor;

	*layer_empty = (remaining_extent == 0);
	return 0;
}

/** Add and merge read extent in a given layer */
static int add_merge_read_extent(struct m0_composite_layer *layer,
				 const struct extent *ext_in)
{
	struct m0_uint128 subobj_id = layer->ccr_subobj;
	struct extent ext_merge = {.off = -1LL, .len = 0};
	enum ext_match_code m;
	int rc;
	ENTRY;

	/* load previous extents to see if we can merge them */
	layer_load_extent_list(layer->ccr_subobj, false, &layer->ccr_rd_exts);

	m = ext_match(layer->ccr_subobj, &layer->ccr_rd_exts, ext_in,
		      &ext_merge, EMT_MERGE, false, true);

	switch (m) {
	case EM_ERROR:
		ERROR("Failed to match/merge extent\n");
		rc = -EIO;
		break;

	case EM_NONE:
		/* no match, add read extent to the layer */
		rc = layer_extent_add(subobj_id, ext_in, false, false);
		break;

	case EM_FULL:
		DBG("Extent included to another. Nothing to do\n");
		rc = 0;
		break;

	case EM_PARTIAL:
		DBG("Extent overlap detected: must merge\n");

		/* Add the extent in overwrite mode. Other merged extents
		 * have been dropped by ext_match previously. */
		rc = layer_extent_add(subobj_id, &ext_merge, false, true);
		break;
	default:
		ERROR("ERROR: unexpected match type\n");
		rc = -EINVAL;
	}
	RETURN(rc);
}

static int m0hsm_release_maxgen(struct m0_uint128 id, uint8_t tier, int max_gen,
				off_t offset, size_t len,
				enum hsm_rls_flags flags,
				bool user_display);

static int top_layer_add_read_extent(struct m0_obj *obj,
			             const struct extent *ext)
{
	struct m0_client_layout  *layout = NULL;
	struct m0_composite_layer *layer;
	int rc;
	int gen;
	ENTRY;

	rc = obj_layout_get(obj, &layout);
	if (rc)
		RETURN(rc);
	M0_ASSERT(layout != NULL);

	layer = top_layer_get(layout);
	if (layer == NULL)
	/* TODO free layout */
		RETURN(-ENODATA);

	rc = add_merge_read_extent(layer, ext);
	if (rc)
		 RETURN(rc);

	gen = hsm_prio2gen(layer->ccr_priority);
	if (gen > 1) {
		/* clean older versions of data in the write tier */
		rc = m0hsm_release_maxgen(obj->ob_entity.en_id,
					  hsm_prio2tier(layer->ccr_priority),
					  gen -1, ext->off, ext->len, 0, false);
	}
	RETURN(rc);
}


int m0hsm_create(struct m0_uint128 id, struct m0_obj *obj,
		 uint8_t tier_idx, bool keep_open)
{
	/* create a composite layout that will be assigned to the object */
	int rc;
	struct m0_client_layout *layout;
	struct m0_obj subobj = {};
	ENTRY;

	if (is_hsm_reserved(id))
		RETURN(-EINVAL);

	/* Create a sub-object that will be the first layer.
	 * This initial subobj is generation 0 */
	rc = create_obj(hsm_subobj_id(id, 0, tier_idx), &subobj, false, tier_idx);
	if (rc != 0)
		RETURN(rc);

	/* allocate composite layout */
	layout = m0_client_layout_alloc(M0_LT_COMPOSITE);
	if (layout == NULL)
		RETURN(-ENOMEM);

	/* make the subobject a single-level layout */
	m0_composite_layer_add(layout, &subobj, hsm_prio(0, tier_idx));

	/* create an extent to enable write operations anywhere in this subobject */
	rc = layer_extent_add(subobj.ob_entity.en_id, &EXT_FULLRANGE, true,
			      false);
	if (rc)
		RETURN(rc);

	/* close it */
	m0_entity_fini(&subobj.ob_entity);

	/* then create the main objet */
//#ifdef BATCH_CREATE_SET_LAYOUT
	if (0) {
		rc = create_obj_with_layout(id, obj, layout, false);
		if (rc)
			RETURN(rc);
	} else {
		rc = create_obj(id, obj, false, HSM_ANY_TIER);
		if (rc)
			RETURN(rc);

		rc = obj_layout_set(obj, layout);
		if (rc)
			RETURN(rc);
	}
	if (!keep_open)
		m0_entity_fini(&obj->ob_entity);

	INFO("Composite object successfully created with "
	     "id=%#"PRIx64":%#"PRIx64"\n", id.u_hi, id.u_lo);

	RETURN(0);
}

/** manage IO resources */
struct io_ctx {
	int		curr_blocks;
	size_t		curr_bsize;
	struct m0_indexvec ext;
	struct m0_bufvec   data;
	struct m0_bufvec   attr;
};

/** write a block to an open entity */
static int do_io_op(struct m0_obj *obj, enum m0_obj_opcode opcode,
		    struct io_ctx *ctx)
{
	struct m0_op *ops[1] = {NULL};
	int rc;
	ENTRY;

	/* Create the write request */
	m0_obj_op(obj, opcode, &ctx->ext, &ctx->data, &ctx->attr,
		  0, 0, &ops[0]);

	/* Launch the write request*/
	m0_op_launch(ops, 1);

	/* wait for completion */
	rc = m0_op_wait(ops[0], M0_BITS(M0_OS_FAILED,
					       M0_OS_STABLE),
			       M0_TIME_NEVER) ?: m0_rc(ops[0]);

	/* finalize and release */
	m0_op_fini(ops[0]);
	m0_op_free(ops[0]);

	RETURN(rc);
}

/** write a block to an open entity */
static int write_blocks(struct m0_obj *obj, struct io_ctx *ctx)
{
	RETURN(do_io_op(obj, M0_OC_WRITE, ctx));
}

/** read data from an open object */
static int read_blocks(struct m0_obj *obj, struct io_ctx *ctx)
{
	RETURN(do_io_op(obj, M0_OC_READ, ctx));
}

/** Generate and Write data to the I/O vector, using the given seed */
static void gen_data(struct m0_bufvec *data, size_t bsize, int seed)
{
	int i;

	for (i = 0; i < data->ov_vec.v_nr; i++)
		memset(data->ov_buf[i], seed, bsize);
}

static void dump_data(struct m0_bufvec *data, size_t bsize)
{
	int i, j;

	for (i = 0; i < data->ov_vec.v_nr; i++)
		for (j = 0; j < bsize; j++)
			putchar(((char *)data->ov_buf[i])[j]);
}

/**
 * Allocate I/O context for a particular number of blocks and block size
 * @param alloc_io_buff	Does the function allocate I/O buffers
 *			or are they provided by the user?
 */
static int prepare_io_ctx(struct io_ctx *ctx, int blocks, size_t block_size,
			  bool alloc_io_buff)
{
	int rc;

	/* allocate new I/O vec? */
	if (blocks != ctx->curr_blocks || block_size != ctx->curr_bsize) {
		if (alloc_io_buff) {
			if (ctx->curr_blocks != 0)
				m0_bufvec_free(&ctx->data);

			/* Allocate block_count * block_size */
			rc = m0_bufvec_alloc(&ctx->data, blocks, block_size);
			if (rc != 0)
				return rc;
		} else {
			if (ctx->curr_blocks != 0)
				m0_bufvec_free2(&ctx->data);

			rc = m0_bufvec_empty_alloc(&ctx->data, blocks);
			if (rc != 0)
				return rc;
		}
	}

	/* allocate extents and attrs, if they are not already */
	if (blocks != ctx->curr_blocks) {

		/* free previous vectors */
		if (ctx->curr_blocks > 0) {
			m0_bufvec_free(&ctx->attr);
			m0_indexvec_free(&ctx->ext);
		}

		/* Allocate attr and extent list and attr */
		rc = m0_bufvec_alloc(&ctx->attr, blocks, 1);
		if(rc != 0)
			return rc;

		rc = m0_indexvec_alloc(&ctx->ext, blocks);
		if (rc != 0)
			return rc;
	}

	ctx->curr_blocks = blocks;
	ctx->curr_bsize = block_size;
	return 0;
}

static inline int check_vec(struct m0_bufvec *vec, int blocks)
{
	/* arrays not allocated? */
	if (!vec->ov_vec.v_count || !vec->ov_buf)
		return -EFAULT;

	/* invalid slot count? */
	if (vec->ov_vec.v_nr != blocks)
		return -EINVAL;

	return 0;
}

/**
 * Fill in an I/O context for a given I/O operation.
 * Map user provided buffer if not NULL.
 */
static int map_io_ctx(struct io_ctx *ctx, int blocks, size_t b_size,
		      off_t offset, char *buff)
{
	int i;

	if (!ctx || blocks == 0)
		return -EINVAL;

	M0_ASSERT(check_vec(&ctx->data, blocks) == 0);
	M0_ASSERT(check_vec(&ctx->attr, blocks) == 0);

	for (i = 0; i < blocks; i++) {
		ctx->ext.iv_index[i] = offset + i * b_size;
		ctx->ext.iv_vec.v_count[i] = b_size;

		VERB("IO block: offset=%#"PRIx64", length=%#"PRIx64"\n",
		     ctx->ext.iv_index[i], ctx->ext.iv_vec.v_count[i]);

		/* we don't want any attributes */
		ctx->attr.ov_vec.v_count[i] = 0;

		if (ctx->data.ov_vec.v_count[i] == 0)
			ctx->data.ov_vec.v_count[i] = b_size;
		/* check the allocated buffer has the right size */
		else if (ctx->data.ov_vec.v_count[i] != b_size)
			return -EINVAL;

		/* map the user-provided I/O buffer */
		if (buff != NULL)
		        ctx->data.ov_buf[i] = buff + i * b_size;
	}

	return 0;
}

/** release resources in an IO context */
static void free_io_ctx(struct io_ctx *ctx, bool alloc_io_buff)
{
	if (alloc_io_buff)
		m0_bufvec_free(&ctx->data);
	else
		m0_bufvec_free2(&ctx->data);
	m0_bufvec_free(&ctx->attr);
	m0_indexvec_free(&ctx->ext);
}

static uint64_t roundup_power2(uint64_t x)
{
	uint64_t power = 1;

	while (power < x)
		power *= 2;

	return power;
}

/**
 * Calculate the optimal block size for the object store I/O
 */
uint64_t get_optimal_bs(struct m0_obj *obj, uint64_t obj_sz)
{
	unsigned long           usz; /* unit size */
	unsigned long           gsz; /* data units in parity group */
	uint64_t                max_bs;
	unsigned                lid;
	struct m0_pool_version *pver;
	struct m0_pdclust_attr *pa;

	if (obj_sz > MAX_M0_BUFSZ)
		obj_sz = MAX_M0_BUFSZ;

	lid = obj->ob_attr.oa_layout_id;
	pver = m0_pool_version_find(&m0_instance->m0c_pools_common,
				    &obj->ob_attr.oa_pver);
	if (pver == NULL) {
		ERROR("Cannot find the object's pool version\n");
		return 0;
	}
	usz = m0_obj_layout_id_to_unit_size(lid);
	pa = &pver->pv_attr;
	gsz = usz * pa->pa_N;
	/* max 2-times pool-width deep, otherwise we may get -E2BIG */
	max_bs = usz * 2 * pa->pa_P * pa->pa_N / (pa->pa_N + 2 * pa->pa_K);

	VERB("usz=%lu pool="FID_F" (N,K,P)=(%u,%u,%u) max_bs=%"PRId64"\n", usz,
	     FID_P(&pver->pv_pool->po_id), pa->pa_N, pa->pa_K, pa->pa_P, max_bs);

	if (obj_sz >= max_bs)
		return max_bs;
	else if (obj_sz <= gsz)
		return gsz;
	else
		return roundup_power2(obj_sz);
}

/**
 * write data to an existing object at the given offset
 * @param seed  Used to generate block contents so it can
 *		be used to verify we read the right layer.
 */
int m0hsm_test_write(struct m0_uint128 id, off_t offset, size_t len, int seed)
{
	int rc;
	size_t block_size;
	size_t rest = len;
	off_t  off = offset;
	struct m0_composite_layer *layer;
	struct m0_client_layout *layout = NULL;
	struct extent wext = {.off = off, .len = len};
	struct m0_obj obj = {}, subobj = {};
	struct io_ctx ctx = {};
	ENTRY;

	/* Set the object entity we want to write */
	m0_obj_init(&obj, m0_uber_realm, &id, m0_client_layout_id(m0_instance));
	rc = open_entity(&obj.ob_entity);
	if (rc)
		RETURN(rc);

	rc = obj_layout_get(&obj, &layout);
	if (rc) {
		ERROR("Could not get object's layout: rc=%d\n", rc);
		return rc;
	}

	layer = top_layer_get(layout);
	if (layer == NULL) {
		ERROR("Could not get the object's top layer\n");
		rc = -EINVAL;
		goto fini;
	}

	m0_obj_init(&subobj, m0_uber_realm, &layer->ccr_subobj,
			   m0_client_layout_id(m0_instance));
	rc = open_entity(&subobj.ob_entity);
	if (rc)
		RETURN(rc);

	block_size = get_optimal_bs(&subobj, rest);
	if (block_size == 0) {
		ERROR("Could not get the optimal block size for the object\n");
		rc = -EINVAL;
		goto fini;
	}
	m0_entity_fini(&subobj.ob_entity);

	VERB("Using I/O block size of %zu bytes\n", block_size);

	for (; rest > 0; rest -= block_size, off += block_size) {
		if (rest < block_size)
			/* non full block */
			block_size = rest;

		rc = prepare_io_ctx(&ctx, 1, block_size, true);
		if (rc) {
			ERROR("prepare_io_ctx() failed: rc=%d\n", rc);
			goto fini;
		}

		rc = map_io_ctx(&ctx, 1, block_size, off, NULL);
		if (rc) {
			ERROR("map_io_ctx() failed: rc=%d\n", rc);
			break;
		}

		/* fill buffers with generated data */
		gen_data(&ctx.data, block_size, seed);

		/* write them */
		rc = write_blocks(&obj, &ctx);
		if (rc) {
			ERROR("write_blocks() failed: rc=%d\n", rc);
			break;
		}
	}

	/* Free bufvec's and indexvec's */
	free_io_ctx(&ctx, true);

	/* Data is always written to the top layer.
	 * Add corresponding read extent */
	rc = rc ?: top_layer_add_read_extent(&obj, &wext);
 fini:
	m0_entity_fini(&obj.ob_entity);

	if (rc == 0)
		INFO("%zu bytes successfully written at offset %#"PRIx64" "
		     "(object id=%#"PRIx64":%#"PRIx64")\n", len, offset,
		     id.u_hi, id.u_lo);

	RETURN(rc);
}

int m0hsm_pwrite(struct m0_obj *obj, void *buffer, size_t length, off_t offset)
{
	int blocks;
	int rc = 0;
	size_t io_size;
	ssize_t rest = length;
	off_t off = offset;
	char *buf = buffer;
	char *pad_buf = NULL;
	struct extent wext;
	struct io_ctx ctx = {};
	ENTRY;

	wext.off = offset;
	wext.len = length;

	io_size = m0_obj_layout_id_to_unit_size(
			m0_client_layout_id(m0_instance));
	M0_ASSERT(io_size > 0);
	VERB("using io_size of %zu bytes\n", io_size);

	while (rest > 0) {
		/* count remaining blocks */
		blocks = rest / io_size;
		if (blocks == 0) {
			/* last non full block */
			blocks = 1;
			/* prepare a padded block */
			INFO("Padding last block of unaligned size %zd up to "
			     "%zu\n", rest, io_size);
			pad_buf = calloc(1, io_size);
			if (!pad_buf) {
				rc = -ENOMEM;
				break;
			}
			memcpy(pad_buf, buf, rest);
			buf = pad_buf;
			wext.len += io_size - rest;
			length += io_size - rest;
		}
		if (blocks > MAX_BLOCK_COUNT)
			blocks = MAX_BLOCK_COUNT;

		rc = prepare_io_ctx(&ctx, blocks, io_size, false) ?:
		     map_io_ctx(&ctx, blocks, io_size, off, buf) ?:
		     write_blocks(obj, &ctx);
		if (rc)
			break;

		buf += blocks * io_size;
		off += blocks * io_size;
		rest -= blocks * io_size;
	}

	if (pad_buf)
		free(pad_buf);

	/* Free bufvec's and indexvec's */
	free_io_ctx(&ctx, false);

	/* Data is always written to the top layer.
	 * Add corresponding read extent */
	if (rc == 0) {
		rc = top_layer_add_read_extent(obj, &wext);

		struct m0_uint128 id = obj->ob_entity.en_id;
		INFO("%zu bytes successfully written at offset %#"PRIx64" "
		     "(object id=%#"PRIx64":%#"PRIx64")\n", length, offset,
		     id.u_hi, id.u_lo);
	}

	RETURN(rc);
}

/**
 * read data from an existing object at the given offset.
 */
int m0hsm_test_read(struct m0_uint128 id, off_t offset, size_t len)
{
	struct m0_obj obj;
	int blocks;
	size_t io_size;
	struct io_ctx ctx = {0};
	size_t rest;
	off_t start = offset;
	int rc;
	ENTRY;

	memset(&obj, 0, sizeof(struct m0_obj));

	/* Set the object entity we want to write */
	m0_obj_init(&obj, m0_uber_realm, &id, m0_client_layout_id(m0_instance));

	io_size = m0_obj_layout_id_to_unit_size(
			m0_client_layout_id(m0_instance));
	M0_ASSERT(io_size > 0);
	VERB("using io_size of %zu bytes\n", io_size);

	rc = open_entity(&obj.ob_entity);
	if (rc)
		RETURN(rc);

	for (rest = len; rest > 0; rest -= blocks * io_size) {
		/* count remaining blocks */
		blocks = rest / io_size;
		if (blocks == 0) {
			/* last non full block */
			blocks = 1;
			io_size = rest;
		}
		if (blocks > MAX_BLOCK_COUNT)
			blocks = MAX_BLOCK_COUNT;

		rc = prepare_io_ctx(&ctx, blocks, io_size, true) ?:
		     map_io_ctx(&ctx, blocks, io_size, start, NULL) ?:
		     read_blocks(&obj, &ctx);
		if (rc)
			break;

		/* dump them to stdout */
		dump_data(&ctx.data, io_size);

		start += blocks * io_size;
	}

	m0_entity_fini(&obj.ob_entity);

	/* Free bufvec's and indexvec's */
	free_io_ctx(&ctx, true);

	if (rc == 0)
		INFO("%zu bytes successfully read at offset %#"PRIx64" "
		     "(object id=%#"PRIx64":%#"PRIx64")\n", len, offset,
		     id.u_hi, id.u_lo);

	RETURN(rc);
}

/** copy an extent from one (flat) object to another */
static int copy_extent_data(struct m0_uint128 src_id,
			    struct m0_uint128 tgt_id,
		            const struct extent *range)
{
	struct m0_obj src_obj = {};
	struct m0_obj tgt_obj = {};
	size_t block_size;
	struct io_ctx ctx = {};
	size_t rest = range->len;
	off_t start = range->off;
	int rc;
	ENTRY;

	m0_obj_init(&src_obj, m0_uber_realm, &src_id,
			   m0_client_layout_id(m0_instance));
	m0_obj_init(&tgt_obj, m0_uber_realm, &tgt_id,
			   m0_client_layout_id(m0_instance));

	/* open the entities */
	rc = open_entity(&src_obj.ob_entity);
	if (rc)
		RETURN(rc);
	rc = open_entity(&tgt_obj.ob_entity);
	if (rc)
		goto out_close_src;

	block_size = get_optimal_bs(&tgt_obj, rest);
	if (block_size == 0) {
		ERROR("Could not get the optimal block size for the object\n");
		rc = -EINVAL;
		goto fini;
	}

	VERB("Using I/O block size of %zu bytes\n", block_size);

	for (; rest > 0; rest -= block_size, start += block_size) {
		if (rest < block_size)
			/* non full block */
			block_size = rest;

		rc = prepare_io_ctx(&ctx, 1, block_size, true);
		if (rc) {
			ERROR("prepare_io_ctx() failed: rc=%d\n", rc);
			goto fini;
		}

		rc = map_io_ctx(&ctx, 1, block_size, start, NULL);
		if (rc) {
			ERROR("map_io_ctx() failed: rc=%d\n", rc);
			break;
		}

		/* read blocks */
		rc = read_blocks(&src_obj, &ctx);
		if (rc) {
			ERROR("read_blocks() failed: rc=%d\n", rc);
			break;
		}

		/* now write data to the target object */
		rc = write_blocks(&tgt_obj, &ctx);
		if (rc) {
			ERROR("write_blocks() failed: rc=%d\n", rc);
			break;
		}
	}

	/* Free bufvec's and indexvec's */
	free_io_ctx(&ctx, true);
 fini:
	m0_entity_fini(&tgt_obj.ob_entity);
 out_close_src:
	m0_entity_fini(&src_obj.ob_entity);

	if (rc == 0)
		INFO("%zu bytes successfully copied from subobj "
		     "<%#"PRIx64":%#"PRIx64"> to <%#"PRIx64":%#"PRIx64">"
	             " at offset %#"PRIx64"\n", range->len,
		     src_id.u_hi, src_id.u_lo, tgt_id.u_hi, tgt_id.u_lo,
		     range->off);

	RETURN(rc);
}


static int check_top_layer_writable(struct m0_client_layout *layout,
				    int max_prio, int tier)
{
	struct m0_composite_layer *layer;

	layer = top_layer_get(layout);
	if (!layer)
		return -ENOENT;

	if (layer->ccr_priority >= max_prio)
		return -ENOENT;

	if (tier != HSM_ANY_TIER && hsm_prio2tier(layer->ccr_priority) != tier)
		return -ENOENT;

	return 0;
}

/**
 * Callback function type for match_layer_foreach().
 * @param[in,out] cb_arg Custom callback argument.
 * @param[in] layout	The whole composite layout structure.
 * @param[in] layer	Layer of the matching extent.
 * @param[in] match	Matching extent.
 * @param[out] stop	setting to true will interupt the iteration.
 */
typedef int (*match_layer_cb_t)(void *cb_arg,
			        struct m0_client_layout *layout,
				struct m0_composite_layer *layer,
			        const struct extent *match,
				bool *stop);

/**
 * Iterate on object layers that match the given tier and the given extent.
 * Iteration stops if a callback returns non-zero or sets the "stop" boolean.
 */
static int match_layer_foreach(struct m0_client_layout *layout, uint8_t tier,
			       const struct extent *ext,
			       match_layer_cb_t cb, void *cb_arg,
			       bool stop_on_error)
{
	struct m0_client_composite_layout *clayout;
	struct m0_composite_layer  *layer;
	struct extent match;
	uint8_t ltier;
	int rc;
	bool stop = false;

	clayout = M0_AMB(clayout, layout, ccl_layout);
	M0_ASSERT(clayout != NULL);

	m0_tl_for(clayer, &clayout->ccl_layers, layer) {
		/* If tier index matches, load its extents */
		ltier = hsm_prio2tier(layer->ccr_priority);
		if (tier != HSM_ANY_TIER && ltier != tier)
			continue;

		/* load readable extents for this layer */
		layer_load_extent_list(layer->ccr_subobj, false,
				       &layer->ccr_rd_exts);

		enum ext_match_code m;
		struct extent ext_curr = *ext;
		/* multiple extents may match, so loop on them */
		do {
			memset(&match, 0, sizeof(match));
			m = ext_match(layer->ccr_subobj, &layer->ccr_rd_exts,
				      &ext_curr, &match,
				      EMT_INTERSECT, false, false);
			if (m == EM_NONE)
				continue;

			VERB("Found layer %s matching extent "
				"[%#"PRIx64"-%#"PRIx64"]: gen=%u, tier=%u, "
				"matching_region [%#"PRIx64"-%#"PRIx64"]\n",
				m == EM_FULL ? "fully" : "partially",
				ext->off, ext->off + ext->len - 1,
				hsm_prio2gen(layer->ccr_priority), ltier,
				match.off, match.off + match.len - 1);

			rc = cb(cb_arg, layout, layer, &match, &stop);
			if (rc && stop_on_error)
				return rc;

			/* look for next extents in the layer */
			if (m == EM_PARTIAL) {
				ext_curr.off = match.off + match.len;
				ext_curr.len -= match.len;
			}

		} while (!stop && m == EM_PARTIAL);
		if (stop)
			break;

	} m0_tl_endfor;

	/* none found, or 'stop' set to true */
	return 0;
}

/** Return the layer with the given priority */
static struct m0_composite_layer *
	layer_get_by_prio(struct m0_client_layout *layout, int prio)
{
	struct m0_client_composite_layout *clayout;
	struct m0_composite_layer  *layer;

	clayout = M0_AMB(clayout, layout, ccl_layout);
	M0_ASSERT(clayout != NULL);

	m0_tl_for(clayer, &clayout->ccl_layers, layer) {
		if (layer->ccr_priority == prio)
			return layer;
	} m0_tl_endfor;

	/* none found */
	return NULL;
}


struct min_gen_check_arg {
	int min_gen;
	struct m0_uint128 except_subobj;
	const struct extent *orig_extent;
	bool found;
};

static int min_gen_check_cb(void *cb_arg,
			    struct m0_client_layout *layout,
			    struct m0_composite_layer *layer,
			    const struct extent *match,
			    bool *stop)
{
	struct min_gen_check_arg *arg = cb_arg;
	struct extent dummy;

	if (m0_uint128_cmp(&layer->ccr_subobj, &arg->except_subobj) == 0) {
		DBG("%s: skip self subobj %#"PRIx64":%#"PRIx64"\n", __func__,
			  arg->except_subobj.u_hi, arg->except_subobj.u_lo);
		/* skip this subobj */
		return 0;
	}

	if (hsm_prio2gen(layer->ccr_priority) < arg->min_gen) {
		DBG("%s: skip layer of lower generation %u\n", __func__,
			  hsm_prio2gen(layer->ccr_priority));
		/* lower generation, skip */
		return 0;
	}

	/* if the extent fully covers it: OK and stop */
	if (ext_match(layer->ccr_subobj, &layer->ccr_rd_exts,
		      arg->orig_extent, &dummy, EMT_INTERSECT,
		      false, false) == EM_FULL) {
		arg->found = true;
		*stop = true;
		return 0;
	}

	/* partial match */
	DBG("%s: partial match\n", __func__);
	return 0;
}

/** check that an extent with at least the given generation exists
 * (except in the designated subobj) */
static int check_min_gen_exists(struct m0_client_layout *layout,
				const struct extent *ext, int gen,
				struct m0_composite_layer *except_layer)
{
	struct min_gen_check_arg cb_arg = {
		.min_gen = gen,
		.except_subobj = except_layer->ccr_subobj,
		.orig_extent = ext,
		.found = false,
	};
	int rc;

	rc = match_layer_foreach(layout, HSM_ANY_TIER, ext, min_gen_check_cb,
				 &cb_arg, false);
	if (rc != 0 || !cb_arg.found)
	{
		ERROR("Found no extent matching [%#"PRIx64
			"-%#"PRIx64"] with generation >= %d: "
			"can't release it from tier %u\n",
			ext->off, ext->off + ext->len - 1, gen,
			hsm_prio2tier(except_layer->ccr_priority));
		return -EPERM;
	}
	return 0;
}

struct release_cb_ctx {
	int	found;
	struct m0_uint128 obj_id;
	uint8_t tier;
	int max_gen; /* max generation to release */
	int max_tier; /* max tier to release from */
};

static int release_cb(void *cb_arg, struct m0_client_layout *layout,
		      struct m0_composite_layer *layer,
		      const struct extent *match,
		      bool *stop)
{
	struct release_cb_ctx *ctx = cb_arg;
	bool layer_empty = false;
	int gen;
	int rc;
	int tier;
	ENTRY;

	gen = hsm_prio2gen(layer->ccr_priority);
	if (ctx->max_gen != -1) {
		/* over max gen, skip it */
		if (gen > ctx->max_gen)
			return 0;
	}
	tier = hsm_prio2tier(layer->ccr_priority);
	if (ctx->max_tier != -1) {
		/* over max tier, skip it */
		if (tier > ctx->max_tier)
			return 0;
	}

	ctx->found++;

	/* XXX The covering can be done by multiple extents
	 * in multiple layers */
	/* An extent will equal or higher generation must exist */
	rc = check_min_gen_exists(layout, match, gen, layer);
	if (rc)
		RETURN(rc);

	/* Check there is a writable layer different from current */
	rc = check_top_layer_writable(layout, layer->ccr_priority,
				      HSM_ANY_TIER);
	if (rc == -ENOENT) {
		/* If not, create one */
		rc = layout_add_top_layer(ctx->obj_id, layout, ctx->tier);
		if (rc)
			RETURN(rc);
	}

	/* Subtract the matching extent from the extent it matches */
	rc = ext_subtract(layer->ccr_subobj, &layer->ccr_rd_exts, match,
			  false, &layer_empty);
	if (rc)
		RETURN(rc);
	INFO("Extent [%#"PRIx64"-%#"PRIx64"] (gen %d) successfully released "
	     "from tier %d\n", match->off, match->off + match->len - 1,
	     gen, hsm_prio2tier(layer->ccr_priority));

	/* Drop the layer if it no longer has readable extents */
	if (layer_empty) {
		VERB("No remaining extent in layer: deleting "
		    "subobject <%"PRIx64":%"PRIx64">\n",
		    layer->ccr_subobj.u_hi, layer->ccr_subobj.u_lo);

		/* The list of extent has not been updated yet => no_check */
		rc = layer_clean(ctx->obj_id, layout, layer);
		if (rc)
			RETURN(rc);
	}

	/** TODO actually release the data region */
	RETURN(rc);
}

/** same as release, but precise max generation */
static int m0hsm_release_maxgen(struct m0_uint128 id, uint8_t tier, int max_gen,
				off_t offset, size_t len,
				enum hsm_rls_flags flags,
				bool user_display)
{
	struct m0_client_layout	*layout = NULL;
	struct release_cb_ctx ctx = {0};
	struct extent ext;
	int rc;
	ENTRY;

	/* get layout once for all, it will be useful in next steps */
	rc = layout_get(id, &layout);
	if (rc)
		RETURN(rc);
	M0_ASSERT(layout != NULL);

	ext.off = offset;
	ext.len = len;

	ctx.obj_id = id;
	ctx.tier = tier;
	ctx.max_gen = max_gen;
	ctx.max_tier = tier;

	rc = match_layer_foreach(layout, tier, &ext, release_cb, &ctx, false);
	if (rc == 0 && ctx.found == 0 && user_display)
		ERROR("No matching extent found\n");
	RETURN(rc);
}

int m0hsm_release(struct m0_uint128 id, uint8_t tier,
	          off_t offset, size_t len, enum hsm_rls_flags flags)
{
	return m0hsm_release_maxgen(id, tier, -1, offset, len, flags, true);
}

int m0hsm_multi_release(struct m0_uint128 id, uint8_t max_tier,
			off_t offset, size_t length, enum hsm_rls_flags flags)
{
	struct m0_client_layout	*layout = NULL;
	struct release_cb_ctx ctx = {0};
	struct extent ext;
	int rc;
	ENTRY;

	/* get layout once for all, it will be useful in next steps */
	rc = layout_get(id, &layout);
	if (rc)
		RETURN(rc);
	M0_ASSERT(layout != NULL);

	ext.off = offset;
	ext.len = length;

	ctx.obj_id = id;
	ctx.tier = HSM_ANY_TIER;
	ctx.max_gen = -1;
	ctx.max_tier = max_tier;

	rc = match_layer_foreach(layout, HSM_ANY_TIER, &ext, release_cb, &ctx,
				false);
	if (rc == 0 && ctx.found == 0)
		ERROR("No matching extent found\n");
	RETURN(rc);
}

struct copy_cb_ctx {
	int		  found;
	struct m0_uint128 obj_id;
	uint8_t		  src_tier;
	uint8_t		  tgt_tier;
	enum hsm_cp_flags flags;
};

/**
 * This callback is called for each match extent in the source tier
 */
static int copy_cb(void *cb_arg, struct m0_client_layout *layout,
	           struct m0_composite_layer *src_layer,
		   const struct extent *match,
		   bool *stop)
{
	struct m0_composite_layer *tgt_layer;
	struct m0_obj subobj = {};
	struct m0_uint128 subobj_id;
	int tgt_prio, w_prio;
	uint8_t w_tier;
	int gen;
	int rc;
	struct copy_cb_ctx *ctx = cb_arg;
	ENTRY;

	ctx->found++;

	/* compute the generation of both source and target */
	gen = hsm_prio2gen(src_layer->ccr_priority);
	tgt_prio = hsm_prio(gen, ctx->tgt_tier);
	/* Most prioritary of src and tgt (the lower value, the higher priority) */
	w_prio = MIN(src_layer->ccr_priority, tgt_prio);

	INFO("%s extent [%#"PRIx64"-%#"PRIx64"] (gen %u) from "
		"tier %u to tier %u\n",
		ctx->src_tier > ctx->tgt_tier ? "Staging" : "Archiving",
		match->off, match->off + match->len - 1,
		gen, ctx->src_tier, ctx->tgt_tier);

	/* Make sure there is a writable extent with higher generation
	 * (must prevent from modifications in both source and target). */
	if (ctx->flags & HSM_WRITE_TO_DEST) {
		/* write must now be directed to the target tier */
		w_tier = ctx->tgt_tier;
		rc = check_top_layer_writable(layout, w_prio, w_tier);
	} else {
		/* write must still be directed to the original tier */
		rc = check_top_layer_writable(layout, w_prio, HSM_ANY_TIER);
		/* write to source tier if a new layer is to be created */
		w_tier = ctx->src_tier;
	}

	if (rc == -ENOENT) {
		/* If not, create one */
		rc = layout_add_top_layer(ctx->obj_id, layout, w_tier);
		if (rc)
			RETURN(rc);
	}

	/* 2: Check if the target subobject already exists */
	tgt_layer = layer_get_by_prio(layout, tgt_prio);
	if (!tgt_layer) {
		/* if not, create it */
		subobj_id = hsm_subobj_id(ctx->obj_id, gen, ctx->tgt_tier);

		/* create a sub-object that will be the first layer */
		rc = create_obj(subobj_id, &subobj, false, ctx->tgt_tier);
		if (rc)
			RETURN(rc);
	} else {
		struct extent already_ext;

		DBG("Target layer already exists\n");
		subobj_id = tgt_layer->ccr_subobj;

		/* check if the given segment is already present */
		layer_load_extent_list(tgt_layer->ccr_subobj, false,
			       &tgt_layer->ccr_rd_exts);

		switch (ext_match(tgt_layer->ccr_subobj,
			   &tgt_layer->ccr_rd_exts, match, &already_ext,
			   EMT_INTERSECT, false, false))
		{
		case EM_ERROR:
			ERROR("Extent matching error\n");
			break;
		case EM_NONE:
			VERB("No previous copy of this extent\n");
			break;
		case EM_PARTIAL:
			VERB("Extent already been partially copied to target tier\n");
			/* TODO only copy missing parts */
			break;
		case EM_FULL:
			/* TODO implement 'force' copy? */
			VERB("Extent has already been "
			     "copied to target tier: nothing to do\n");
			goto release;
		}
	}

	/* 3: copy data (from subojbect to subobject) */
	rc = copy_extent_data(src_layer->ccr_subobj, subobj_id, match);
	if (rc) {
		ERROR("copy_extent_data() failed: rc=%d\n", rc);
		goto fini;
	}

	/* Add the target extent (readable) */
	if (tgt_layer == NULL) {
		/* no previous extent */
		rc = layer_extent_add(subobj_id, match, false, false);
		if (rc) {
			ERROR("layer_extent_add() failed: rc=%d\n", rc);
			goto fini;
		}

		/* Finally, add the subobject to the composite layout (if it was new).
		 * XXX Is there a need to do this before the copy? perhaps to archive
		 * multiple extents in parallel. */
		DBG("Adding subobj <%"PRIx64":%"PRIx64"> as layer with prio %#x\n",
			subobj_id.u_hi, subobj_id.u_lo, tgt_prio);
		m0_composite_layer_add(layout, &subobj, tgt_prio);

		rc = layout_set(ctx->obj_id, layout);
		if (rc) {
			ERROR("layout_set() failed: rc=%d\n", rc);
			goto fini;
		}
	} else {
		/* else: merge with previous extents */
		rc = add_merge_read_extent(tgt_layer, match);
		if (rc) {
			ERROR("add_merge_read_extent() failed: rc=%d\n", rc);
			goto fini;
		}
	}

 release:
	/* 4: release source extent, if requested */
	if (ctx->flags & HSM_MOVE) {
		struct release_cb_ctx rls_ctx = {0};
		bool dummy;

		rls_ctx.obj_id = ctx->obj_id;
		rls_ctx.tier = ctx->src_tier;
		/* max gen is its gen */
		rls_ctx.max_gen = gen;
		rls_ctx.max_tier = ctx->src_tier;

		VERB("Releasing extent [%#"PRIx64"-%#"PRIx64"] in tier %d\n",
			match->off, match->off + match->len - 1,
			ctx->src_tier);
		/* Release needs to operate with an updated target layer,
		 * else it will not allow to release source extent. */
		if (tgt_layer != NULL)
			extent_list_free(&tgt_layer->ccr_rd_exts);
		rc = release_cb(&rls_ctx, layout, src_layer, match, &dummy);
		if (rc) {
			ERROR("release_cb() failed: rc=%d\n", rc);
			goto fini;
		}
	}

	/* Drop previous version on target tier.
	 * If gen is 0, no previous version is supposed to exist. */
	if (!(ctx->flags & HSM_KEEP_OLD_VERS) && gen > 0) {
		VERB("Releasing extents [%#"PRIx64"-%#"PRIx64"] in tier %d, with gen <= %d\n",
			match->off, match->off + match->len - 1,
			ctx->tgt_tier, gen - 1);
		rc = m0hsm_release_maxgen(ctx->obj_id, ctx->tgt_tier,
					  gen - 1, match->off, match->len, 0,
					  false);
		if (rc) {
			ERROR("m0hsm_release_maxgen() failed: rc=%d\n", rc);
			goto fini;
		}
	}
 fini:
	if (tgt_layer == NULL)
		m0_entity_fini(&subobj.ob_entity);

	RETURN(rc);
}

int m0hsm_copy(struct m0_uint128 id, uint8_t src_tier, uint8_t tgt_tier,
	      off_t offset, size_t len, enum hsm_cp_flags flags)
{
	struct m0_client_layout	*layout = NULL;
	struct copy_cb_ctx ctx = {0};
	struct extent ext;
	int rc;

	ENTRY;

	/* get layout once for all, it will be useful in next steps */
	rc = layout_get(id, &layout);
	if (rc)
		RETURN(rc);
	M0_ASSERT(layout != NULL);

	/* prepare context to be passed to callback functions */
	ctx.obj_id = id;
	ctx.src_tier = src_tier;
	ctx.tgt_tier = tgt_tier;
	ctx.flags = flags;

	/* Check there is matching data in the source tier and get
	 * the corresponding layer */
	ext.off = offset;
	ext.len = len;

	rc = match_layer_foreach(layout, src_tier, &ext, copy_cb, &ctx, false);
	if (rc == 0 && ctx.found == 0)
		ERROR("No matching extent found\n");
	RETURN(rc);
}

/**
 * This callback is called for eaching match extent for staging
 */
static int stage_cb(void *cb_arg, struct m0_client_layout *layout,
	           struct m0_composite_layer *src_layer,
		   const struct extent *match,
		   bool *stop)
{
	struct copy_cb_ctx *ctx = cb_arg;
	uint8_t tier;
	int rc;
	ENTRY;

	/* check if the tier is > target tier */
	tier = hsm_prio2tier(src_layer->ccr_priority);
	if (tier <= ctx->tgt_tier)
		/* skip it */
		return 0;

	/* set source tier for copy_cb */
	ctx->src_tier = tier;
	rc = copy_cb(cb_arg, layout, src_layer, match, stop);
	RETURN(rc);
}

int m0hsm_stage(struct m0_uint128 id, uint8_t tgt_tier,
		off_t offset, size_t length, enum hsm_cp_flags flags)
{
	struct m0_client_layout	*layout = NULL;
	struct copy_cb_ctx ctx = {0};
	struct extent ext;
	int rc;

	/* prepare context to be passed to callback functions */
	ctx.obj_id = id;
	ctx.src_tier = HSM_ANY_TIER;
	ctx.tgt_tier = tgt_tier;
	ctx.flags = flags;

	ENTRY;

	/* get layout once for all, it will be useful in next steps */
	rc = layout_get(id, &layout);
	if (rc)
		RETURN(rc);
	M0_ASSERT(layout != NULL);

	ext.off = offset;
	ext.len = length;

	/* for each layer > target_tier, move the data to the target tier */
	rc = match_layer_foreach(layout, HSM_ANY_TIER, &ext, stage_cb,
				 &ctx, false);

	if (rc == 0 && ctx.found == 0)
		ERROR("No matching extent found\n");
	RETURN(rc);
}

/**
 * This callback is called for eaching match extent for archiving
 */
static int archive_cb(void *cb_arg, struct m0_client_layout *layout,
		      struct m0_composite_layer *src_layer,
		      const struct extent *match, bool *stop)
{
	struct copy_cb_ctx *ctx = cb_arg;
	uint8_t tier;
	int rc;
	ENTRY;

	/* check if the tier is < target tier */
	tier = hsm_prio2tier(src_layer->ccr_priority);
	if (tier >= ctx->tgt_tier)
		/* skip it */
		return 0;

	/* set source tier for copy_cb */
	ctx->src_tier = tier;
	rc = copy_cb(cb_arg, layout, src_layer, match, stop);
	RETURN(rc);
}

int m0hsm_archive(struct m0_uint128 id, uint8_t tgt_tier,
		off_t offset, size_t length, enum hsm_cp_flags flags)
{
	struct m0_client_layout	*layout = NULL;
	struct copy_cb_ctx ctx = {0};
	struct extent ext;
	int rc;

	/* prepare context to be passed to callback functions */
	ctx.obj_id = id;
	ctx.src_tier = HSM_ANY_TIER;
	ctx.tgt_tier = tgt_tier;
	ctx.flags = flags;

	ENTRY;

	/* get layout once for all, it will be useful in next steps */
	rc = layout_get(id, &layout);
	if (rc)
		RETURN(rc);
	M0_ASSERT(layout != NULL);

	ext.off = offset;
	ext.len = length;

	/* for each layer > target_tier, move the data to the target tier */
	rc = match_layer_foreach(layout, HSM_ANY_TIER, &ext, archive_cb,
				 &ctx, false);

	if (rc == 0 && ctx.found == 0)
		ERROR("No matching extent found\n");
	RETURN(rc);
}


/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
