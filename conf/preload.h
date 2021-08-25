/* -*- C -*- */
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

#pragma once
#ifndef __MOTR_CONF_PRELOAD_H__
#define __MOTR_CONF_PRELOAD_H__

struct m0_confx;

/**
 * @page conf-fspec-preload Pre-Loading of Configuration Cache
 *
 * - @ref conf-fspec-preload-string
 *   - @ref conf-fspec-preload-string-format
 *   - @ref conf-fspec-preload-string-examples
 * - @ref conf_dfspec_preload "Detailed Functional Specification"
 *
 * When configuration cache is created, it can be pre-loaded with
 * configuration data.  Cache pre-loading can be useful for testing,
 * boot-strapping, and manual control. One of use cases is a situation
 * when confc cannot or should not communicate with confd.
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-fspec-preload-string Configuration string
 *
 * The application pre-loads confc cache by passing textual
 * description of configuration objects -- so called configuration
 * string -- to m0_confc_init() via `local_conf' parameter.
 *
 * When confc API is used by a kernel module, configuration string is
 * provided via mount(8) option.
 *
 * <!---------------------------------------------------------------->
 * @subsection conf-fspec-preload-string-format Format
 *
 * The format of configuration string corresponds to the format of
 * string argument of m0_xcode_read() function.
 *
 * <!---------------------------------------------------------------->
 * @subsection conf-fspec-preload-string-examples Examples
 *
 * See examples of configuration strings in ut/conf.cg and
 * m0t1fs/linux_kernel/st/st.
 *
 * @see @ref conf_dfspec_preload "Detailed Functional Specification"
 */

/**
 * @defgroup conf_dfspec_preload Pre-Loading of Configuration Cache
 * @brief Detailed Functional Specification.
 *
 * @see @ref conf, @ref conf-fspec-preload "Functional Specification"
 *
 * @{
 */

/**
 * Encodes configuration string.
 *
 * @note If the call succeeds, the user is responsible for freeing
 *       allocated memory with m0_confx_free(*out).
 */
M0_INTERNAL int m0_confstr_parse(const char *str, struct m0_confx **out);

/** Frees the memory, dynamically allocated by m0_confstr_parse(). */
M0_INTERNAL void m0_confx_free(struct m0_confx *enc);

/**
 * @note If the call succeeds, the user is responsible for calling
 *       m0_confx_string_free(*out).
 */
M0_INTERNAL int m0_confx_to_string(struct m0_confx *confx, char **out);

/**
 * @pre m0_addr_is_aligned(str, PAGE_SHIFT)
 */
M0_INTERNAL void m0_confx_string_free(char *str);

/** @} conf_dfspec_preload */
#endif /* __MOTR_CONF_PRELOAD_H__ */
