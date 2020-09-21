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

#pragma once
#ifndef __MOTR_CONF_FOP_H__
#define __MOTR_CONF_FOP_H__

#include "fop/fop.h"

/**
 * @defgroup conf_fop Configuration FOPs
 *
 * @{
 */

extern struct m0_fop_type m0_conf_fetch_fopt;
extern struct m0_fop_type m0_conf_fetch_resp_fopt;

extern struct m0_fop_type m0_conf_update_fopt;
extern struct m0_fop_type m0_conf_update_resp_fopt;

extern struct m0_fop_type m0_fop_conf_load_fopt;
extern struct m0_fop_type m0_fop_conf_load_rep_fopt;

extern struct m0_fop_type m0_fop_conf_flip_fopt;
extern struct m0_fop_type m0_fop_conf_flip_rep_fopt;

M0_INTERNAL int m0_conf_fops_init(void);
M0_INTERNAL void m0_conf_fops_fini(void);

M0_INTERNAL int m0_confx_types_init(void);
M0_INTERNAL void m0_confx_types_fini(void);


/** @} conf_fop */
#endif /* __MOTR_CONF_FOP_H__ */
