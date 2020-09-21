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

#ifndef __MOTR_XCODE_ENUM_H__
#define __MOTR_XCODE_ENUM_H__

/**
 * @defgroup xcode
 *
 * Enumeration support in xcode
 * ----------------------------
 *
 * xcode provides a rudimentary support for enumeration (enum) reflection.
 *
 * A enumeration type is represented by an instance of m0_xcode_enum structure,
 * which contains an array (m0_xcode_enum::xe_val[]) of m0_xcode_enum_val
 * structures describing all valid enumeration constants for the type.
 *
 * xcode provides functions to convert enums between binary and symbolic form
 * (m0_xcode_enum_read() and m0_xcode_print()). In addition, a enum can be
 * interpreted as a set of bit-flags, OR-ed to construct bit-masks. Resulting
 * bit-masks can be parsed and printed by m0_xcode_bitmask_read() and
 * m0_xcode_bitmask_print().
 *
 * m0_xcode_enum representation is constructed automatically by the build
 * process for each enum declaration tagged with the M0_XCA_ENUM. This only
 * works for named enums.
 *
 * @{
 */

struct m0_xcode_enum_val;

/** Enum descriptor. */
struct m0_xcode_enum {
	/** Name of the enum type. */
	const char                     *xe_name;
	/** Number of constants in the type. */
	uint32_t                        xe_nr;
	/**
	 * Array of constant descriptors.
	 *
	 * This array contains (self->xe_nr + 1) element. The last element is
	 * a sentinel, see xcode/enum.c:valget().
	 */
	const struct m0_xcode_enum_val *xe_val;
	/**
	 * Maximal among constant name (m0_xcode_enum_val::xev_name) lengths for
	 * this type.
	 *
	 * This is useful for formatting.
	 */
	int                             xe_maxlen;
};

/** Enum value descriptor. */
struct m0_xcode_enum_val {
	/**
	 * The index, starting with 0, of the constant in the enum declaration.
	 *
	 * The literally first constant within "enum { ... }" declaration gets
	 * index 0, the second---1 and so on.
	 *
	 * The last element in the m0_xcode_enum::xe_val[] array is a sentinel
	 * with index -1.
	 */
	int         xev_idx;
	/** Constant value. */
	uint64_t    xev_val;
	/**
	 * Symbolic constant name.
	 *
	 * The sentinel element contains "Invalid value of enum 'NAME'." string
	 * here, where NAME is m0_xcode_enum::xe_name.
	 */
	const char *xev_name;
};

bool        m0_xcode_enum_is_valid   (const struct m0_xcode_enum *en,
				      uint64_t val);
/**
 * Returns symbolic name for a enum constant.
 *
 * If "buf" is NULL:
 *
 *     - this function always returns a static string;
 *
 *     - for an invalid value, a fixed (per enum type) string is returned (see
 *       m0_xcode_enum_val::xev_name). This string does not depend on the value
 *       and hence, cannot be parsed back by m0_xcode_enum_read().
 *
 * If "buf" is not NULL:
 *
 *     - valid values are processed as in (buf == NULL) case, that is, a static
 *       string is returned;
 *
 *     - invalid value is snprintf-ed into buf in the sexadecimal form. The
 *       buffer should be sufficiently large.
 */
const char *m0_xcode_enum_print      (const struct m0_xcode_enum *en,
				      uint64_t val, char *buf);
/**
 * Parses a symbolic enum constant name into the binary value.
 *
 * "buf" should be a usual zero-terminated string. The function first tries to
 * interpret first "nr" bytes as a symbolic name of a constant in the given
 * enum. Failing that, the function tries to interpret first "nr" bytes of "buf"
 * as a sexadecimal representation of a value (potentially looking beyond "nr"
 * bytes in this case).
 */
int         m0_xcode_enum_read       (const struct m0_xcode_enum *en,
				      const char *buf, int nr, uint64_t *val);
bool        m0_xcode_bitmask_is_valid(const struct m0_xcode_enum *en,
				      uint64_t val);
/**
 * Outputs the symbolic name of a bitmask.
 *
 * The general forms of the output, constructed in the first "nr" bytes of "buf"
 * are:
 *
 * @verbatim
 *     CONST_0|CONST_1|...|CONST_N|REST
 * @endverbatim
 *
 * @verbatim
 *     CONST_0|CONST_1|...|CONST_N
 * @endverbatim
 *
 * where CONST-s are symbolic names of set bits in the bitmask and REST is the
 * sexadecimal representation of the remaining invalid bits that have no
 * matching constants. The second form is used when m0_xcode_bitmask_is_valid()
 * holds.
 */
int         m0_xcode_bitmask_print   (const struct m0_xcode_enum *en,
				      uint64_t val, char *buf, int nr);
/**
 * Parses the symbolic representation of the bitmask.
 *
 * This function can parse the output of m0_xcode_bitmask_print(). And maybe
 * more, but do not rely on it.
 */
int         m0_xcode_bitmask_read    (const struct m0_xcode_enum *en,
				      const char *buf, int nr, uint64_t *val);
struct m0_xcode_obj;
struct m0_xcode_cursor;

/**
 * Custom field reader for enums.
 *
 * The pointer to this function is installed into m0_xcode_field::xf_read by
 * m0gccxml2xcode for fields tagged with the M0_XCA_FENUM macro.
 */
M0_INTERNAL int m0_xcode_enum_field_read(const struct m0_xcode_cursor *it,
					 struct m0_xcode_obj *obj,
					 const char *str);

/**
 * Custom field reader for bitmasks.
 *
 * The pointer to this function is installed into m0_xcode_field::xf_read by
 * m0gccxml2xcode for fields tagged with the M0_XCA_FBITMASK macro.
 */
M0_INTERNAL int m0_xcode_bitmask_field_read(const struct m0_xcode_cursor *it,
					    struct m0_xcode_obj *obj,
					    const char *str);

/** @} end of xcode group */
#endif /* __MOTR_XCODE_ENUM_H__ */

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
