/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * 
 * For any questions about this software or licensing, please email opensource@seagate.com
 * or cortx-questions@seagate.com.
 *
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_MEMORY
#if !defined(__KERNEL__)
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <libgen.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#endif

#include "lib/trace.h"

#include "lib/mutex.h"
#include "lib/mempressure.h"
#include "lib/memory.h"
#include "module/instance.h"
#include "lib/string.h"
#include "config.h"     /* HAVE_MEMPRESSURE */

/**
 * @addtogroup memory pressure
 *
 * This file contains implementation details of memory
 * pressure features event publish/subscribe.
 *
 * m0_mempressure_mod_init() 
 * 	It loads mempressure module.
 * 	it creates the level[ low, medium,
 * 		critical ] listener eventfds and epoll it.
 * 	listener thread wait on eventfds, if any mem pressure
 * 		trigger by memory manager.
 * 	If event found, it runs all the subscribed
 * 		 event callbacks.
 * 	event callback is a top half, it just increases 
 * 		the atomic counter, set by component objects.
 * 	component objects[ beck_tool ] when found atomic  couter
 * 		 has been set, it executes its bottom half.
 * 	component[ beck tool ]  event_bh  call builder_fini/ini.
 *
 * m0_mempressure_mod_fini() 
 * 	Unloads mempressure module.
 *
 * m0_mempressure_cb_add()
 * 	For subscribing for event callback event_th[ top half ]
 *
 * m0_mempressure_cb_del()
 * 	Unsubscribe event_th from mempresure.
 *
 * m0_mempressure_get()
 * 	Get current level[ mem pressure ]
 *
 * @todo Interface
 *  current implementation done for memory subsystem.
 *  Need to generalize for other subsystem resources.
 *
 *
 * @{
 */

#if !defined(__KERNEL__)
static int  thread_start(struct mempressure_obj *lobj);
static void listener_thread(struct mempressure_obj *lobj);
static void close_fd_on_exit(struct mempressure_obj *lobj);

static void mp_event_th_run(struct m0_mempressure *loc);
static void event_group_lock(struct m0_mempressure *mp_obj);
static void event_group_unlock(struct m0_mempressure *loc);
static int  fd_to_level(struct mempressure_obj *mobj, int fd);

static int path_snprintf(char *buf, size_t buflen,
				const char *format, ...);
static int system_do(char *buf, size_t buflen, const char *format, ...);

M0_TL_DESCR_DEFINE(runmq, "runq mp", static,
			struct m0_mempressure_cb, mc_linkage,
			mp_magic, M0_MP_MAGIC, M0_MP_RUNQ_MAGIC);
M0_TL_DEFINE(runmq, static, struct m0_mempressure_cb);

enum {
	MAX_EVENTS = 2,
	SLEN       = 32,
};

/**
 * cgroup files paths and groupname.
 */
static const char  mempath[]           = "/sys/fs/cgroup/memory/";
static const char  m_pressure_level[]  = "memory.pressure_level";
static const char  cg_event_controll[] = "cgroup.event_control";
static const char *level_name[] = {
	"low",
	"medium",
	"critical",
};
static char        c_group[SLEN];
#endif

/**
 * variadic path generating function.
 */
#if !defined(__KERNEL__)
static int path_snprintf(char *buf, size_t buflen, const char *format, ...)
{
	va_list ap;
	int     len;

	va_start(ap, format);
	len = vsnprintf(buf, buflen, format, ap);
	va_end(ap);
	if (len <= 0 || len >= buflen)
		return -1;
	return len;
}
#endif

/**
 * variadic command execute function.
 */
#if !defined(__KERNEL__)
static int system_do(char *buf, size_t buflen, const char *format, ...)
{
	va_list ap;
	int     res;

	va_start(ap, format);
	res = vsnprintf(buf, buflen, format, ap);
	va_end(ap);
	if (res == -1) {
		M0_LOG(M0_ERROR, "buffer error");
		return -1;
	}
	res = system(buf);
	if (res != 0)
		return -1;
	return res;
}
#endif

/**
 * group lock to protect subscriber event queue.
 */
#if !defined(__KERNEL__)
static void event_group_lock(struct m0_mempressure *mp_obj)
{
	m0_mutex_lock(&mp_obj->mp_gr_lock);
}
#endif

/**
 * Publishes a new event to all the subscribers.
 */
