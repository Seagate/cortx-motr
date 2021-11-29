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


#include "balloc/balloc.h"

#include "be/extmap.h"
#include "be/seg.h"
#include "be/seg0.h"		/* m0_be_0type */

#include "dtm/dtm.h"		/* m0_dtx */

#include "fid/fid.h"		/* m0_fid */

#include "lib/finject.h"
#include "lib/errno.h"
#include "lib/locality.h"	/* m0_locality0_get */
#include "lib/memory.h"
#include "lib/string.h"
#include "lib/cksum_utils.h"
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ADSTOB
#include "lib/trace.h"		/* M0_LOG */

#include "addb2/addb2.h"
#include "module/instance.h"	/* m0_get */

#include "stob/partition.h"
//#include "stob/ad_private.h"
//#include "stob/ad_private_xc.h"
#include "stob/addb2.h"
#include "stob/domain.h"
#include "stob/io.h"
#include "stob/module.h"	/* m0_stob_ad_module */
#include "stob/stob.h"
#include "stob/stob_internal.h"	/* m0_stob__fid_set */
#include "stob/type.h"		/* m0_stob_type */
#include "be/domain.h"

/**
 * @addtogroup stobpart
 *
 * @{
 */

static struct m0_stob_ops stob_ad_ops;

static int stob_part_io_init(struct m0_stob *stob, struct m0_stob_io *io);
static void stob_ad_write_credit(const struct m0_stob_domain *dom,
				 const struct m0_stob_io     *iv,
				 struct m0_be_tx_credit      *accum);
static void
stob_ad_rec_frag_undo_redo_op_cred(const struct m0_fol_frag *frag,
				   struct m0_be_tx_credit   *accum);
static int stob_ad_rec_frag_undo_redo_op(struct m0_fol_frag *frag,
					 struct m0_be_tx    *tx);

M0_FOL_FRAG_TYPE_DECLARE(stob_ad_rec_frag, static,
			 stob_ad_rec_frag_undo_redo_op,
			 stob_ad_rec_frag_undo_redo_op,
			 stob_ad_rec_frag_undo_redo_op_cred,
			 stob_ad_rec_frag_undo_redo_op_cred);
static int stob_ad_seg_free(struct m0_dtx *tx,
			    struct m0_stob_ad_domain *adom,
			    const struct m0_be_emap_seg *seg,
			    const struct m0_ext *ext,
			    uint64_t val);
static int stob_part_punch(struct m0_stob *stob,
			   struct m0_indexvec *range,
			   struct m0_dtx *tx);
#if 0
static int stob_ad_0type_init(struct m0_be_domain *dom,
			      const char *suffix,
			      const struct m0_buf *data)
{
	struct m0_stob_ad_module *module = &m0_get()->i_stob_ad_module;
	struct stob_ad_0type_rec *rec = data->b_addr;
	struct ad_domain_map     *ad;
	int                       rc;

	M0_PRE(rec != NULL && data->b_nob == sizeof(*rec));
	M0_PRE(strlen(suffix) < ARRAY_SIZE(ad->adm_path));

	M0_ALLOC_PTR(ad);
	rc = ad == NULL ? -ENOMEM : 0;

	if (rc == 0) {
		/* XXX won't be stored as pointer */
		ad->adm_dom = rec->sa0_ad_domain;
		strncpy(ad->adm_path, suffix, sizeof(ad->adm_path) - 1);
		m0_mutex_lock(&module->sam_lock);
		ad_domains_tlink_init_at_tail(ad, &module->sam_domains);
		m0_mutex_unlock(&module->sam_lock);
	}

	return M0_RC(rc);
}

static void stob_ad_0type_fini(struct m0_be_domain *dom,
			       const char *suffix,
			       const struct m0_buf *data)
{
	struct m0_stob_ad_module *module = &m0_get()->i_stob_ad_module;
	struct stob_ad_0type_rec *rec = data->b_addr;
	struct ad_domain_map     *ad;

	M0_PRE(rec != NULL && data->b_nob == sizeof(*rec));

	m0_mutex_lock(&module->sam_lock);
	ad = m0_tl_find(ad_domains, ad, &module->sam_domains,
			m0_streq(suffix, ad->adm_path));
	M0_ASSERT(ad != NULL);
	ad_domains_tlink_del_fini(ad);
	m0_free(ad);
	m0_mutex_unlock(&module->sam_lock);
}

struct m0_be_0type m0_stob_ad_0type = {
	.b0_name = "M0_BE:AD",
	.b0_init = stob_ad_0type_init,
	.b0_fini = stob_ad_0type_fini
};

M0_INTERNAL struct m0_stob_ad_domain *
stob_ad_domain2ad(const struct m0_stob_domain *dom)
{
	struct m0_stob_ad_domain *adom;

	adom = (struct m0_stob_ad_domain *)dom->sd_private;
	m0_stob_ad_domain_bob_check(adom);
	M0_ASSERT(m0_stob_domain__dom_key(m0_stob_domain_id_get(dom)) ==
		  adom->sad_dom_key);

	return adom;
}

M0_INTERNAL struct m0_balloc *
m0_stob_ad_domain2balloc(const struct m0_stob_domain *dom)
{
	struct m0_stob_ad_domain *adom = stob_ad_domain2ad(dom);

	return b2m0(adom->sad_ballroom);
}

