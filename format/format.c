/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

#include "lib/assert.h"      /* M0_PRE */
#include "lib/types.h"       /* PRIx64 */
#include "lib/errno.h"
#include "lib/hash_fnc.h"    /* m0_hash_fnc_fnv1 */
#include "lib/misc.h"
#include "motr/magic.h"
#include "motr/iem.h"       /* M0_MOTR_IEM_DESC */
#include "format/format.h"

/**
 * @addtogroup format
 *
 * @{
 */

M0_INTERNAL void m0_format_header_pack(struct m0_format_header *dest,
				       const struct m0_format_tag *src)
{
	dest->hd_magic = M0_FORMAT_HEADER_MAGIC;
	dest->hd_bits  = (uint64_t)src->ot_version << 48 |
			 (uint64_t)src->ot_type    << 32 |
			 (uint64_t)src->ot_size;
}

M0_INTERNAL void m0_format_header_unpack(struct m0_format_tag *dest,
					 const struct m0_format_header *src)
{
	*dest = (struct m0_format_tag){
		.ot_version = src->hd_bits >> 48,
		.ot_type    = src->hd_bits >> 32 & 0x0000ffff,
		.ot_size    = src->hd_bits & 0xffffffff
	};
}

M0_INTERNAL void m0_format_footer_generate(struct m0_format_footer *footer,
					   const void              *buffer,
					   uint32_t                 size)
{
	footer->ft_magic    = M0_FORMAT_FOOTER_MAGIC;
	footer->ft_checksum = m0_hash_fnc_fnv1(buffer, size);
}

static int get_footer_from_buf(const void                     *buffer,
			       const struct m0_format_footer **footer,
			       uint32_t                       *footer_offset)
{
	const struct m0_format_header *header = buffer;

	struct m0_format_tag tag;

	M0_PRE(buffer != NULL);

	if (header->hd_magic != M0_FORMAT_HEADER_MAGIC)
		return M0_ERR_INFO(-EPROTO, "format header magic mismatch for"
				   " the buffer %p, expected %"PRIx64
				   ", got %"PRIx64,
				   buffer, (uint64_t)M0_FORMAT_HEADER_MAGIC,
				   header->hd_magic);

	m0_format_header_unpack(&tag, header);
	/* M0_LOG(M0_DEBUG, "format header of %p buffer: version %hu, type %hu" */
	/* 		 ", footer_offset %u", buffer, tag.ot_version, */
	/* 		 tag.ot_version, tag.ot_footer_offset); */

	*footer = buffer + tag.ot_footer_offset;
	*footer_offset = tag.ot_footer_offset;

	return 0;
}

M0_INTERNAL void m0_format_footer_update(const void *buffer)
{
	struct m0_format_footer *footer;
	uint32_t                 footer_offset;
	int                      rc;

	if (buffer == NULL)
		return;

	rc = get_footer_from_buf(buffer,
				(const struct m0_format_footer **)&footer,
				&footer_offset);
	M0_ASSERT_INFO(rc == 0, "failed to update footer, invalid struct at"
				" addr %p - no valid header found", buffer);
	if (rc != 0)
		return;

	m0_format_footer_generate(footer, buffer, footer_offset);
}

static void format_iem_post(const char *msg, bool iem)
{
	if (iem)
		M0_MOTR_IEM_DESC(M0_MOTR_IEM_SEVERITY_E_ERROR,
				 M0_MOTR_IEM_MODULE_IO,
				 M0_MOTR_IEM_EVENT_MD_ERROR, "%s", msg);
	else
		M0_LOG(M0_ERROR, "%s", msg);
}

M0_INTERNAL int m0_format_footer_verify_generic(
			const struct m0_format_footer *footer,
			const void                    *buffer,
			uint32_t                       size,
			bool                           iem)
{
	uint64_t checksum;

	M0_PRE(footer != NULL);

	if (footer->ft_magic != M0_FORMAT_FOOTER_MAGIC) {
		format_iem_post("Internal corruption detected", iem);
		return M0_ERR(-EPROTO);
	}
	checksum = m0_hash_fnc_fnv1(buffer, size);
	if (footer->ft_checksum != checksum) {
		format_iem_post("Internal corruption detected", iem);
		return M0_ERR(-EPROTO);
	}
	return M0_RC(0);
}

M0_INTERNAL int m0_format_footer_verify(const void *buffer, bool iem)
{
	const struct m0_format_footer *footer;
	uint32_t footer_offset;
	int      rc;

	M0_ASSERT(buffer != NULL);
	rc = get_footer_from_buf(buffer, &footer, &footer_offset);
	if (rc != 0) {
		format_iem_post("Internal corruption detected", iem);
		return M0_ERR(rc);
	}
	return m0_format_footer_verify_generic(footer, buffer,
					       footer_offset, iem);
}

/** @} end of format group */
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
