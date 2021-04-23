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

#ifndef __MOTR_XCODE_XCODE_ATTR_H__
#define __MOTR_XCODE_XCODE_ATTR_H__

/**
 * @addtogroup xcode
 * @{
 */

/**
 * Set xcode attribute on a struct or strucut's field. This sets a special gcc
 * __attribute__ which is ignored by gcc during compilation, but which is then
 * used by gccxml and m0gccxml2xcode to generate xcode data.
 *
 * Please, refer to m0gccxml2xcode documentation for more details.
 */
#ifdef ENABLE_GCCXML
#define M0_XC_ATTR(name, val) __attribute__((gccxml("xc_" name, val)))
#else
#define M0_XC_ATTR(name, val) __attribute__((annotate("xc_" name "," val)))
#endif

/**
 * Shortened versions of M0_XC_ATTR to specifiy m0_xcode_aggr types.
 */
#define M0_XCA_RECORD   M0_XC_ATTR("atype", "M0_XA_RECORD")
#define M0_XCA_SEQUENCE M0_XC_ATTR("atype", "M0_XA_SEQUENCE")
#define M0_XCA_ARRAY    M0_XC_ATTR("atype", "M0_XA_ARRAY")
#define M0_XCA_UNION    M0_XC_ATTR("atype", "M0_XA_UNION")
#define M0_XCA_BLOB     M0_XC_ATTR("atype", "M0_XA_BLOB")
#define M0_XCA_ENUM     M0_XC_ATTR("enum",  "nonce")

#define M0_XCA_OPAQUE(value)   M0_XC_ATTR("opaque", value)
#define M0_XCA_TAG(value)      M0_XC_ATTR("tag", value)
#define M0_XCA_FENUM(value)    M0_XC_ATTR("fenum", #value)
#define M0_XCA_FBITMASK(value) M0_XC_ATTR("fbitmask", #value)

/**
 * Set "xcode domain" attribute on a struct. The domain is used in `m0protocol`
 * utility to separate xcode structs into groups.
 *
 * @param  value  a domain name, valid values are 'be', 'rpc', 'conf' or
 *		  any combination of those separated by a '|' (pipe symbol)
 *		  without spaces, e.g. 'be|conf|rpc'.
 *
 * @example  M0_XCA_DOMAIN(be)
 *           M0_XCA_DOMAIN(conf|rpc)
 */
#define M0_XCA_DOMAIN(value)   M0_XC_ATTR("domain", #value)

/** @} end of xcode group */

/* __MOTR_XCODE_XCODE_ATTR_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
