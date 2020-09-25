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


#include <stdlib.h>		/* strtoull, strtoul */
#include <netdb.h>  /* gethostbyname_r */
#include <arpa/inet.h> /* inet_ntoa, inet_ntop */
#include <unistd.h> /* gethostname */
#include <errno.h>
#include <sys/time.h>     /* getrusage */
#include <sys/resource.h> /* getrusage */
#include <stdio.h>        /* fopen */
#include "lib/misc.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

uint64_t m0_strtou64(const char *str, char **endptr, int base)
{
	return strtoull(str, endptr, base);
}
M0_EXPORTED(m0_strtou64);

uint32_t m0_strtou32(const char *str, char **endptr, int base)
{
	return strtoul(str, endptr, base);
}
M0_EXPORTED(m0_strtou32);

M0_INTERNAL void m0_performance_counters(char *buf, size_t buf_len)
{
	struct rusage  usage;
	FILE          *proc_self_io;
	int            nr;
	int            rc;
	int            len;
	char               proc_io_buf[0x1000];
	unsigned long long rchar;
	unsigned long long wchar;
	unsigned long long syscr;
	unsigned long long syscw;
	unsigned long long read_bytes;
	unsigned long long write_bytes;
	unsigned long long cancelled_write_bytes;

	rc = getrusage(RUSAGE_SELF, &usage);
	if (rc == 0) {
		len = snprintf(buf, buf_len, "utime %ld.%06ld stime %ld.%06ld "
			       "maxrss %ld nvcsw %ld nivcsw %ld\n"
			       "minflt %ld majflt %ld "
			       "inblock %ld oublock %ld\n",
			       usage.ru_utime.tv_sec,
			       (long)usage.ru_utime.tv_usec,
			       usage.ru_stime.tv_sec,
			       (long)usage.ru_stime.tv_usec,
			       usage.ru_maxrss, usage.ru_nvcsw, usage.ru_nivcsw,
			       usage.ru_minflt, usage.ru_majflt,
			       usage.ru_inblock, usage.ru_oublock);
		if (len >= buf_len || len < 0)
			len = buf_len;
		buf_len -= len;
		buf     += len;
	}
	if (buf_len <= 1)
		return;
	proc_self_io = fopen("/proc/self/io", "r");
	if (proc_self_io != NULL) {
		nr = fscanf(proc_self_io, "%s %llu %s %llu %s %llu %s %llu "
		            "%s %llu %s %llu %s %llu",
		            proc_io_buf, &rchar, proc_io_buf, &wchar,
		            proc_io_buf, &syscr, proc_io_buf, &syscw,
		            proc_io_buf, &read_bytes, proc_io_buf,
		            &write_bytes, proc_io_buf, &cancelled_write_bytes);
		fclose(proc_self_io);
		if (nr == 14) {
			snprintf(buf, buf_len, "rchar %llu wchar %llu "
			         "syscr %llu syscw %llu\n"
			         "read_bytes %llu write_bytes %llu "
			         "cancelled_write_bytes %llu\n",
			         rchar, wchar, syscr, syscw,
			         read_bytes, write_bytes,
				 cancelled_write_bytes);
		}
	}
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
