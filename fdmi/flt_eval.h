/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_FDMI_FDMI_FLT_EVAL_H__
#define __MOTR_FDMI_FDMI_FLT_EVAL_H__

#include "fdmi/filter.h"
/**
 * @defgroup FDMI_DLD_fspec_filter_eval FDMI filter evaluator description
 * @ingroup fdmi_main
 * @see @ref FDMI-DLD-fspec "FDMI Functional Specification"
 * @see @ref FDMI_DLD_fspec_filter
 * @{
 */


struct m0_conf_fdmi_filter;

/**
 * Array of operands
 */
struct m0_fdmi_flt_operands {
	int                         ffp_count;
	struct m0_fdmi_flt_operand  ffp_operands[FDMI_FLT_MAX_OPNDS_NR];
};

/**
 * Function type, implementing some operation of FDMI filter tree
 */
typedef int (*m0_fdmi_flt_op_cb_t)(struct m0_fdmi_flt_operands *opnds,
                                   struct m0_fdmi_flt_operand  *res);

/**
 * FDMI filter evaluator context
 *
 * FDMI filter evaluator do calculation of FDMI filters
 */
struct m0_fdmi_eval_ctx {
	/** Array of operation handlers. Index is code of
	  * operation from @ref m0_fdmi_flt_op_code */
	m0_fdmi_flt_op_cb_t  opers[M0_FFO_TOTAL_OPS_CNT];
};

/**
 * Structure provided by user to retrieve values from variable nodes
 */
struct m0_fdmi_eval_var_info {
	void *user_data;
	int (*get_value_cb)(void                        *user_data,
	                    struct m0_fdmi_flt_var_node *value_desc,
	                    struct m0_fdmi_flt_operand  *value);
};

/**
 * Initialize FDMI filter evaluator context
 *
 * @param ctx  FDMI filter evaluator context
 */
M0_INTERNAL void m0_fdmi_eval_init(struct m0_fdmi_eval_ctx *ctx);

/**
 * Add operation handler to the evaluator context
 *
 * Can be used by FDMI sources to add operation handlers
 * for data types, specific to these FDMI sources
 *
 * @param ctx FDMI filter evaluator context
 * @param op  operation code
 * @param cb  Handler to be registered
 *
 * @return 0 on sucess, error code otherwise
 */
M0_INTERNAL int m0_fdmi_eval_add_op_cb(struct m0_fdmi_eval_ctx *ctx,
                                       enum m0_fdmi_flt_op_code op,
                                       m0_fdmi_flt_op_cb_t      cb);

/**
 * Delete operation handler from the evaluator context
 *
 * @param ctx FDMI filter evaluator context
 * @param op  operation code
 *
 * @see m0_fdmi_eval_add_op_cb
 */
M0_INTERNAL void m0_fdmi_eval_del_op_cb(struct m0_fdmi_eval_ctx *ctx,
                                        enum m0_fdmi_flt_op_code op);

/**
 * Evaluate filter expression tree value
 *
 * Result of filter expression is always boolean.
 *
 * @param filter   FDMI filter
 * @param ctx      FDMI filter evaluator context
 * @param var_info Information about how to get value of variable nodes
 * @return  <0, if some error occured, @n
 *           0, if result of evaluation is False @n
 *           1, if result of evaluation is True
 */
M0_INTERNAL int m0_fdmi_eval_flt(struct m0_fdmi_eval_ctx      *ctx,
                                 struct m0_conf_fdmi_filter   *filter,
                                 struct m0_fdmi_eval_var_info *var_info);

/**
 * Finalize FDMI evaluator
 *
 * @param ctx     FDMI filter evaluator context
 */
M0_INTERNAL void m0_fdmi_eval_fini(struct m0_fdmi_eval_ctx *ctx);

/** @} end of FDMI_DLD_fspec_filter_eval */

#endif /* __MOTR_FDMI_FDMI_FLT_EVAL_H__ */

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
