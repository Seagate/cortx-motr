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
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_STOB
#include "lib/trace.h"		/* M0_LOG */

#include "addb2/addb2.h"
#include "module/instance.h"	/* m0_get */

#include "stob/partition.h"
#include "stob/addb2.h"
#include "stob/domain.h"
#include "stob/io.h"
#include "stob/module.h"	/* m0_stob_ad_module */
#include "stob/stob.h"
#include "stob/stob_internal.h"	/* m0_stob__fid_set */
#include "stob/type.h"		/* m0_stob_type */
#include "be/domain.h"
#include "be/partition_table.h"

/**
 * @addtogroup stobpart
 *
 * @{
 */
/** TODO stob_part_domain_ops is defined as gloabl for now
 * to remove compile warning, update to static once
 * partition stob domain functionalty(EOS-24532) is ready */
struct m0_stob_domain_ops stob_part_domain_ops;
static struct m0_stob_ops        stob_part_ops;
struct part_stob_cfg {
	m0_bcount_t  psg_id;
	m0_bcount_t  psg_size_in_chunks;
};

static int stob_part_io_init(struct m0_stob *stob, struct m0_stob_io *io);
static int stob_part_punch(struct m0_stob *stob,
			   struct m0_indexvec *range,
			   struct m0_dtx *tx);

static void stob_part_write_credit(const struct m0_stob_domain *dom,
				   const struct m0_stob_io     *iv,
				   struct m0_be_tx_credit      *accum)
{

}

static void stob_part_domain_fini(struct m0_stob_domain *dom)
{

}

M0_INTERNAL struct m0_stob_part_domain *
stob_part_domain2part(const struct m0_stob_domain *dom)
{
	struct m0_stob_part_domain *pdom;

	pdom = (struct m0_stob_part_domain *)dom->sd_private;
	/* *m0_stob_part_domain_bob_check(pdom); */
	M0_ASSERT(m0_stob_domain__dom_key(m0_stob_domain_id_get(dom)) ==
		  pdom->spd_dom_key);
	return pdom;
}

static struct m0_stob_part *stob_part_stob2part(const struct m0_stob *stob)
{
	return container_of(stob, struct m0_stob_part, part_stob);
}

static struct m0_stob *stob_part_alloc(struct m0_stob_domain *dom,
				       const struct m0_fid *stob_fid)
{
	struct m0_stob_part *partstob;

	M0_ALLOC_PTR(partstob);
	return partstob == NULL ? NULL : &partstob->part_stob;
}

static void stob_part_free(struct m0_stob_domain *dom,
			   struct m0_stob *stob)
{
	struct m0_stob_part *partstob = stob_part_stob2part(stob);

	m0_free(partstob->part_table);
	m0_free(partstob);
}

static int stob_part_cfg_parse(const char *str_cfg_create, void **cfg_create)
{
	struct part_stob_cfg *cfg;
	int                   rc;

	if (str_cfg_create == NULL)
		return M0_ERR(-EINVAL);

	M0_ALLOC_PTR(cfg);
	if (cfg != NULL) {
		/* format = partition-id:partition-size */
		rc = sscanf(str_cfg_create, ":%"SCNd64":%"SCNd64"",
		    &cfg->psg_id,
		    &cfg->psg_size_in_chunks);
		rc = rc == 4 ? 0 : -EINVAL;
	} else
		rc = -ENOMEM;
	if (rc == 0)
		*cfg_create = cfg;

	return rc;
}

static void stob_part_cfg_free(void *cfg_create)
{
	m0_free(cfg_create);
}

static int stob_part_init(struct m0_stob *stob,
			  struct m0_stob_domain *dom,
			  const struct m0_fid *stob_fid)
{
	stob->so_ops = &stob_part_ops;
	return 0;
}

static void stob_part_fini(struct m0_stob *stob)
{
}

static void stob_part_create_credit(struct m0_stob_domain *dom,
				    struct m0_be_tx_credit *accum)
{
}

static int stob_part_create(struct m0_stob *stob,
			    struct m0_stob_domain *dom,
			    struct m0_dtx *dtx,
			    const struct m0_fid *stob_fid,
			    void *cfg)
{
	struct m0_stob_part              *partstob = stob_part_stob2part(stob);
	struct m0_be_ptable_part_tbl_info pt;
	m0_bcount_t                       primary_part_index;
	m0_bcount_t                       part_index;
	struct part_stob_cfg             *pcfg;

	M0_ENTRY();
	pcfg = (struct part_stob_cfg *)cfg;
	partstob = stob_part_stob2part(stob);
	partstob->part_id = pcfg->psg_id;
	partstob->part_size_in_chunks = pcfg->psg_size_in_chunks;

	M0_ALLOC_ARR(partstob->part_table,
		     partstob->part_size_in_chunks);
	if (partstob->part_table == NULL)
		return M0_ERR(-ENOMEM);

	if ( m0_be_ptable_get_part_info(&pt))
		M0_ASSERT(0);

	partstob->part_chunk_size_in_bits = pt.pti_chunk_size_in_bits;
	/**
	 * populate partition table
	 */
	part_index = 0;
	for (primary_part_index = 0;
	     primary_part_index <= pt.pti_dev_size_in_chunks;
	     primary_part_index++) {
		if (pt.pti_pri_part_info[primary_part_index].ppi_part_id ==
			    partstob->part_id)
			partstob->part_table[part_index++] = primary_part_index;
	}
	M0_ASSERT(part_index <= partstob->part_size_in_chunks);
	M0_LEAVE();
	return 0;
}

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
	return 0;
}