static struct m0_stob_ad *stob_ad_stob2ad(const struct m0_stob *stob)
{
	return container_of(stob, struct m0_stob_ad, ad_stob);
}

static void stob_ad_type_register(struct m0_stob_type *type)
{
	struct m0_stob_ad_module *module = &m0_get()->i_stob_ad_module;
	int                       rc;

	M0_FOL_FRAG_TYPE_INIT(stob_ad_rec_frag, "AD record fragment");
	rc = m0_fol_frag_type_register(&stob_ad_rec_frag_type);
	M0_ASSERT(rc == 0); /* XXX void */
	m0_mutex_init(&module->sam_lock);
	ad_domains_tlist_init(&module->sam_domains);
}

static void stob_ad_type_deregister(struct m0_stob_type *type)
{
	struct m0_stob_ad_module *module = &m0_get()->i_stob_ad_module;

	ad_domains_tlist_fini(&module->sam_domains);
	m0_mutex_fini(&module->sam_lock);
	m0_fol_frag_type_deregister(&stob_ad_rec_frag_type);
}

M0_INTERNAL void m0_stob_ad_init_cfg_make(char **str, struct m0_be_domain *dom)
{
	char buf[0x40];

	snprintf(buf, ARRAY_SIZE(buf), "%p", dom);
	*str = m0_strdup(buf);
}

M0_INTERNAL void m0_stob_ad_cfg_make(char **str,
				     const struct m0_be_seg *seg,
				     const struct m0_stob_id *bstore_id,
				     const m0_bcount_t size)
{
	char buf[0x400];

	snprintf(buf, ARRAY_SIZE(buf), "%p:"FID_F":"FID_F":%"PRId64, seg,
		 FID_P(&bstore_id->si_domain_fid),
		 FID_P(&bstore_id->si_fid), size);
	*str = m0_strdup(buf);
}

static int stob_ad_domain_cfg_init_parse(const char *str_cfg_init,
					 void **cfg_init)
{
	struct ad_domain_init_cfg *cfg;
	int                        rc;

	M0_ASSERT(str_cfg_init != NULL); /* TODO: remove this assert */
	if (str_cfg_init == NULL)
		return M0_ERR(-EINVAL);

	M0_ALLOC_PTR(cfg);
	if (cfg == NULL)
		return M0_ERR(-ENOMEM);

	rc = sscanf(str_cfg_init, "%p", (void **)&cfg->dic_dom);
	*cfg_init = cfg;
	M0_ASSERT(rc == 1); /* TODO: remove this assert */
	return rc == 1 ? 0 : -EINVAL;
}

static void stob_ad_domain_cfg_init_free(void *cfg_init)
{
	m0_free(cfg_init);
}

static int stob_ad_domain_cfg_create_parse(const char *str_cfg_create,
					   void **cfg_create)
{
	struct ad_domain_cfg *cfg;
	m0_bcount_t           grp_blocks;
	int                   rc;

	if (str_cfg_create == NULL)
		return M0_ERR(-EINVAL);

	M0_ALLOC_PTR(cfg);
	if (cfg != NULL) {
		/* format = seg:domain_fid:fid:container_size */
		rc = sscanf(str_cfg_create, "%p:"FID_SF":"FID_SF":%"SCNd64"",
			    (void **)&cfg->adg_seg,
			    FID_S(&cfg->adg_id.si_domain_fid),
			    FID_S(&cfg->adg_id.si_fid),
			    &cfg->adg_container_size);
		rc = rc == 6 ? 0 : -EINVAL;
	} else
		rc = -ENOMEM;

	if (rc == 0) {
		if (cfg->adg_container_size == 0)
			cfg->adg_container_size = BALLOC_DEF_CONTAINER_SIZE;
		cfg->adg_bshift     = BALLOC_DEF_BLOCK_SHIFT;
		/*
		 * Big number of groups slows balloc initialisation. Therefore,
		 * group size is counted depending on BALLOC_DEF_GROUPS_NR.
		 * Group size must be power of 2.
		 */
		grp_blocks = (cfg->adg_container_size >> cfg->adg_bshift) /
			     BALLOC_DEF_GROUPS_NR;
		grp_blocks = 1 << m0_log2(grp_blocks);
		grp_blocks = max64u(grp_blocks, BALLOC_DEF_BLOCKS_PER_GROUP);
		cfg->adg_blocks_per_group = grp_blocks;
		cfg->adg_spare_blocks_per_group =
			m0_stob_ad_spares_calc(grp_blocks);
		M0_LOG(M0_DEBUG, "device size %"PRId64, cfg->adg_container_size);
		*cfg_create = cfg;
	}
	return M0_RC(rc);
}

/*
 * XXX @todo: A more sophisticated version of this function is necessary,
 * that will take into account the number of pool versions that the disk
 * belongs to, along with parameters of pdclust. The following module will
 * return a value around 20 % of blocks per group.
 *
 * On other note, reserving a fraction K / (N + K) is too conservative as
 * it takes into consideration the case when on failure all parity groups
 * need to be repaired (which is true only when N + 2K == P).
 * Probably K / P is the  right ratio.
 */
