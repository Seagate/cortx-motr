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


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0D
#include "lib/trace.h"

#include <stdio.h>            /* printf */
#include <unistd.h>           /* pause */
#include <stdlib.h>           /* atoi */
#include <err.h>              /* warnx */
#include <signal.h>           /* sigaction */
#include <sys/time.h>
#include <sys/resource.h>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/misc.h"              /* M0_SET0 */
#include "lib/user_space/trace.h"  /* m0_trace_set_buffer_size */

#include "motr/setup.h"
#include "motr/init.h"
#include "motr/version.h"
#include "module/instance.h"  /* m0 */
#include "net/lnet/lnet.h"
#include "net/sock/sock.h"
#include "reqh/reqh_service.h"
#include "motr/process_attr.h"
#include "ha/note.h"          /* M0_NC_ONLINE */

/**
   @addtogroup m0d
   @{
 */

/**
   Represents various network transports supported
   by a particular node in a cluster.
 */
static struct m0_net_xprt *cs_xprts[] = {
	&m0_net_lnet_xprt,
#ifndef __KERNEL__
	&m0_net_sock_xprt
	/*&m0_net_libfabric_xprt*/
#endif
};

/* Signal handler result */
enum result_status
{
	/* Default value */
	M0_RESULT_STATUS_WORK    = 0,
	/* Stop work Motr instance */
	M0_RESULT_STATUS_STOP    = 1,
	/* Restart Motr instance */
	M0_RESULT_STATUS_RESTART = 2,
};

extern volatile sig_atomic_t gotsignal;
static bool regsignal = false;

/**
   Signal handler registered so that pause()
   returns in order to trigger proper cleanup.
 */
static void cs_term_sig_handler(int signum)
{
	gotsignal = signum == SIGUSR1 ? M0_RESULT_STATUS_RESTART :
					M0_RESULT_STATUS_STOP;
}

/**
   Registers signal handler to catch SIGTERM, SIGINT and
   SIGQUIT signals and pauses the Motr process.
   Registers signal handler to catch SIGUSR1 signals to
   restart the Motr process.
 */
static int cs_register_signal(void)
{
	struct sigaction        term_act;
	int rc;

	regsignal = false;
	gotsignal = M0_RESULT_STATUS_WORK;
	term_act.sa_handler = cs_term_sig_handler;
	sigemptyset(&term_act.sa_mask);
	term_act.sa_flags = 0;

	rc = sigaction(SIGTERM, &term_act, NULL) ?:
		sigaction(SIGINT,  &term_act, NULL) ?:
		sigaction(SIGQUIT, &term_act, NULL) ?:
		sigaction(SIGUSR1, &term_act, NULL);
	if (rc == 0)
		regsignal = true;
	return rc;
}

static int cs_wait_signal(void)
{
	M0_PRE(regsignal);
	m0_console_printf("Press CTRL+C to quit.\n");
	m0_console_flush();
	do {
		pause();
	} while (!gotsignal);

	return gotsignal;
}