/** TODO stob_part_domain_ops is defined as gloabl for now
 * to remove compile warning, update to static once
 * partition stob domain functionalty(EOS-24532) is ready */
struct m0_stob_domain_ops stob_part_domain_ops = {
	.sdo_fini		= &stob_part_domain_fini,
	.sdo_stob_alloc	    	= &stob_part_alloc,
	.sdo_stob_free	    	= &stob_part_free,
	.sdo_stob_cfg_parse 	= &stob_part_cfg_parse,
	.sdo_stob_cfg_free  	= &stob_part_cfg_free,
	.sdo_stob_init	    	= &stob_part_init,
	.sdo_stob_create_credit	= &stob_part_create_credit,
	.sdo_stob_create	= &stob_part_create,
	.sdo_stob_write_credit	= &stob_part_write_credit,
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

#if 0
const struct m0_stob_type m0_stob_part_type = {
	.st_ops  = &stob_part_type_ops,
	.st_fidt = {
		.ft_id   = STOB_TYPE_PARTITION,
		.ft_name = "partitionstob",
	},
};
#endif

static const struct m0_stob_io_op stob_part_io_op;

static bool stob_part_endio(struct m0_clink *link);
static void stob_part_io_release(struct m0_stob_part_io *pio);

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

   @see part_vec_alloc()
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

static m0_bcount_t stob_part_dev_offset_get(struct m0_stob_part *partstob,
					    m0_bcount_t user_byte_offset)
{
	m0_bcount_t   chunk_off_mask;
	m0_bcount_t   table_index;
	m0_bcount_t   offset_within_chunk;
	m0_bcount_t   device_chunk_offset;
	m0_bcount_t   device_byte_offset;

	chunk_off_mask = (1 << partstob->part_size_in_chunks) - 1;
	offset_within_chunk = user_byte_offset & chunk_off_mask;
	M0_LOG(M0_DEBUG, "\nDEBUG relative offset in given chunk: %" PRIu64,
		offset_within_chunk);
	table_index =
		(user_byte_offset >> partstob->part_size_in_chunks);
	M0_LOG(M0_DEBUG, "\n\nDEBUG: table_index :%" PRIu64,
		table_index);

	device_chunk_offset = partstob->part_table[table_index];

	M0_LOG(M0_DEBUG, "\nDEBUG: device_chunk_offset: %" PRIu64,
		device_chunk_offset);
	device_byte_offset =
		( device_chunk_offset << partstob->part_size_in_chunks ) +
		offset_within_chunk;
	M0_LOG(M0_DEBUG, "\nDEBUG: device offset in bytes: %" PRIu64,
		device_byte_offset);

	return(device_byte_offset);
}

/**
   Fills back IO request with device offset.
 */
static void stob_part_back_fill(struct m0_stob_io *io,
				struct m0_stob_io *back)
{
	uint32_t             idx;
	struct m0_stob      *stob = io->si_obj;
	struct m0_stob_part *partstob = stob_part_stob2part(stob);

	idx = 0;
	do {
		back->si_user.ov_vec.v_count[idx] =
			io->si_user.ov_vec.v_count[idx];
		back->si_user.ov_buf[idx] =
			io->si_user.ov_buf[idx];

		back->si_stob.iv_index[idx] =
			stob_part_dev_offset_get(partstob,
						 io->si_stob.iv_index[idx]);
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
	int                         rc;

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
	struct m0_stob_part_domain *pdom;
	struct m0_stob_part_io     *pio     = io->si_stob_private;
	struct m0_stob_io          *back    = &pio->pi_back;
	int                         rc      = 0;
	bool                        wentout = false;

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

	pdom = stob_part_domain2part(m0_stob_dom_get(io->si_obj));

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
		rc = m0_stob_io_prepare_and_launch(back, pdom->spd_bstore,
						   io->si_tx, io->si_scope);
		wentout = rc == 0;
	} else {
		/*
		 * Back IO request was constructed OK, but is empty (all
		 * IO was satisfied from holes). Notify caller about
		 * completion.
		 */
		M0_ASSERT(io->si_opcode == SIO_READ);
		stob_part_endio(&pio->pi_clink);
	}

	if (!wentout)
		stob_part_io_release(pio);
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

/** @} end group stobpart */

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
