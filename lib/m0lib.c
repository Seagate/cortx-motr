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


#include "lib/bitmap_xc.h"
#include "lib/buf_xc.h"
#include "lib/types_xc.h"
#include "lib/vec_xc.h"
#include "lib/types_xc.h"
#include "lib/ext_xc.h"
#include "lib/string_xc.h"
#include "lib/misc.h"       /* M0_EXPORTED */

M0_INTERNAL int libm0_init(void)
{
	extern int m0_node_uuid_init(void);
	return m0_node_uuid_init();
}
M0_EXPORTED(libm0_init);

M0_INTERNAL void libm0_fini(void)
{
}
M0_EXPORTED(libm0_fini);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
