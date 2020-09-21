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

#ifndef __MOTR_CONSOLE_CONSOLE_H__
#define __MOTR_CONSOLE_CONSOLE_H__

#include "lib/types.h"

/**
   @defgroup console Console

   Build a standalone utility that

   - connects to a specified service.
   - constructs a fop of a specified fop type and with specified
     values of fields and sends it to the service.
   - waits fop reply.
   - outputs fop reply to the user.

   The console utility can send a DEVICE_FAILURE fop to a server. Server-side
   processing for fops of this type consists of calling a single stub function.
   Real implementation will be supplied by the middleware.cm-setup task.

   @{
*/

extern bool m0_console_verbose;

/** @} end of console group */
#endif /* __MOTR_CONSOLE_CONSOLE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