M0_INTERNAL m0_bcount_t m0_stob_ad_spares_calc(m0_bcount_t grp_blocks)
{
#ifdef __SPARE__SPACE__

	return  grp_blocks % 5 == 0 ? grp_blocks / 5 : grp_blocks / 5 + 1;
#else
	return 0;
#endif
}

/* This function will go through si_stob vector
 * Checksum is stored in contigious buffer: si_cksum, while COB extents may not be
 * contigious e.g.
 * Assuming each extent has two DU, so two checksum.
 *     | CS0 | CS1 | CS2 | CS3 | CS4 | CS5 | CS6 |
 *     | iv_index[0] |      | iv_index[1] | iv_index[2] |     | iv_index[3] |
 * Now if we have an offset for CS3 then after first travesal b_addr will poin to
 * start of CS2 and then it will land in m0_ext_is_in and will compute correct
 * addr for CS3.
 */
#endif
M0_INTERNAL void * m0_stob_ad_get_checksum_addr(struct m0_stob_io *io, m0_bindex_t off )
{
	void         *b_addr = io->si_cksum.b_addr;
	void         *cksum_addr = NULL;
	struct m0_ext ext;
	int           i;

	/* Get the checksum nobs consumed till reaching the off in given io */
	for (i = 0; i < io->si_stob.iv_vec.v_nr; i++)
	{
		ext.e_start = io->si_stob.iv_index[i];
		ext.e_end = io->si_stob.iv_index[i] + io->si_stob.iv_vec.v_count[i];

		if (m0_ext_is_in(&ext, off)) {
			cksum_addr = m0_extent_get_checksum_addr(b_addr, off,
								 ext.e_start,
								 io->si_unit_sz,
								 io->si_cksum_sz);
			break;
		}
		else {
			/* off is beyond the current extent, increment the b_addr */
			b_addr +=  m0_extent_get_checksum_nob(ext.e_start,
					io->si_stob.iv_vec.v_count[i], io->si_unit_sz, io->si_cksum_sz);
			M0_ASSERT(b_addr <=io->si_cksum.b_addr + io->si_cksum.b_nob  );
		}
	}

	/** TODO: Enable this once PARITY support is added ? */
	/** M0_ASSERT(cksum_addr != NULL); */
	return cksum_addr;
}
#if 0
static void stob_ad_domain_cfg_create_free(void *cfg_create)
{
	m0_free(cfg_create);
}

M0_INTERNAL bool m0_stob_ad_domain__invariant(struct m0_stob_ad_domain *adom)
{
	return _0C(adom->sad_ballroom != NULL);
}

static struct m0_sm_group *stob_ad_sm_group(void)
{
	return m0_locality0_get()->lo_grp;
}

static int stob_ad_bstore(struct m0_stob_id *stob_id, struct m0_stob **out)
{
	struct m0_stob *stob;
	int		rc;

	rc = m0_stob_find(stob_id, &stob);
	if (rc == 0) {
		if (m0_stob_state_get(stob) == CSS_UNKNOWN)
			rc = m0_stob_locate(stob);
		if (rc != 0 || m0_stob_state_get(stob) != CSS_EXISTS) {
			m0_stob_put(stob);
			rc = rc ?: -ENOENT;
		}
	}
	*out = rc == 0 ? stob : NULL;
	return M0_RC(rc);
}

static struct m0_stob_ad_domain *
stob_ad_domain_locate(const char *location_data)
{
	struct m0_stob_ad_module *module = &m0_get()->i_stob_ad_module;
	struct ad_domain_map *ad;

	m0_mutex_lock(&module->sam_lock);
	ad = m0_tl_find(ad_domains, ad, &module->sam_domains,
			m0_streq(location_data, ad->adm_path));
	m0_mutex_unlock(&module->sam_lock);
	return ad == NULL ? NULL : ad->adm_dom;
}

static int stob_ad_domain_init(struct m0_stob_type *type,
			       const char *location_data,
			       void *cfg_init,
			       struct m0_stob_domain **out)
{
	struct ad_domain_init_cfg *cfg = cfg_init;
	struct m0_stob_ad_domain  *adom;
	struct m0_stob_domain     *dom;
	struct m0_be_seg          *seg;
	struct m0_ad_balloc       *ballroom;
	bool                       balloc_inited;
	int                        rc = 0;

	adom = stob_ad_domain_locate(location_data);
	if (adom == NULL)
		return M0_RC(-ENOENT);
	else
		seg = m0_be_domain_seg(cfg->dic_dom, adom);

	if (seg == NULL) {
		M0_LOG(M0_ERROR, "segment doesn't exist for addr=%p", adom);
		return M0_ERR(-EINVAL);
	}

	M0_ASSERT(m0_stob_ad_domain__invariant(adom));

	M0_ALLOC_PTR(dom);
	if (dom == NULL)
		return M0_ERR(-ENOMEM);

	m0_stob_domain__dom_id_make(&dom->sd_id,
				    m0_stob_type_id_get(type),
				    0, adom->sad_dom_key);
	dom->sd_private = adom;
	dom->sd_ops     = &stob_ad_domain_ops;
	m0_be_emap_init(&adom->sad_adata, seg);

	ballroom = adom->sad_ballroom;
	m0_balloc_init(b2m0(ballroom));
	rc = ballroom->ab_ops->bo_init(ballroom, seg,
				       adom->sad_bshift,
				       adom->sad_container_size,
				       adom->sad_blocks_per_group,
#ifdef __SPARE_SPACE__
				       adom->sad_spare_blocks_per_group);
#else
					0);
