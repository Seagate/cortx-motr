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


/**
   @addtogroup xcode

   @{
 */

#include <errno.h>
#include <err.h>
#include <sysexits.h>
#include <stdlib.h>                        /* malloc, NULL */
#include <string.h>                        /* memset */

#include "xcode/ff2c/parser.h"

static struct ff2c_term *alloc(void)
{
	struct ff2c_term *term;

	term = malloc(sizeof *term);
	if (term == NULL)
		err(EX_TEMPFAIL, "Cannot allocate term (%zu bytes)", sizeof *term);
	memset(term, 0, sizeof *term);
	return term;
}

static void error(struct ff2c_context *ctx, const char *msg)
{
	char buf[100];

	ff2c_context_loc(ctx, sizeof buf, buf);
	errx(2, "%s: %s", buf, msg);
}

static struct ff2c_term *add(struct ff2c_term *term)
{
	struct ff2c_term *new;

	new = alloc();
	new->fn_parent = term;
	return term->fn_tail = *(term->fn_head == NULL ?
				 &term->fn_head : &term->fn_tail->fn_next) = new;
}

static void token(struct ff2c_context *ctx, struct ff2c_term *term,
		  struct ff2c_token *tok)
{
	switch (tok->ft_type) {
	case FTT_TAG:
		term->fn_type = FNT_TAG;
		break;
	case FTT_ESCAPE:
		term->fn_type = FNT_ESCAPE;
		break;
	default:
		err(EX_SOFTWARE, "impossible token");
	}
	term->fn_tok = *tok;
}

static int declaration(struct ff2c_context *ctx, struct ff2c_term *term);

static int field(struct ff2c_context *ctx, struct ff2c_term *term)
{
	int result;

	result = declaration(ctx, term);
	if (result == 0) {
		struct ff2c_token tok;

		while (1) {
			result = ff2c_token_get(ctx, &tok);
			if (result > 0) {
				result = 0;
				if (tok.ft_type == FTT_TAG ||
				    tok.ft_type == FTT_ESCAPE)
					token(ctx, add(term), &tok);
				else {
					ff2c_token_put(ctx, &tok);
					break;
				}
			} else
				error(ctx, "unterminated field");
		}
	}
	return result;
}

static int field_list(struct ff2c_context *ctx, struct ff2c_term *term)
{
	int result;
	struct ff2c_token tok;

	result = ff2c_token_get(ctx, &tok);
	if (result > 0 && tok.ft_type == FTT_OPEN) {
		do
			result = field(ctx, add(term));
		while (result == 0 && (result = ff2c_token_get(ctx, &tok) > 0) &&
		       tok.ft_type == FTT_SEMICOLON);
		if (result > 0) {
			if (tok.ft_type == FTT_CLOSE)
				return 0;
			else
				error(ctx, "\"}\" or \";\"expected");
		} else if (result == 0)
			error(ctx, "unterminated field list");
	} else
		error(ctx, "\"{\" expected");
	return -EINVAL;
}

static int type(struct ff2c_context *ctx, struct ff2c_term *term)
{
	int result;

	result = ff2c_token_get(ctx, &term->fn_tok);
	if (result > 0) {
		switch (term->fn_tok.ft_type) {
		case FTT_VOID:
		case FTT_U8:
		case FTT_U32:
		case FTT_U64:
		case FTT_OPAQUE:
			term->fn_type = FNT_ATOMIC;
			return 0;
		case FTT_RECORD:
		case FTT_UNION:
		case FTT_SEQUENCE:
		case FTT_ARRAY:
			term->fn_type = FNT_COMPOUND;
			return field_list(ctx, term);
		case FTT_IDENTIFIER:
			term->fn_type = FNT_TYPENAME;
			return 0;
		default:
			break;
		}
	}
	error(ctx, "type expected");
	return -EINVAL;
}

static int declaration(struct ff2c_context *ctx, struct ff2c_term *term)
{
	int result;

	term->fn_type = FNT_DECLARATION;
	result = type(ctx, add(term));
	if (result == 0) {
		result = ff2c_token_get(ctx, &term->fn_tok);
		if (result > 0 && term->fn_tok.ft_type == FTT_IDENTIFIER)
			return 0;
		else
			error(ctx, "declaration must be terminated "
			      "with an identifier");
	}
	return -EINVAL;
}

static int require(struct ff2c_context *ctx, struct ff2c_term *term)
{
	int result;

	term->fn_type = FNT_REQUIRE;
	result = ff2c_token_get(ctx, &term->fn_tok);
	if (result > 0 && term->fn_tok.ft_type == FTT_STRING)
		return 0;
	else {
		error(ctx, "\"require\" must be followed by a pathname");
		return -EINVAL;
	}
}

static int statement(struct ff2c_context *ctx, struct ff2c_term *term)
{
	int result;
	struct ff2c_token tok;

	result = ff2c_token_get(ctx, &tok);
	if (result > 0) {
		if (tok.ft_type == FTT_REQUIRE)
			result = require(ctx, term);
		else {
			ff2c_token_put(ctx, &tok);
			result = declaration(ctx, term);
		}
	}
	return result;
}

static int ff(struct ff2c_context *ctx, struct ff2c_term *term)
{
	int               result;
	struct ff2c_token tok;

	term->fn_type = FNT_FF;
	do
		result = statement(ctx, add(term));
	while (result == 0 && (result = ff2c_token_get(ctx, &tok) > 0) &&
	       tok.ft_type == FTT_SEMICOLON);
	return result;
}

int ff2c_parse(struct ff2c_context *ctx, struct ff2c_term **out)
{
	*out = alloc();
	return ff(ctx, *out);
}

void ff2c_term_fini(struct ff2c_term *term)
{
	free(term);
}

const char *ff2c_term_type_name[] = {
	[FNT_FF]          = "FF",
	[FNT_REQUIRE]     = "REQUIRE",
	[FNT_DECLARATION] = "DECLARATION",
	[FNT_ATOMIC]      = "ATOMIC",
	[FNT_COMPOUND]    = "COMPOUND",
	[FNT_TYPENAME]    = "TYPENAME",
	[FNT_TAG]         = "TAG",
	[FNT_ESCAPE]      = "ESCAPE",
};

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
