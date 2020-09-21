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


#include "ut/ut_internal.h"
#include "ut/ut.h"                 /* m0_ut_redirect */
#include "lib/finject.h"           /* m0_fi_fpoint_data */
#include "lib/finject_internal.h"  /* m0_fi_fpoint_type_from_str */
#include "lib/string.h"            /* m0_strdup */
#include "lib/errno.h"             /* EINVAL */
#include "lib/memory.h"            /* m0_free */
#include <yaml.h>                  /* yaml_parser_t */
#include <stdlib.h>                /* system */
#include <err.h>                   /* warn */
#include <sys/stat.h>              /* mkdir */
#include <unistd.h>                /* dup */

static int sandbox_remove(const char *sandbox)
{
	char *cmd;
	int   rc;

	if (sandbox == NULL)
		return 0;

	rc = asprintf(&cmd, "rm -fr '%s'", sandbox);
	M0_ASSERT(rc > 0);

	rc = system(cmd);
	if (rc != 0)
		warn("*WARNING* sandbox cleanup at \"%s\" failed: %i\n",
		     sandbox, rc);

	free(cmd);
	return rc;
}

int m0_ut_sandbox_init(const char *dir)
{
	int rc;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	if (dir == NULL)
		return 0;

	rc = sandbox_remove(dir);
	if (rc != 0)
		return rc;

	rc = mkdir(dir, 0700) ?: chdir(dir);
	if (rc != 0)
		(void)sandbox_remove(dir);
	return rc;
}

void m0_ut_sandbox_fini(const char *dir, bool keep)
{
	int rc;

	rc = chdir("..");
	M0_ASSERT(rc == 0);

	if (!keep)
		sandbox_remove(dir);
}

M0_INTERNAL void m0_stream_redirect(FILE * stream, const char *path,
				    struct m0_ut_redirect *redir)
{
	FILE *result;

	/*
	 * This solution is not portable and will only work on systems which
	 * supports dup(2) and dup2(2) system calls (these are supported
	 * in Linux).
	 */
	redir->ur_stream = stream;
	fflush(stream);
	fgetpos(stream, &redir->ur_pos);
	redir->ur_oldfd = fileno(stream);
	redir->ur_fd = dup(redir->ur_oldfd);
	M0_ASSERT(redir->ur_fd != -1);
	result = freopen(path, "a+", stream);
	M0_ASSERT(result != NULL);
}

M0_INTERNAL void m0_stream_restore(const struct m0_ut_redirect *redir)
{
	int result;

	/*
	 * see comment in m0_stream_redirect() for detailed information
	 * about how to redirect and restore standard streams
	 */
	fflush(redir->ur_stream);
	result = dup2(redir->ur_fd, redir->ur_oldfd);
	M0_ASSERT(result != -1);
	close(redir->ur_fd);
	clearerr(redir->ur_stream);
	fsetpos(redir->ur_stream, &redir->ur_pos);
}

M0_INTERNAL bool m0_error_mesg_match(FILE * fp, const char *mesg)
{
	enum {
		MAXLINE = 1025,
	};

	char line[MAXLINE];

	M0_PRE(fp != NULL);
	M0_PRE(mesg != NULL);

	fseek(fp, 0L, SEEK_SET);
	memset(line, '\0', MAXLINE);
	while (fgets(line, MAXLINE, fp) != NULL) {
		if (strncmp(mesg, line, strlen(mesg)) == 0)
			return true;
	}
	return false;
}

/** @} end of ut group */

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