#endif
	balloc_inited = rc == 0;

	rc = rc ?: stob_ad_bstore(&adom->sad_bstore_id,
				  &adom->sad_bstore);
	if (rc != 0) {
		if (balloc_inited)
			ballroom->ab_ops->bo_fini(ballroom);
		m0_be_emap_fini(&adom->sad_adata);
		m0_free(dom);
	} else {
		m0_stob_ad_domain_bob_init(adom);
		adom->sad_be_seg   = seg;
		adom->sad_babshift = adom->sad_bshift -
				m0_stob_block_shift(adom->sad_bstore);
		M0_LOG(M0_DEBUG, "sad_bshift = %lu\tstob bshift=%lu",
		       (unsigned long)adom->sad_bshift,
		       (unsigned long)m0_stob_block_shift(adom->sad_bstore));
		M0_ASSERT(adom->sad_babshift >= 0);
	}

	if (rc == 0)
		*out = dom;
	return rc == 0 ? M0_RC(rc) : M0_ERR(rc);
}

static void stob_ad_domain_fini(struct m0_stob_domain *dom)
{
	struct m0_stob_ad_domain *adom = stob_ad_domain2ad(dom);
	struct m0_ad_balloc      *ballroom = adom->sad_ballroom;

	ballroom->ab_ops->bo_fini(ballroom);
	m0_be_emap_fini(&adom->sad_adata);
	m0_stob_put(adom->sad_bstore);
	m0_stob_ad_domain_bob_fini(adom);
	m0_free(dom);
}

static void stob_ad_domain_create_credit(struct m0_be_seg *seg,
					 const char *location_data,
					 struct m0_be_tx_credit *accum)
{
	struct m0_be_emap map = {};
	struct m0_buf     data = { .b_nob = sizeof(struct stob_ad_0type_rec) };

	M0_BE_ALLOC_CREDIT_PTR((struct m0_stob_ad_domain *)NULL, seg, accum);
	m0_be_emap_init(&map, seg);
	m0_be_emap_credit(&map, M0_BEO_CREATE, 1, accum);
	m0_be_emap_fini(&map);
	m0_be_0type_add_credit(seg->bs_domain, &m0_stob_ad_0type,
			       location_data, &data, accum);
}

static void stob_ad_domain_destroy_credit(struct m0_be_seg *seg,
					  const char *location_data,
					  struct m0_be_tx_credit *accum)
{
	struct m0_be_emap map = {};

	M0_BE_FREE_CREDIT_PTR((struct m0_stob_ad_domain *)NULL, seg, accum);
	m0_be_emap_init(&map, seg);
	m0_be_emap_credit(&map, M0_BEO_DESTROY, 1, accum);
	m0_be_emap_fini(&map);
	m0_be_0type_del_credit(seg->bs_domain, &m0_stob_ad_0type,
			       location_data, accum);
}

/* TODO Make cleanup on fail. */
static int stob_ad_domain_create(struct m0_stob_type *type,
				 const char *location_data,
				 uint64_t dom_key,
				 void *cfg_create)
{
	struct ad_domain_cfg     *cfg = (struct ad_domain_cfg *)cfg_create;
	struct m0_be_seg         *seg = cfg->adg_seg;
	struct m0_sm_group       *grp = stob_ad_sm_group();
	struct m0_stob_ad_domain *adom;
	struct m0_be_emap        *emap;
	struct m0_balloc         *cb = NULL;
	struct m0_be_tx           tx = {};
	struct m0_be_tx_credit    cred = M0_BE_TX_CREDIT(0, 0);
	struct stob_ad_0type_rec  seg0_ad_rec;
	struct m0_buf             seg0_data;
	int                       rc;

	M0_PRE(seg != NULL);
	M0_PRE(strlen(location_data) < ARRAY_SIZE(adom->sad_path));

	adom = stob_ad_domain_locate(location_data);
	if (adom != NULL)
		return M0_ERR(-EEXIST);

	m0_sm_group_lock(grp);
	m0_be_tx_init(&tx, 0, seg->bs_domain, grp, NULL, NULL, NULL, NULL);
	stob_ad_domain_create_credit(seg, location_data, &cred);
	m0_be_tx_prep(&tx, &cred);
	/* m0_balloc_create() makes own local transaction thereby must be called
	 * before openning of exclusive transaction. m0_balloc_destroy() is not
	 * implemented, so balloc won't be cleaned up on a further fail.
	 */
	rc = m0_balloc_create(dom_key, seg, grp, &cb,
			      &cfg->adg_id.si_fid);
	rc = rc ?: m0_be_tx_exclusive_open_sync(&tx);

	M0_ASSERT(adom == NULL);
	if (rc == 0)
		M0_BE_ALLOC_PTR_SYNC(adom, seg, &tx);
	if (adom != NULL) {
		m0_format_header_pack(&adom->sad_header, &(struct m0_format_tag){
			.ot_version = M0_STOB_AD_DOMAIN_FORMAT_VERSION,
			.ot_type    = M0_FORMAT_TYPE_STOB_AD_DOMAIN,
			.ot_footer_offset =
				offsetof(struct m0_stob_ad_domain, sad_footer)
		});
		adom->sad_dom_key          = dom_key;
		adom->sad_container_size   = cfg->adg_container_size;
		adom->sad_bshift           = cfg->adg_bshift;
		adom->sad_blocks_per_group = cfg->adg_blocks_per_group;
#ifdef __SPARE_SPACE__
		adom->sad_spare_blocks_per_group =
			cfg->adg_spare_blocks_per_group;
#endif
		adom->sad_bstore_id        = cfg->adg_id;
		adom->sad_overwrite        = false;
		strcpy(adom->sad_path, location_data);
		m0_format_footer_update(adom);
		emap = &adom->sad_adata;
		m0_be_emap_init(emap, seg);
		rc = M0_BE_OP_SYNC_RET(
			op,
			m0_be_emap_create(emap, &tx, &op,
					  &cfg->adg_id.si_fid),
			bo_u.u_emap.e_rc);
		m0_be_emap_fini(emap);

		seg0_ad_rec = (struct stob_ad_0type_rec){.sa0_ad_domain = adom}; /* XXX won't be a pointer */
		m0_format_header_pack(&seg0_ad_rec.sa0_header, &(struct m0_format_tag){
			.ot_version = M0_STOB_AD_0TYPE_REC_FORMAT_VERSION,
			.ot_type    = M0_FORMAT_TYPE_STOB_AD_0TYPE_REC,
			.ot_footer_offset = offsetof(struct stob_ad_0type_rec, sa0_footer)
		});
		m0_format_footer_update(&seg0_ad_rec);
		seg0_data   = M0_BUF_INIT_PTR(&seg0_ad_rec);
		rc = rc ?: m0_be_0type_add(&m0_stob_ad_0type, seg->bs_domain,
					   &tx, location_data, &seg0_data);
		if (rc == 0) {
			adom->sad_ballroom = &cb->cb_ballroom;
			m0_format_footer_update(adom);
			M0_BE_TX_CAPTURE_PTR(seg, &tx, adom);
		}

		m0_be_tx_close_sync(&tx);
	}

	m0_be_tx_fini(&tx);
	m0_sm_group_unlock(grp);

	if (adom == NULL && rc == 0)
		rc = M0_ERR(-ENOMEM);

	return M0_RC(rc);
}

