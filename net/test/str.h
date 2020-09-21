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

#ifndef __MOTR_NET_TEST_STR_H__
#define __MOTR_NET_TEST_STR_H__

#include "net/test/serialize.h"


/**
   @defgroup NetTestStrDFS Serialization of ASCIIZ string
   @ingroup NetTestDFS

   @see
   @ref net-test

   @{
 */

/**
   Serialize or deserialize ASCIIZ string.
   @pre op == M0_NET_TEST_SERIALIZE || op == M0_NET_TEST_DESERIALIZE
   @pre str != NULL
   @note str should be freed (with m0_free0()) after deserialisation
         to prevent memory leak.
 */
m0_bcount_t m0_net_test_str_serialize(enum m0_net_test_serialize_op op,
				      char **str,
				      struct m0_bufvec *bv,
				      m0_bcount_t bv_offset);

/** @} end of NetTestStrDFS group */
#endif /*  __MOTR_NET_TEST_STR_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
