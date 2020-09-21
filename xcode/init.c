/* -*- C -*- */
/*
 * Copyright (c) 2015-2020 Seagate Technology LLC and/or its Affiliates
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
 * @addtogroup xcode
 *
 * @{
 */

#define _XT(x)
#define _TI(x)
#define _EN(x)
#define _FI(x) M0_INTERNAL void x(void);
#define _FF(x) M0_INTERNAL void x(void);
#include "xcode/xlist.h"
#undef __MOTR_XCODE_XLIST_H__
#undef _XT
#undef _TI
#undef _EN
#undef _FI
#undef _FF

int m0_xcode_init(void)
{
#define _XT(x)
#define _TI(x)
#define _EN(x)
#define _FI(x) x();
#define _FF(x)
#include "xcode/xlist.h"
#undef __MOTR_XCODE_XLIST_H__
#undef _XT
#undef _TI
#undef _EN
#undef _FI
#undef _FF
	return 0;
}

void m0_xcode_fini(void)
{
#define _XT(x)
#define _TI(x)
#define _EN(x)
#define _FI(x)
#define _FF(x) x();
#include "xcode/xlist.h"
#undef _XT
#undef _TI
#undef _EN
#undef _FI
#undef _FF
}

/** @} end of xcode group */

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