static int stob_ad_domain_destroy(struct m0_stob_type *type,
				  const char *location_data)
{
	struct m0_stob_ad_domain *adom = stob_ad_domain_locate(location_data);
	struct m0_sm_group       *grp  = stob_ad_sm_group();
	struct m0_be_emap        *emap = &adom->sad_adata;
	struct m0_be_seg         *seg;
	struct m0_be_tx           tx   = {};
	struct m0_be_tx_credit    cred = M0_BE_TX_CREDIT(0, 0);
	int                       rc;

	if (adom == NULL)
		return 0;

	seg = adom->sad_be_seg;
	m0_sm_group_lock(grp);
	m0_be_tx_init(&tx, 0, seg->bs_domain, grp, NULL, NULL, NULL, NULL);
	stob_ad_domain_destroy_credit(seg, location_data, &cred);
	m0_be_tx_prep(&tx, &cred);
	rc = m0_be_tx_exclusive_open_sync(&tx);
	if (rc == 0) {
		m0_be_emap_init(emap, seg);
		rc = M0_BE_OP_SYNC_RET(op, m0_be_emap_destroy(emap, &tx, &op),
				       bo_u.u_emap.e_rc);
		rc = rc ?: m0_be_0type_del(&m0_stob_ad_0type, seg->bs_domain,
					   &tx, location_data);
		if (rc == 0)
			M0_BE_FREE_PTR_SYNC(adom, seg, &tx);
		m0_be_tx_close_sync(&tx);
	}
	m0_be_tx_fini(&tx);
	m0_sm_group_unlock(grp);

	/* m0_balloc_destroy() isn't implemented */

	return M0_RC(rc);
}

static struct m0_stob *stob_ad_alloc(struct m0_stob_domain *dom,
				     const struct m0_fid *stob_fid)
{
	struct m0_stob_ad *adstob;

	M0_ALLOC_PTR(adstob);
	return adstob == NULL ? NULL : &adstob->ad_stob;
}

static void stob_ad_free(struct m0_stob_domain *dom,
			 struct m0_stob *stob)
{
	struct m0_stob_ad *adstob = stob_ad_stob2ad(stob);
	m0_free(adstob);
}

static int stob_ad_cfg_parse(const char *str_cfg_create, void **cfg_create)
{
	return 0;
}

static void stob_ad_cfg_free(void *cfg_create)
{
}

static int stob_ad_init(struct m0_stob *stob,
			struct m0_stob_domain *dom,
			const struct m0_fid *stob_fid)
{
	struct m0_stob_ad_domain *adom = stob_ad_domain2ad(dom);
	struct m0_be_emap_cursor  it = {};
	struct m0_uint128         prefix;
	int                       rc;

	prefix = M0_UINT128(stob_fid->f_container, stob_fid->f_key);
	M0_LOG(M0_DEBUG, U128X_F, U128_P(&prefix));
	stob->so_ops = &stob_ad_ops;
	rc = M0_BE_OP_SYNC_RET_WITH(
		&it.ec_op,
		m0_be_emap_lookup(&adom->sad_adata, &prefix, 0, &it),
		bo_u.u_emap.e_rc);
	if (rc == 0) {
		m0_be_emap_close(&it);
	}
	return rc == -ESRCH ? -ENOENT : rc;
}
#endif
static void stob_part_fini(struct m0_stob *stob)
{
}
#if 0
static void stob_ad_create_credit(struct m0_stob_domain *dom,
				  struct m0_be_tx_credit *accum)
{
	struct m0_stob_ad_domain *adom = stob_ad_domain2ad(dom);
	m0_be_emap_credit(&adom->sad_adata, M0_BEO_INSERT, 1, accum);
}