#if !defined(__KERNEL__)
static void mp_event_th_run(struct m0_mempressure *mp)
{
	struct m0_mempressure_cb *icb;

	M0_ENTRY();
	M0_LOG(M0_DEBUG, "mempressure event th for level %d.",
	       mp->mp_cur_level);
	m0_tl_for(runmq, &mp->mp_runmq, icb) {
		icb->mc_pressure(icb->level);
	} m0_tl_endfor;
	M0_LEAVE();
}
#endif

/**
 * unlock subscriber event queue.
 */
#if !defined(__KERNEL__)
static void event_group_unlock(struct m0_mempressure *mp_obj)
{
	m0_mutex_unlock(&mp_obj->mp_gr_lock);
}
#endif

/**
 * start event listener thread for any obvious event occur.
 *   validate [cgroup/mempresure/ecent_control] path
 *   create and run thread per level.
 */
#if !defined(__KERNEL__)
static int thread_start(struct mempressure_obj *lobj)
{
	int  result;
	int  fd;
	char mp_path[PATH_MAX];
	char cmd_line[LINE_MAX];

	M0_ENTRY();
	result = path_snprintf(mp_path, PATH_MAX, "%s/%s/%s",
			       mempath, c_group, m_pressure_level);
	if (result == -1)
		goto err_blk;
	lobj->mo_c_fd = open(mp_path, O_RDONLY);
	if (lobj->mo_c_fd == -1) {
		M0_LOG(M0_ERROR, "Cannot open file : %s  errono : %d.",
		       (char *)mp_path, errno);
		goto err_blk;
	}
	result = path_snprintf(lobj->mo_ec_path, PATH_MAX, "%s/%s/%s",
			       mempath, c_group, cg_event_controll);
	if (result == -1)
		goto err_blk;
	/* open event control files for all levels and register it. */
	for (fd = 0; fd  < M0_MPL_NR; fd++) {
		lobj->mo_e_fd[fd] = eventfd(0, O_NONBLOCK);
		if (lobj->mo_e_fd[fd] == -1) {
			M0_LOG(M0_ERROR, "eventfd() failed.");
			goto err_blk;
		}
		lobj->mo_p_ec[fd] = open(lobj->mo_ec_path, O_WRONLY);
		if (lobj->mo_p_ec[fd] == -1) {
			M0_LOG(M0_ERROR, "Cannot open file : %s  errono : %d.",
			       (char *)lobj->mo_ec_path, errno);
			goto err_blk;
		}
		result = path_snprintf(cmd_line, LINE_MAX, "%d %d %s",
				       lobj->mo_e_fd[fd],
				       lobj->mo_c_fd, level_name[fd]);
		if (result == -1)
			goto err_blk;
		M0_LOG(M0_DEBUG, "event control cmd_line: %s", (char*)cmd_line);
		result = write(lobj->mo_p_ec[fd], cmd_line, strlen(cmd_line) + 1);
		if (result == -1) {
			M0_LOG(M0_ERROR, "Cannot write to event_control: %s "
			       "errono : %d.", (char *)lobj->mo_ec_path, errno);
			goto err_blk;
		}
	}
	result = M0_THREAD_INIT(&lobj->mo_listener_t, struct mempressure_obj *,
				NULL, &listener_thread, lobj, "memp");

	M0_LEAVE();
	err_blk:
		return result;
}
#endif

/**
 *  close all the fds created by the listener thread.
 */
#if !defined(__KERNEL__)
static void close_fd_on_exit(struct mempressure_obj *lobj)
{
	int fd;

	M0_ENTRY();
	if (lobj->mo_c_fd >= 0) {
		close(lobj->mo_c_fd);
		lobj->mo_c_fd = -1;
	}
	for (fd = 0; fd  < M0_MPL_NR; fd++) {
		if (lobj->mo_e_fd[fd] >= 0) {
			close(lobj->mo_e_fd[fd]);
			lobj->mo_e_fd[fd] = -1;
		}
	}
	for (fd = 0; fd  < M0_MPL_NR; fd++) {
		if (lobj->mo_p_ec[fd] >= 0) {
			close(lobj->mo_p_ec[fd]);
			lobj->mo_p_ec[fd] = -1;
		}
	}
	M0_LEAVE();
}
#endif

/**
 * FD to level translation.
 */
#if !defined(__KERNEL__)
static int fd_to_level(struct mempressure_obj *mobj, int fd)
{
	int i;
	for ( i = 0; i < M0_MPL_NR; i++) {
		if (mobj->mo_e_fd[i] == fd)
			return i;
	}
	return -1;
}
#endif


