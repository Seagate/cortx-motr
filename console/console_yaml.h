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

#ifndef __MOTR_CONSOLE_YAML_H__
#define __MOTR_CONSOLE_YAML_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <yaml.h>

/**
   @addtogroup console_yaml
   @{
*/

/** enable/disable yaml support */
M0_EXTERN bool yaml_support;

/**
 * @brief Keeps info for YAML parser.
 */
struct m0_cons_yaml_info {
        /** YAML parser structure */
        yaml_parser_t    cyi_parser;
        /** YAML event structure */
        yaml_event_t     cyi_event;
        /** YAML document structure */
        yaml_document_t  cyi_document;
        /** Current Node in document */
	yaml_node_t	*cyi_current;
        /** YAML file pointer */
        FILE            *cyi_file;
};

/**
 * @brief Inititalizes parser by opening given file.
 *	  and also checks for error by getting root node.
 *
 * @param path YAML file path.
 *
 * @return 0 success and -errno failure.
 */
M0_INTERNAL int m0_cons_yaml_init(const char *path);

/**
 * @brief  Search for specified string and get the respctive value
 *	   form YAML file. (like "name : console")
 *
 * @param value Search string (like name).
 * @param data  Respective data (like console).
 *
 * @return 0 success and -errno failure.
 */
M0_INTERNAL int m0_cons_yaml_set_value(const char *value, void *data);

/**
 * @brief  Search for specified string and set the respctive value
 *	   form YAML file. (like "name : console")
 *
 * @param value Search string (like name).
 *
 * @return 0 success and -errno failure.
 */
M0_INTERNAL void *m0_cons_yaml_get_value(const char *value);

/**
 * @brief Deletes the parser and closes the YAML file.
 */
M0_INTERNAL void m0_cons_yaml_fini(void);

/** @} end of console_yaml group */
/* __MOTR_CONSOLE_YAML_H__ */
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