static int stob_ad_create(struct m0_stob *stob,
			  struct m0_stob_domain *dom,
			  struct m0_dtx *dtx,
			  const struct m0_fid *stob_fid,
			  void *cfg)
{
	struct m0_stob_ad_domain *adom = stob_ad_domain2ad(dom);
	struct m0_uint128         prefix;

	M0_PRE(dtx != NULL);
	prefix = M0_UINT128(stob_fid->f_container, stob_fid->f_key);
	M0_LOG(M0_DEBUG, U128X_F, U128_P(&prefix));
	return M0_BE_OP_SYNC_RET(op,
				 m0_be_emap_obj_insert(&adom->sad_adata,
						       &dtx->tx_betx, &op,
						       &prefix, AET_HOLE),
				 bo_u.u_emap.e_rc);
}
#endif

/**
 * Function to calculate credit required for punch operation.
 * Iterate the stob extents from the want indexvec which provides range for
 * punch and accumulate the credits required.
 * Once all extents are iterated, @got indexvec contains granted credits that
 * are used for punch operation.
 * The credits are calculated only for the range of segments provided by
 * user which is a vector of extents to be punched. If not all but part of the
 * credits are granted, user is responsible to handle the transaction break and
 * request for the remaining credits in the next transaction to complete the
 * corresponding punch operation.
 * Note: It is the responsibility of user to free @got indexvec.
 *
 * @param want  range provided by the user.
 * @param got   range for which credits are granted.
 */
static int stob_part_punch_credit(struct m0_stob *stob,
				  struct m0_indexvec *want,
				  struct m0_indexvec *got,
				  struct m0_be_tx_credit *accum)
{
	return M0_RC(0);
}

static void stob_part_destroy_credit(struct m0_stob *stob,
				     struct m0_be_tx_credit *accum)
{
}

static int stob_part_destroy(struct m0_stob *stob, struct m0_dtx *tx)
{
	return M0_RC(0);
}

static int stob_part_punch(struct m0_stob *stob,
			   struct m0_indexvec *range,
			   struct m0_dtx *tx)
{
	return M0_RC(0);
}

static uint32_t stob_part_block_shift(struct m0_stob *stob)
{
	struct m0_stob_ad_domain *adom;

	adom = stob_ad_domain2ad(m0_stob_dom_get(stob));
	return m0_stob_block_shift(adom->sad_bstore);
}

static struct m0_stob_domain_ops stob_ad_domain_ops = {
	.sdo_fini		= &stob_ad_domain_fini,
	.sdo_stob_alloc	    	= &stob_ad_alloc,
	.sdo_stob_free	    	= &stob_ad_free,
	.sdo_stob_cfg_parse 	= &stob_ad_cfg_parse,
	.sdo_stob_cfg_free  	= &stob_ad_cfg_free,
	.sdo_stob_init	    	= &stob_ad_init,
	.sdo_stob_create_credit	= &stob_ad_create_credit,
	.sdo_stob_create	= &stob_ad_create,
	.sdo_stob_write_credit	= &stob_ad_write_credit,
};

static struct m0_stob_ops stob_part_ops = {
	.sop_fini            = &stob_part_fini,
	.sop_destroy_credit  = &stob_part_destroy_credit,
	.sop_destroy         = &stob_part_destroy,
	.sop_punch_credit    = &stob_part_punch_credit,
	.sop_punch           = &stob_part_punch,
	.sop_io_init         = &stob_part_io_init,
	.sop_block_shift     = &stob_part_block_shift,
};

const struct m0_stob_type m0_stob_part_type = {
	.st_ops  = &stob_part_type_ops,
	.st_fidt = {
		.ft_id   = STOB_TYPE_PARTITION,
		.ft_name = "partitionstob",
	},
};

/*
 * Adieu
 */

static const struct m0_stob_io_op stob_part_io_op;

static bool stob_part_endio(struct m0_clink *link);
static void stob_part_io_release(struct m0_stob_ad_io *aio);

static int stob_part_io_init(struct m0_stob *stob, struct m0_stob_io *io)
{
	struct m0_stob_part_io *pio;
	int                     rc;

	M0_PRE(io->si_state == SIS_IDLE);

	M0_ALLOC_PTR(pio);
	if (pio != NULL) {
		io->si_stob_private = pio;
		io->si_op = &stob_part_io_op;
		pio->pi_fore = io;
		m0_stob_io_init(&pio->pi_back);
		m0_clink_init(&pio->pi_clink, &stob_part_endio);
		m0_clink_add_lock(&pio->pi_back.si_wait, &pio->pi_clink);
		rc = 0;
	} else {
		rc = M0_ERR(-ENOMEM);
	}
	return M0_RC(rc);
}

