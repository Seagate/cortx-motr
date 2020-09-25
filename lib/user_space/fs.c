/* -*- C -*- */
/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

#include <unistd.h>		/* close(2), getcwd, get_current_dir_name */
#include <dirent.h>		/* opendir(3) */
#include <fcntl.h>		/* open(2), F_FULLFSYNC */
#include <errno.h>
#include "lib/memory.h"         /* M0_ALLOC_ARR */

M0_INTERNAL int m0_cleandir(const char *dir)
{
	struct dirent *de;
	DIR           *d;
	int            rc;
	int            fd;

	fd = open(dir, O_RDONLY|O_DIRECTORY);
	if (fd == -1) {
		if (errno == ENOENT)
			return M0_RC(0);
		rc = -errno;
		M0_LOG(M0_NOTICE, "open(%s) failed: rc=%d", dir, rc);
		return M0_ERR(rc);
	}
	d = opendir(dir);
	if (d != NULL) {
		while ((de = readdir(d)) != NULL)
			unlinkat(fd, de->d_name, 0);
		closedir(d);
	}
	close(fd);

	rc = rmdir(dir) == 0 ? 0 : -errno;
	if (rc != 0)
		M0_LOG(M0_ERROR, "rmdir(%s) failed: rc=%d", dir, rc);
	return M0_RC(rc);
}

M0_INTERNAL int m0_file_read(const char *path, char **out)
{
	FILE  *f;
	long   size;
	size_t n;
	int    rc = 0;

	M0_ENTRY("path=`%s'", path);

	f = fopen(path, "rb");
	if (f == NULL)
		return M0_ERR_INFO(-errno, "path=`%s'", path);

	rc = fseek(f, 0, SEEK_END);
	if (rc == 0) {
		size = ftell(f);
		rc = fseek(f, 0, SEEK_SET);
	}
	if (rc != 0) {
		fclose(f);
		return M0_ERR_INFO(-errno, "fseek() failed: path=`%s'", path);
	}
	/* it should be freed by the caller */
	M0_ALLOC_ARR(*out, size + 1);
	if (*out != NULL) {
		n = fread(*out, 1, size + 1, f);
		M0_ASSERT_INFO(n == size, "n=%zu size=%ld", n, size);
		if (ferror(f))
			rc = -errno;
		else if (!feof(f))
			rc = M0_ERR(-EFBIG);
		else
			(*out)[n] = '\0';
	} else {
		rc = M0_ERR(-ENOMEM);
	}

	fclose(f);
	return M0_RC(rc);
}

M0_INTERNAL char *m0_getcwd(void)
{
#if defined(M0_LINUX)
	return get_current_dir_name();
#elif defined(M0_DARWIN)
	return getcwd(NULL, 0);
#endif
}

M0_INTERNAL int m0_fdatasync(int fd)
{
#if defined(M0_LINUX)
	return fdatasync(fd);
#elif defined(M0_DARWIN)
	return fcntl(fd, F_FULLFSYNC);
#endif
}

M0_INTERNAL int m0_syncfs(int fd)
{
#if defined(M0_LINUX)
	return syncfs(fd);
#elif defined(M0_DARWIN)
	sync();
	return 0;
#endif
}

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