/**
 * Listener thread function.
 * listen any event posted on eventfd.
 * publish the event ast during unlock to all subscriber.
 *
 */
#if !defined(__KERNEL__)
static void listener_thread(struct mempressure_obj *lobj)
{
	int      result;
	int      ep_fd = -1;
	int      rc;
	int      level;
	int      event_fd;
	int      fd;
	uint64_t red_val;
	struct   epoll_event events[M0_MPL_NR];
	struct   epoll_event read_event;

	M0_ENTRY();
	ep_fd = epoll_create1(0);
	if (ep_fd == -1 ) {
		M0_LOG(M0_ERROR, "epoll_create1 failed.");
		goto fail;
	}
	/* register all levels eventfds for epolloing. */
	for (fd = 0; fd < M0_MPL_NR; fd++) {
		read_event.events = EPOLLIN | EPOLLET | EFD_CLOEXEC;
		read_event.data.fd = lobj->mo_e_fd[fd];
		result = epoll_ctl(ep_fd, EPOLL_CTL_ADD, lobj->mo_e_fd[fd], &read_event);
		if (ep_fd == -1 ) {
			M0_LOG(M0_ERROR, "epoll_ctl failed.");
			goto fail;
		}
	}

	while (1) {
		if (!lobj->mo_p_ref->mp_active)
			break;
		lobj->mo_p_ref->mp_cur_level = -1;
		result = epoll_wait(ep_fd, &events[0], M0_MPL_NR, 50000);
		if ( result != -1) {
			level = -1;
			for ( rc = 0; rc < result; rc++) {
				if (events[rc].events & EPOLLIN) {
					event_fd = events[rc].data.fd;
					result = eventfd_read(event_fd, &red_val);
					if (result == 0) {
						level = fd_to_level(lobj, event_fd);
						break;
					} else
						M0_LOG(M0_ERROR, "Event read failed.");
				}
			}
			if (level != -1) {
				event_group_lock(lobj->mo_p_ref);
				lobj->mo_p_ref->mp_cur_level = level;
				M0_LOG(M0_NOTICE, "Event arrived, level : %d.",
				       level);
				/* Publish the messages to subscribers. */
				mp_event_th_run(lobj->mo_p_ref);
				event_group_unlock(lobj->mo_p_ref);
			}
		} else if (result == -1) {
			M0_LOG(M0_ERROR, "epoll error. : %d\n", result);
			goto fail;
		}
	}

	fail:
		if (ep_fd >= 0) {
			close(ep_fd);
		}
	M0_LEAVE();
}
#endif

/**
 *  Get method for current event if exist?
 */
M0_INTERNAL enum m0_mempressure_level m0_mempressure_get(void)
{
#if !defined(__KERNEL__)

#ifndef HAVE_MEMPRESSURE
	return M0_ERR(-ENOSYS);;
#endif
	struct m0_mempressure *mp_obj = m0_get()->i_moddata[M0_MODULE_MP];
	return mp_obj->mp_cur_level;
#else
	return M0_ERR(-ENOSYS);
#endif
}
M0_EXPORTED(m0_mempressure_get);

/**
 * subscribe interface for registering event_th.
 */
M0_INTERNAL int m0_mempressure_cb_add(struct m0_mempressure_cb *cb)
{
#if !defined(__KERNEL__)

#ifndef HAVE_MEMPRESSURE
	M0_LOG(M0_ERROR, "m0_mempressure_cb_add not supported.");
	return M0_ERR(-ENOSYS);;
#endif
	struct m0_mempressure *mp = m0_get()->i_moddata[M0_MODULE_MP];

	M0_ENTRY();
	M0_LOG(M0_DEBUG, "m0_mempressure_cb_add: %p.", cb);
	event_group_lock(mp);
	runmq_tlink_init_at_tail(cb, &mp->mp_runmq);
	event_group_unlock(mp);
	M0_LEAVE();
	return 0;
#else
	M0_LOG(M0_ERROR, "m0_mempressure_cb_add not supported.");
	return M0_ERR(-ENOSYS);;
#endif
}
M0_EXPORTED(m0_mempressure_cb_add);

/**
 * unsubscribe interface for event_th.
 */
