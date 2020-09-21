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

#pragma once
#ifndef __MOTR_CONF_VALIDATION_H__
#define __MOTR_CONF_VALIDATION_H__

#include "lib/types.h"  /* bool */

struct m0_conf_cache;
struct m0_conf_obj;
struct m0_fid;

/**
 * @defgroup conf_validation
 *
 * Motr subsystems that use confc API (m0t1fs, m0d, ioservice, &c.)
 * have certain expectations of the configuration objects they work with.
 * Subsystem developers specify these expectations in the form of "rules",
 * which valid configuration data should conform to.
 *
 * @{
 */

/**
 * Performs semantic validation of the DAG of configuration objects.
 *
 * If m0_conf_validation_error() finds a problem with configuration
 * data, it returns a pointer to a string that describes the problem.
 * This may be either a pointer to a string that the function stores
 * in `buf', or a pointer to some (imutable) static string (in which
 * case `buf' is unused).  If the function stores a string in `buf',
 * then at most `buflen' bytes are stored (the string may be truncated
 * if `buflen' is too small).  The string always includes a terminating
 * null byte ('\0').
 *
 * If no issues with configuration data are found, m0_conf_validation_error()
 * returns NULL.
 *
 * @pre  buf != NULL && buflen != 0
 */
char *m0_conf_validation_error(struct m0_conf_cache *cache,
			       char *buf, size_t buflen);

/**
 * Similar to m0_conf_validation_error(), but requires conf cache to be locked.
 *
 * @pre  buf != NULL && buflen != 0
 * @pre  m0_conf_cache_is_locked(cache)
 */
M0_INTERNAL char *m0_conf_validation_error_locked(
	const struct m0_conf_cache *cache, char *buf, size_t buflen);

/** Validation rule. */
struct m0_conf_rule {
	/*
	 * Use the name of the function that .cvr_error points at.
	 * This simplifies finding the rule that failed.
	 */
	const char *cvr_name;
	/**
	 * @see m0_conf_validation_error() for arguments' description.
	 *
	 * @pre  m0_conf_cache_is_locked(cache)
	 * (This precondition is enforced by m0_conf_validation_error().)
	 */
	char     *(*cvr_error)(const struct m0_conf_cache *cache,
			       char *buf, size_t buflen);
};

/** Maximal number of rules in a m0_conf_ruleset. */
enum { M0_CONF_RULES_MAX = 32 };

/** Named set of validation rules. */
struct m0_conf_ruleset {
	/*
	 * Use the name of m0_conf_ruleset variable. This simplifies
	 * finding the rule that failed.
	 */
	const char         *cv_name;
	/*
	 * This array must end with { NULL, NULL }.
	 */
	struct m0_conf_rule cv_rules[M0_CONF_RULES_MAX];
};

/** @} */
#endif /* __MOTR_CONF_VALIDATION_H__ */

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