static void stob_part_io_fini(struct m0_stob_io *io)
{
	struct m0_stob_part_io *pio = io->si_stob_private;
	stob_part_io_release(pio);
	m0_clink_del_lock(&pio->pi_clink);
	m0_clink_fini(&pio->pi_clink);
	m0_stob_io_fini(&pio->pi_back);
	m0_free(pio);
}

/**
   Releases vectors allocated for back IO.

   @note that back->si_stob.ov_vec.v_count is _not_ freed separately, as it is
   aliased to back->si_user.z_bvec.ov_vec.v_count.

   @see ad_vec_alloc()
 */
static void stob_part_io_release(struct m0_stob_part_io *pio)
{
	struct m0_stob_io *back = &pio->pi_back;

	M0_ASSERT(back->si_user.ov_vec.v_count == back->si_stob.iv_vec.v_count);
	m0_free0(&back->si_user.ov_vec.v_count);
	back->si_stob.iv_vec.v_count = NULL;

	m0_free0(&back->si_user.ov_buf);
	m0_free0(&back->si_stob.iv_index);

	back->si_obj = NULL;
}


/**
   Allocates back IO buffers after number of fragments has been calculated.

   @see stob_part_io_release()
 */
static int stob_part_vec_alloc(struct m0_stob    *obj,
			       struct m0_stob_io *back,
			       uint32_t           frags)
{
	m0_bcount_t *counts;
	int          rc = 0;

	M0_ASSERT(back->si_user.ov_vec.v_count == NULL);

	if (frags > 0) {
		M0_ALLOC_ARR(counts, frags);
		back->si_user.ov_vec.v_count = counts;
		back->si_stob.iv_vec.v_count = counts;
		M0_ALLOC_ARR(back->si_user.ov_buf, frags);
		M0_ALLOC_ARR(back->si_stob.iv_index, frags);

		back->si_user.ov_vec.v_nr = frags;
		back->si_stob.iv_vec.v_nr = frags;

		if (counts == NULL || back->si_user.ov_buf == NULL ||
		    back->si_stob.iv_index == NULL) {
			m0_free(counts);
			m0_free(back->si_user.ov_buf);
			m0_free(back->si_stob.iv_index);
			rc = M0_ERR(-ENOMEM);
		}
	}
	return M0_RC(rc);
}

/**
 * Constructs back IO for read.
 *
 * This is done in two passes:
 *
 *     - first, calculate number of fragments, taking holes into account. This
 *       pass iterates over user buffers list (src), target extents list (dst)
 *       and extents map (map). Once this pass is completed, back IO vectors can
 *       be allocated;
 *
 *     - then, iterate over the same sequences again. For holes, call memset()
 *       immediately, for other fragments, fill back IO vectors with the
 *       fragment description.
 *
 * @note assumes that allocation data can not change concurrently.
 *
 * @note memset() could become a bottleneck here.
 *
 * @note cursors and fragment sizes are measured in blocks.
 */
static int stob_part_read_prepare(struct m0_stob_io *io)
{
	struct m0_stob_io        *back;
	struct m0_stob_part_io   *pio = io->si_stob_private;
	int                       rc;

	M0_PRE(io->si_opcode == SIO_READ);

	back   = &pio->pi_back;
	rc = stob_part_vec_alloc(io->si_obj,
				 back,
				 io->si_stob.iv_vec.v_nr);
	if (rc != 0)
		return M0_RC(rc);

	stob_part_back_fill(io, back);

	return M0_RC(rc);
}

/**
   Fills back IO request with device offset.
 */
static void stob_part_back_fill(struct m0_stob_io *io,
				struct m0_stob_io *back)
{
	uint32_t       idx;

	idx = 0;
	do {
		back->si_user.ov_vec.v_count[idx] =
			io->si_user.ov_vec.v_count[idx];
		back->si_user.ov_buf[idx] =
			io->si_user.ov_buf[idx];

		back->si_stob.iv_index[idx] =
			stob_part_dev_offset_get(io->si_stob.iv_index[idx]);
		/**
		 * no need to update count again as it is aliases to
		 si_user.ov_vec.v_count, hence below statement is not required.
		 back->si_stob.iv_vec.v_count[idx] =
			io->si_stob.iv_vec.v_count[idx]; */

		idx++;
	} while (idx < io->si_stob.iv_vec.v_nr);
	back->si_user.ov_vec.v_nr = idx;
	back->si_stob.iv_vec.v_nr = idx;
}

/**
 * Constructs back IO for write.
 *
 * - constructs back IO with translated device address;
 *  for now there is 1:1 mapping between the io extents and
 *  translated extents, this will work with static allocation
 *  where chuncks for same partition are adjacent in memory
 *  in future to support dynamic allocation need to device
 *  io extent further if it crosses chunk boundry
 *
 */
static int stob_part_write_prepare(struct m0_stob_io *io)
{
	struct m0_stob_io          *back;
	struct m0_stob_part_io     *pio = io->si_stob_private;

	M0_PRE(io->si_opcode == SIO_WRITE);
	M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id, M0_AVI_PART_WR_PREPARE);
	M0_ENTRY("op=%d frags=%lu",
		 io->si_opcode,
		 (unsigned long)io->si_stob.iv_vec.v_nr);
	back = &pio->pi_back;

	rc = stob_part_vec_alloc(io->si_obj, back, io->si_stob.iv_vec.v_nr);
	if (rc == 0)
		stob_part_back_fill(io, back);
	return M0_RC(rc);
}

