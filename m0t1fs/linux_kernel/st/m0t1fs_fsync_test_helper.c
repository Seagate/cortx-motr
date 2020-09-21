/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

/**
 * Creates a file in motr and try to fsync some data to it.
 */
int main(int argc, char **argv)
{
	int rv = 0;
	int fd = 0;
	char *str = "Hello World\n";
	char object_path[PATH_MAX];

	/* Check we were told the mount point. */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s /path/to/mount/point\n", argv[0]);
		exit(1);
	}

	/* Build a path for our to-create object. */
	rv = snprintf(object_path, sizeof(object_path), "%s/0:91837432", argv[1]);
	if (rv >= sizeof(object_path)) {
		fprintf(stderr, "Path overflow\n");
		exit(1);
	}

	fd = creat(object_path, 0600);
	if (fd >= 0) {
		/* Write some data. */
		rv = write(fd, str, strlen(str));

		/* Try and fsync the file. */
		rv = fsync(fd);

		/* fsync should return 0 for sucess. */
		fprintf(stderr, "fsync returned: %d\n", rv);
	} else {
		fprintf(stderr, "Failed to creat %s: %s\n",
				object_path, strerror(errno));
		exit(1);
	}

	return rv;
}
