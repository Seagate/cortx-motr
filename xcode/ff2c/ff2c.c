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


/**
   @addtogroup xcode

   <b>ff2c</b>

   ff2c is a simple translator, taking as an input a set of descriptions of
   desired serialized representations and producing a set of C declarations of
   types having given serialized representations. In addition, definitions of
   xcode data (m0_xcode_type and m0_xcode_field) for generated types is
   produced.

   Serialized representation descriptions are given in a language with the
   following grammar:

   @verbatim
ff             ::= statement-list
statement-list ::= statement | statement ';' statement-list
statement      ::= require | declaration
require        ::= 'require' '"' pathname '"'
declaration    ::= type identifier
type           ::= atomic | compound | opaque | identifier
atomic         ::= 'void' | 'u8' | 'u32' | 'u64'
compound       ::= kind '{' field-list '}'
opaque         ::= '*' identifier
kind           ::= 'record' | 'union' | 'sequence'
field-list     ::= field | field ';' field-list
field          ::= declaration tag escape
tag            ::= empty | ':' expression
escape         ::= empty | '[' identifier ']'
   @endverbatim

The language is case-sensitive. Tokens are separated by blank space and C-style
comments.

The meaning of language constructs is explained in the following example:
@verbinclude "./sample.ff"

  The translator is structured as a sequence of 4 passes:

      - lexical analysis (lex.c): takes a buffer as an input and produces a
        stream of tokens;

      - parser (parser.h): builds a parse tree out of stream of tokens. Every
        node in the tree have the same structure: it contains a token and a list
        of child nodes;

      - semantical pass (sem.h): traverses parse tree and builds a tree of
        semantical nodes, corresponding to types and fields. Semantical nodes
        are "higher level" than syntax nodes are more closely aligned with the
        requirements of the following generation pass;

      - generation (gen.h): take semantic tree and produce corresponding C
        declarations and definitions.
   @{
 */

#include <err.h>
#include <sysexits.h>
#include <unistd.h>                           /* getopt, close, open */
#include <sys/mman.h>                         /* mmap, munmap */
#include <sys/types.h>
#include <sys/stat.h>                         /* stat */
#include <fcntl.h>                            /* O_RDONLY */
#include <libgen.h>                           /* dirname */
#include <string.h>                           /* basename, strdup, strlen */
#include <stdlib.h>                           /* malloc, free */
#include <ctype.h>                            /* toupper */
#include <stdio.h>                            /* fopen, fclose */

#include "xcode/ff2c/lex.h"
#include "xcode/ff2c/parser.h"
#include "xcode/ff2c/sem.h"
#include "xcode/ff2c/gen.h"

int main(int argc, char **argv)
{
	int          fd;
	int          optch;
	int          result;
	const char  *path;
	void        *addr;
	struct stat  buf;
	char        *scratch;
	char        *ch;
	char        *bname;
	char        *dname;
	char        *gname;
	char        *out_h;
	char        *out_c;
	size_t       len;
	FILE        *c;
	FILE        *h;

	struct ff2c_context   ctx;
	struct ff2c_term     *t;
	struct ff2c_ff        ff;
	struct ff2c_gen_opt   opt;

	while ((optch = getopt(argc, argv, "")) != -1) {
	}

	path = argv[optind];
	fd = open(path, O_RDONLY);
	if (fd == -1)
		err(EX_NOINPUT, "cannot open \"%s\"", path);
	result = fstat(fd, &buf);
	if (result == -1)
		err(EX_UNAVAILABLE, "cannot fstat \"%s\"", path);
	addr = mmap(NULL, buf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED)
		err(EX_OSERR, "cannot mmap \"%s\"", path);

	scratch = fmt("%s", path);
	len = strlen(scratch);
	if (len > 3 && strcmp(scratch + len - 3, ".ff") == 0)
		*(scratch + len - 3) = 0;

	out_h = fmt("%s_ff.h", scratch);
	out_c = fmt("%s_ff.c", scratch);
	/* basename(3) and dirname(3) can modify the string, duplicate. */
	bname = fmt("%s", basename(scratch));
	dname = fmt("%s", basename(dirname(scratch)));
	gname = fmt("__MOTR_%s_%s_FF_H__", dname, bname);

	for (ch = gname; *ch != 0; ch++) {
		*ch = toupper(*ch);
		if (strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_", *ch) == NULL)
 			*ch = '_';
	}

	opt.go_basename  = bname;
	opt.go_guardname = gname;

	c = fopen(out_c, "w");
	if (c == NULL)
		err(EX_CANTCREAT, "cannot open \"%s\" for writing", out_c);

	h = fopen(out_h, "w");
	if (h == NULL)
		err(EX_CANTCREAT, "cannot open \"%s\" for writing", out_h);

	memset(&ctx, 0, sizeof ctx);
	memset(&ff, 0, sizeof ff);

	ff2c_context_init(&ctx, addr, buf.st_size);
	result = ff2c_parse(&ctx, &t);
	if (result != 0)
		err(EX_DATAERR, "cannot parse");

	ff2c_sem_init(&ff, t);

	opt.go_out = h;
	ff2c_h_gen(&ff, &opt);
	opt.go_out = c;
	ff2c_c_gen(&ff, &opt);

	ff2c_sem_fini(&ff);
	ff2c_term_fini(t);
	ff2c_context_fini(&ctx);

	fclose(h);
	fclose(c);

	free(out_c);
	free(out_h);
	free(gname);
	free(scratch);
	free(bname);
	free(dname);
	result = munmap(addr, buf.st_size);
	if (result == -1)
		warn("cannot munmap");
	result = close(fd);
	if (result == -1)
		warn("cannot close");
	return EXIT_SUCCESS;
}

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