M0_INTERNAL void m0_mempressure_cb_del(struct m0_mempressure_cb *cb)
{
#if !defined(__KERNEL__)

#ifndef HAVE_MEMPRESSURE
	M0_LOG(M0_ERROR, "m0_mempressure_cb_add not supported.");
	return;
#endif
	struct m0_mempressure *mp = m0_get()->i_moddata[M0_MODULE_MP];;

	M0_ENTRY();
	M0_LOG(M0_DEBUG, "m0_mempressure_cb_del: %p.", cb);
	event_group_lock(mp);
	M0_ASSERT(runmq_tlist_contains(&mp->mp_runmq, cb));
	runmq_tlink_del_fini(cb);
	event_group_unlock(mp);
	M0_LEAVE();
#else
	M0_LOG(M0_ERROR, "m0_mempressure_cb_add not supported.");
	return;
#endif
}
M0_EXPORTED(m0_mempressure_cb_del);

/**
 * mod init method.
 *  initialize group lock.
 *  initialize event queue.
 *  create/run three level [ low, medium, critical ] non-blocking threads. 
 */
M0_INTERNAL int m0_mempressure_mod_init()
{
#if !defined(__KERNEL__)
	int    result;
	int    pid;
	char   cgroup_cmd[PATH_MAX];

	struct m0_mempressure  *mp_obj;
	struct mempressure_obj *mobj;

#ifndef HAVE_MEMPRESSURE
	return 0;
#endif
	M0_ENTRY();
	M0_ALLOC_PTR(mp_obj);
	M0_ASSERT(mp_obj != NULL);
	pid = getpid();
	result = path_snprintf(c_group, SLEN, "motr-%d", pid);
	if (result == -1)
		goto err_blk;
	result = system_do(cgroup_cmd, PATH_MAX,
			   "/bin/cgcreate -g memory:/%s", c_group);
	if (result == -1)
		goto err_blk;
	M0_LOG(M0_DEBUG, "cgcreate cgroup %s created.", (char*)c_group);
	result = system_do(cgroup_cmd, PATH_MAX,
			   "echo %d >>/sys/fs/cgroup/memory/%s/tasks",
			   pid, c_group);
	if (result == -1)
		goto err_blk;
	M0_LOG(M0_NOTICE, "registered pid: %d in cgroup: %s",
	       pid, (char*)cgroup_cmd);
	mp_obj->mp_active = true;
	m0_mutex_init(&mp_obj->mp_gr_lock);
	runmq_tlist_init(&mp_obj->mp_runmq);

	mobj = &mp_obj->mp_listener_obj;
	mobj->mo_p_ref = mp_obj;
	result = thread_start(mobj);
	if (result == -1) {
		m0_free(mp_obj);
		M0_LOG(M0_ERROR, "thread_start failed.");
	}
	M0_LOG(M0_NOTICE, "event listener Thread created.");

	if (result != -1)
		m0_get()->i_moddata[M0_MODULE_MP] = mp_obj;

	M0_LEAVE();
	err_blk:
		if(result != 0) {
			mp_obj->mp_active = false;
			return M0_ERR(-ENOSYS);
		}
	return 0;
#else
	return 0;
#endif
}
M0_EXPORTED(m0_mempressure_mod_init);


/**
 * finialize the mempressure mod.
 */
M0_INTERNAL void m0_mempressure_mod_fini()
{
#if !defined(__KERNEL__)
	struct mempressure_obj  mobj;
	char                    cgroup_cmd[PATH_MAX];

#ifndef HAVE_MEMPRESSURE
	return;
#endif
	struct m0_mempressure  *mp_obj = m0_get()->i_moddata[M0_MODULE_MP];

	M0_ENTRY();
	event_group_lock(mp_obj);
	mp_obj->mp_active = false;
	event_group_unlock(mp_obj);
	mobj = mp_obj->mp_listener_obj;
	m0_thread_join(&mobj.mo_listener_t);
	m0_thread_fini(&mobj.mo_listener_t);
	close_fd_on_exit(&mp_obj->mp_listener_obj);
	m0_mutex_fini(&mp_obj->mp_gr_lock);
	M0_LOG(M0_DEBUG, "cgdelete cgroup : %s.", (char*)c_group);
	/* remove the control group. */
	system_do(cgroup_cmd, PATH_MAX, "/bin/cgdelete -g memory:/%s",
		  c_group);
	m0_free(mp_obj);
	M0_LEAVE();
#else
	return;
#endif
}
M0_EXPORTED(m0_mempressure_mod_fini);

/** @} end of mempressure */
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
