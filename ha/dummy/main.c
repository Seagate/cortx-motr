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


/**
 * @addtogroup ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "lib/assert.h"                 /* M0_ASSERT */
#include "lib/misc.h"                   /* NULL */
#include "fid/fid.h"                    /* M0_FID_TINIT */
#include "ha/halon/interface.h"         /* m0_halon_interface */

int main(int argc, char *argv[])
{
	struct m0_halon_interface *hi;
	int rc;

	rc = m0_halon_interface_init(&hi, "", "", NULL, NULL);
	M0_ASSERT(rc == 0);
	rc = m0_halon_interface_start(hi, "0@lo:12345:42:100",
	                              &M0_FID_TINIT('r', 1, 1),
	                              &M0_FID_TINIT('s', 1, 1),
	                              &M0_FID_TINIT('s', 1, 2),
				      NULL, NULL, NULL, NULL, NULL,
	                              NULL, NULL, NULL, NULL);
	M0_ASSERT(rc == 0);
	m0_halon_interface_stop(hi);
	m0_halon_interface_fini(hi);
	return 0;
}

#undef M0_TRACE_SUBSYSTEM
/** @} end of ha group */

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
