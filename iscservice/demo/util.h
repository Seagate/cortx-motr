/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_ISCSERVICE_DEMO_UTIL_H__
#define __MOTR_ISCSERVICE_DEMO_UTIL_H__

#include "motr/client.h"     /* m0_entity */
#include "iscservice/isc.h"  /* m0_fop_isc */

extern bool m0trace_on;

/**
 * Initialise Object Store
 *
 * idx selects which local endpoint and proc fid to use from the
 * startup configuration file at $HOME/.m0util/ (starts from 0).
 */
int m0util_init(int idx);
int m0util_free(void);

/**
 * Open m0-entity (object)
 *
 * @retval 0 on success
 */
int open_entity(struct m0_entity *entity);

/**
 * Return parity group size for object.
 */
uint64_t m0util_m0gs(struct m0_obj *obj, int tier);

int m0util_setrc(char *progname);

/**
 * Loads a library in all available m0ds.
 */

/* Import */
struct m0_fid;
struct m0util_isc_req;
struct m0_buf;
struct m0_rpc_link;
enum m0_conf_service_type;

struct m0_layout_io_plop;

enum m0util_buffer_len {
	/** 32K */
	CBL_DEFAULT_MAX = M0_RPC_DEF_MAX_RPC_MSG_SIZE >> 2,
	CBL_DEFAULT_MIN = 256,
};

/** A request holding all parameters relevant to a computation. */
struct m0util_isc_req {
	/** Buffer to store returned result. */
	struct m0_buf          cir_result;
	/** Error code for the computation. */
	int                    cir_rc;
	/** RPC session of the ISC service. */
	struct m0_rpc_session *cir_rpc_sess;
	/** FOP for ISC service. */
	struct m0_fop_isc      cir_isc_fop;
	/** A generic fop for ISC service. */
	struct m0_fop          cir_fop;
	/** Semaphore to signal the reply to. */
	struct m0_semaphore   *cir_sem;
	/** plop this request corresponds to. */
	struct m0_layout_plop *cir_plop;
	/** all reqs list link */
	struct m0_list_link    cir_link;
};

extern struct m0_semaphore isc_sem;
extern struct m0_list      isc_reqs;

#define m0util_isc_reqs_teardown(req) \
  while (!m0_list_is_empty(&isc_reqs) && \
         (req = m0_list_entry(isc_reqs.l_head, \
                              struct m0util_isc_req, cir_link)) && \
         (m0_list_del(&req->cir_link), true))

/**
 * Loads a library to all Motr instances hosting an ISC service.
 * It assumes that library is located at identical path on all
 * Motr instances.
 * */
int m0util_isc_api_register(const char *libpath);

/** RPC link for ISC service specified by the fid. */
struct m0_rpc_link * m0util_isc_rpc_link_get(struct m0_fid *svc_fid);

/**
 * Returns the fid of next service of given type in configuration.
 * @param[in]  svc_fid Represents the current service fid. In order
 *		       to fetch the fid of the first service of given
 *		       this should be set to M0_FID0.
 * @param[in]  s_type  Type of the service being searched for. eg. M0_CST_ISC
 *                     for ISC service. See more types in conf/schema.h
 * @param[out] nxt_fid "fid" associated with the next service.
 * @retval     0       When the next service is found.
 * @retval    -ENOENT  When no service is found.
 */
int m0util_isc_nxt_svc_get(struct m0_fid *svcc_fid, struct m0_fid *nxt_fid,
			    enum m0_conf_service_type s_type);

/**
 * Prepares a request using provided parameters.
 * @param[in] args      Holds arguments for the computation.
 * @param[in] comp      The unique fid associated with the computation.
 * @param[in] iopl      IO plop associated with the request.
 * @param[in] reply_len Expected length of reply buffer. This is allowed to
 *                      be greater than the actual length of reply.
 */
int m0util_isc_req_prepare(struct m0util_isc_req *, struct m0_buf *args,
			   const struct m0_fid *comp,
			   struct m0_layout_io_plop *iopl, uint32_t reply_len);

/**
 * Sends a request asynchronously. The request is added to the
 * isc_reqs list. On reply receipt, isc_sem(aphore) is up-ped.
 * The received reply is populated at req->cir_result.
 * The error code is returned at req->cir_rc.
 */
int m0util_isc_req_send(struct m0util_isc_req *req);

/**
 * Sends a request and waits till reply is received. The received reply is
 * populated in req->cir_result. The error code is returned at req->cir_rc.
 */
int m0util_isc_req_send_sync(struct m0util_isc_req *req);

/**
 * Finalizes the request, including input and result buffers.
 */
void m0util_isc_req_fini(struct m0util_isc_req *req);

extern unsigned unit_size; /* in KiBs, default 4 */
extern struct m0_realm uber_realm;

int alloc_segs(struct m0_bufvec *data, struct m0_indexvec *ext,
	       struct m0_bufvec *attr, uint64_t bsz, uint32_t cnt);
void free_segs(struct m0_bufvec *data, struct m0_indexvec *ext,
	       struct m0_bufvec *attr);
uint64_t set_exts(struct m0_indexvec *ext, uint64_t off, uint64_t bsz);

extern int trace_level;
extern char *prog;

enum {
	LOG_ERROR = 0,
	LOG_DEBUG = 1,
	LOG_DEBUG2 = 2
};

#define LOG(_fmt, ...) \
  fprintf(stderr, "%s: %s():%d: "_fmt, prog, __func__, __LINE__, ##__VA_ARGS__)
#define ERR(_fmt, ...) if (trace_level >= LOG_ERROR) LOG(_fmt, ##__VA_ARGS__)
#define ERRS(_fmt, ...) if (trace_level >= LOG_ERROR) \
	LOG(_fmt ": %s\n", ##__VA_ARGS__, strerror(errno))
#define DBG(_fmt, ...) if (trace_level >= LOG_DEBUG) LOG(_fmt, ##__VA_ARGS__)
#define DBG2(_fmt, ...) if (trace_level >= LOG_DEBUG2) LOG(_fmt, ##__VA_ARGS__)

#define	CHECK_BSZ_ARGS(bsz, m0bs) \
  if ((bsz) < 1 || (bsz) % PAGE_SIZE) { \
    ERR("bsz(%lu) must be multiple of %luK\n", (m0bs), PAGE_SIZE/1024); \
    return -EINVAL; \
  } \
  if ((m0bs) < 1 || (m0bs) % (bsz)) { \
    ERR("bsz(%lu) must divide m0bs(%lu)\n", (bsz), (m0bs)); \
    return -EINVAL; \
  }


#endif /* __MOTR_ISCSERVICE_DEMO_UTIL_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
