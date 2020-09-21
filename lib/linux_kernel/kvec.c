/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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


#include "lib/types.h"
#include "lib/assert.h"
#include "lib/vec.h"
#include <linux/pagemap.h> /* PAGE_SIZE */

M0_INTERNAL int m0_0vec_page_add(struct m0_0vec *zvec,
				 struct page *pg, m0_bindex_t index)
{
	struct m0_buf buf;

	buf.b_addr = page_address(pg);
	buf.b_nob = PAGE_SIZE;

	return m0_0vec_cbuf_add(zvec, &buf, &index);
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