M0_INTERNAL int main(int argc, char **argv)
{
	static struct m0       instance;
	int                    trace_buf_size;
	int                    result;
	int                    rc;
	struct m0_motr         motr_ctx;
	struct rlimit          rlim = {10240, 10240};

	if (argc > 1 &&
	    (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
		m0_build_info_print();
		exit(EXIT_SUCCESS);
	}

	if (argc > 2 && strcmp(argv[1], "--trace-buf-size-mb") == 0) {
		trace_buf_size = atoi(argv[2]); /* in MiB */
		if (trace_buf_size > 0 &&
		    m0_trace_set_buffer_size((size_t)trace_buf_size *
					     1024 * 1024) == 0)
		{
			argv[2] = argv[0];
			argv += 2;
			argc -= 2;
		} else {
			if (trace_buf_size <= 0)
				m0_error_printf("motr: trace buffer size should"
				                " be greater than zero "
				                "(was %i)\n", trace_buf_size);
			exit(EXIT_FAILURE);
		}
	}

	rc = setrlimit(RLIMIT_NOFILE, &rlim);
	if (rc != 0) {
		warnx("\n Failed to setrlimit\n");
		goto out;
	}

	rc = cs_register_signal();
	if (rc != 0) {
		warnx("\n Failed to register signals\n");
		goto out;
	}

init_m0d:
	gotsignal = M0_RESULT_STATUS_WORK;

	errno = 0;
	M0_SET0(&instance);
	rc = m0_init(&instance);
	if (rc != 0) {
		warnx("\n Failed to initialise Motr \n");
		goto out;
	}

start_m0d:
	M0_SET0(&motr_ctx);
	rc = m0_cs_init(&motr_ctx, cs_xprts, ARRAY_SIZE(cs_xprts), stderr, false);
	if (rc != 0) {
		warnx("\n Failed to initialise Motr \n");
		goto cleanup2;
	}

	/*
	 * Prevent from automatic process fid generation by setting the context
	 * up with a dummy fid of non-process type.
	 */
	motr_ctx.cc_reqh_ctx.rc_fid = M0_FID_INIT(0, 1);
	/*
	 * Process FID specification is mandatory for m0d. Motr instance setup
	 * is going to stumble upon fid type precondition in m0_reqh_init()
	 * unless real process fid is present in argv.
	 */
	rc = m0_cs_setup_env(&motr_ctx, argc, argv);
	if (rc != 0)
		goto cleanup1;

	rc = m0_cs_start(&motr_ctx);
	if (rc == 0) {
		/* For st/m0d-signal-test.sh */
		m0_console_printf("Started\n");
		m0_console_flush();

#ifdef HAVE_SYSTEMD
		/*
	 	 * From the systemd's point of view, service can be considered as
	 	 * started when it can handle incoming connections, which is true
	 	 * after m0_cs_start() is called.
	 	 */
		rc = sd_notify(0, "READY=1");
		if (rc < 0)
			warnx("systemd READY notification failed, rc=%d\n", rc);
		else if (rc == 0)
			warnx("systemd notifications not allowed\n");
		else
			warnx("systemd READY notification successful\n");
		rc = 0;
#endif
		result = cs_wait_signal();
		if (gotsignal)
			warnx("got signal %d", gotsignal);
	}

	if (rc == 0 && result == M0_RESULT_STATUS_RESTART) {
		/*
		 * Note! A very common cause of failure restart is
		 * non-finalize (non-clean) any subsystem
		 */
		m0_cs_fini(&motr_ctx);
restart_signal:
		m0_quiesce();

		gotsignal = M0_RESULT_STATUS_WORK;

		rc = m0_cs_memory_limits_setup(&instance);
		if (rc != 0) {
			warnx("\n Failed to set process memory limits Motr \n");
			goto out;
		}

		rc = m0_resume(&instance);
		if (rc != 0) {
			warnx("\n Failed to reconfigure Motr \n");
			goto out;
		}

		/* Print to m0.log for ./sss/st system test*/
		m0_console_printf("Restarting\n");
		m0_console_flush();

		goto start_m0d;
	}

	if (rc == 0) {
		/* Ignore cleanup labels signal handling for normal start. */
		gotsignal = M0_RESULT_STATUS_WORK;
	}

cleanup1:
	m0_cs_fini(&motr_ctx);
	if (gotsignal) {
		if (gotsignal == M0_RESULT_STATUS_RESTART)
			goto restart_signal;
		gotsignal = M0_RESULT_STATUS_WORK;
	}
cleanup2:
	m0_fini();
	if (gotsignal == M0_RESULT_STATUS_RESTART)
		goto init_m0d;
out:
	errno = rc < 0 ? -rc : rc;
	return errno;
}

/** @} endgroup m0d */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