static int stob_part_io_launch_prepare(struct m0_stob_io *io)
{
	struct m0_stob_part_io     *pio  = io->si_stob_private;
	struct m0_stob_io          *back = &pio->pi_back;
	int                         rc;

	M0_PRE(io->si_stob.iv_vec.v_nr > 0);
	M0_PRE(!m0_vec_is_empty(&io->si_user.ov_vec));
	M0_PRE(io->si_state == SIS_PREPARED);

	/* prefix fragments execution mode is not yet supported */
	M0_PRE((io->si_flags & SIF_PREFIX) == 0);
	/* only read-write at the moment */
	M0_PRE(io->si_opcode == SIO_READ || io->si_opcode == SIO_WRITE);

	M0_ENTRY("op=%d, stob %p, stob_id="STOB_ID_F,
		 io->si_opcode, io->si_obj, STOB_ID_P(&io->si_obj->so_id));

	M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id, M0_AVI_PART_PREPARE);

	back->si_opcode   = io->si_opcode;
	back->si_flags    = io->si_flags;
	back->si_fol_frag = io->si_fol_frag;
	back->si_id       = io->si_id;

	switch (io->si_opcode) {
	case SIO_READ:
		rc = stob_part_read_prepare(io);
		break;
	case SIO_WRITE:
		rc = stob_part_write_prepare(io);
		break;
	default:
		M0_IMPOSSIBLE("Invalid io type.");
	}

	return rc;
}

/**
 * Launch asynchronous IO.
 *
 * Call ad_write_prepare() or ad_read_prepare() to do the bulk of work, then
 * launch back IO just constructed.
 */
static int stob_part_io_launch(struct m0_stob_io *io)
{
	struct m0_stob_ad_domain *adom;
	struct m0_stob_ad_io     *aio     = io->si_stob_private;
	struct m0_stob_io        *back    = &aio->ai_back;
	int                       rc      = 0;
	bool                      wentout = false;

	M0_PRE(io->si_stob.iv_vec.v_nr > 0);
	M0_PRE(!m0_vec_is_empty(&io->si_user.ov_vec));
	M0_PRE(io->si_state == SIS_BUSY);

	/* prefix fragments execution mode is not yet supported */
	M0_PRE((io->si_flags & SIF_PREFIX) == 0);
	/* only read-write at the moment */
	M0_PRE(io->si_opcode == SIO_READ || io->si_opcode == SIO_WRITE);

	M0_ENTRY("op=%d stob_id="STOB_ID_F,
		 io->si_opcode, STOB_ID_P(&io->si_obj->so_id));
	M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id, M0_AVI_AD_LAUNCH);

	adom = stob_ad_domain2ad(m0_stob_dom_get(io->si_obj));

	if (back->si_stob.iv_vec.v_nr > 0) {
		/**
		 * Sorts index vecs in incremental order.
		 * @todo : Needs to check performance impact
		 *        of sorting each stobio on ad stob.
		 */
		M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id,
			     M0_AVI_AD_SORT_START);
		m0_stob_iovec_sort(back);
		M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id,
			     M0_AVI_AD_SORT_END);
		rc = m0_stob_io_prepare_and_launch(back, adom->sad_bstore,
						   io->si_tx, io->si_scope);
		wentout = rc == 0;
	} else {
		/*
		 * Back IO request was constructed OK, but is empty (all
		 * IO was satisfied from holes). Notify caller about
		 * completion.
		 */
		M0_ASSERT(io->si_opcode == SIO_READ);
		stob_part_endio(&aio->ai_clink);
	}

	if (!wentout)
		stob_part_io_release(aio);
	return M0_RC(rc);
}

static bool stob_part_endio(struct m0_clink *link)
{
	struct m0_stob_part_io *pio;
	struct m0_stob_io      *io;

	pio = container_of(link, struct m0_stob_part_io, pi_clink);
	io = pio->pi_fore;

	M0_ENTRY("op=%di, stob %p, stob_id="STOB_ID_F,
		 io->si_opcode, io->si_obj, STOB_ID_P(&io->si_obj->so_id));

	M0_ASSERT(io->si_state == SIS_BUSY);
	M0_ASSERT(pio->pi_back.si_state == SIS_IDLE);

	io->si_rc     = pio->pi_back.si_rc;
	io->si_count += pio->pi_back.si_count;
	io->si_state  = SIS_IDLE;
	M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id, M0_AVI_AD_ENDIO);
	M0_ADDB2_ADD(M0_AVI_STOB_IO_END, FID_P(m0_stob_fid_get(io->si_obj)),
		     m0_time_sub(m0_time_now(), io->si_start),
		     io->si_rc, io->si_count, pio->pi_back.si_user.ov_vec.v_nr);
	stob_part_io_release(pio);
	m0_chan_broadcast_lock(&io->si_wait);
	return true;
}

static const struct m0_stob_io_op stob_part_io_op = {
	.sio_launch  = stob_part_io_launch,
	.sio_prepare = stob_part_io_launch_prepare,
	.sio_fini    = stob_part_io_fini,
};

/** @} end group stobad */

#undef M0_TRACE_SUBSYSTEM
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
