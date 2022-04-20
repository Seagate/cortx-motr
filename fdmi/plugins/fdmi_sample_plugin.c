/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"

#include "motr/client.h"
#include "fid/fid.h"
#include "motr/client_internal.h"
#include "lib/getopts.h"	/* M0_GETOPTS */
#include "fdmi/fdmi.h"
#include "fdmi/plugin_dock.h"
#include "fdmi/service.h"
#include "reqh/reqh.h"

/**
 * Plugin sample conf params. Here _fsp means "fdmi sample plugin"
 * and _spp - "sample plugin params".
 */
struct m0_fsp_params {
	char          *spp_local_addr;
	char          *spp_hare_addr;
	char          *spp_profile_fid;
	char          *spp_process_fid;
	char          *spp_fdmi_plugin_fid_s;
	struct m0_fid  spp_fdmi_plugin_fid;
	bool           spp_output_strings;

	/**
	 * To add an extra reference on this record. When the record is
	 * consumed or it is stored in other persistent storage, pass
	 * the FDMI record ID back to this plugin (in stdin). At that time,
	 * plugin will release this FDMI record by calling the plugin dock API
	 * `.fpo_release_fdmi_rec()` .
	 */
	bool           spp_extra_ref;
};

/**
 * This is the FDMI plugin program. Its purpose is to start the listener
 * for the special type of the FDMI events and forward them to the fdmi_app
 * wrapper written in python. The communication is done via stdout.
 */

/**
 * Storage for the program run parameters passed from console or the wrapper
 * application.
 */
struct m0_fsp_params fsp_params;

/**
 * The main loop semaphore. The main loop here is just sleeping most of
 * the time and waiting for some stop signal (e.g ctrl+c) from the python
 * fdmi_app that controls this plugin program.
 *
 * The main job is done in one of the Motr locality threads, that handles
 * the fops with FDMI data and call the callback function installed by this
 * program if the type of the plugin match is found.
 */
static struct m0_semaphore fsp_sem;
volatile static int terminated = 0;
/**
 * This program is basically adding the new client to the cluster. To do so
 * we need to specify the client endpoint and the address of the cluster
 * configuration service. This structure is used for delivering this info
 * to the Motr core.
 */
static struct m0_config	fsp_conf = {};

/**
 * Pointer to the initialized m0_client structure denoting our program
 * functionality.
 */
static struct m0_client	*fsp_client = NULL;

/* DIX service (client interface for kv storage) config. */
static struct m0_idx_dix_config	fsp_dix_conf = {};

/**
 * The vector of the FDMI plugin callback operations. They are called by
 * the Motr when the new FDMI event occurs.
 */
const struct m0_fdmi_pd_ops *fsp_pdo;

/**
 * The handle of the local FDMI service that we start. Its purpose is to
 * receive the FDMI rpcs, find the local consumer (this program) and send
 * forward them to the consumer plugin in the form of FDMI records.
 */
static struct m0_reqh_service *fsp_fdmi_service = NULL;

static void dump_fol_rec_to_json(struct m0_uint128 *rec_id,
				 struct m0_fol_rec *rec)
{
	struct m0_fol_frag *frag;
	int i, j;

	m0_tl_for(m0_rec_frag, &rec->fr_frags, frag) {
		struct m0_fop_fol_frag *fp_frag = frag->rp_data;
		struct m0_cas_op *cas_op = fp_frag->ffrp_fop;
		struct m0_cas_recv cg_rec = cas_op->cg_rec;
		struct m0_cas_rec *cr_rec = cg_rec.cr_rec;

		for (i = 0; i < cg_rec.cr_nr; i++) {
			const unsigned char *addr;
			int len;

			printf("{ \"opcode\": \"%d\", ",
				fp_frag->ffrp_fop_code);

			printf("\"rec_id\": \""U128X_F"\", ",
				U128_P(rec_id));

			printf("\"fid\": \""FID_F"\", ",
				FID_P(&cas_op->cg_id.ci_fid));

			len  = cr_rec[i].cr_key.u.ab_buf.b_nob;
			addr = cr_rec[i].cr_key.u.ab_buf.b_addr;
			if (fsp_params.spp_output_strings) {
				printf("\"cr_key\": \"%.*s\", ", len, addr);
			} else {
				printf("\"cr_key\": \"");
				for (j = 0; j < len; j++)
					printf("%02x", addr[j]);
				printf("\", ");
			}
			len  = cr_rec[i].cr_val.u.ab_buf.b_nob;
			addr = cr_rec[i].cr_val.u.ab_buf.b_addr;
			if (len > 0) {
				if (fsp_params.spp_output_strings) {
					printf("\"cr_val\": \"%.*s\"",
						len, addr);
				} else {
					printf("\"cr_val\": \"");
					for (j = 0; j < len; j++)
						printf("%02x", addr[j]);
					printf("\"");
				}
			} else {
				printf("\"cr_val\": \"0\"");
			}
			printf(" }\n");
		}
	} m0_tl_endfor;
}

static void fsp_usage(void)
{
	fprintf(stderr,
		"Usage: fdmi_sample_plugin "
		"-l local_addr -h ha_addr -p profile_fid -f process_fid "
		"-g fdmi_plugin_fid [-s] [-r]\n"
		"Use -? or -i for more verbose help on common arguments.\n"
		"Usage example for common arguments: \n"
		"fdmi_sample_plugin -l 192.168.52.53@tcp:12345:4:1 "
		"-h 192.168.52.53@tcp:12345:1:1 "
		"-p 0x7000000000000001:0x37 -f 0x7200000000000001:0x19 "
		"-g 0x6c00000000000001:0x51"
		"-s output the key/val as a string. Otherwise as hex."
		"-r plugin adds an extra reference on the record and then "
		"   release it when it is consumed"
		"\n");
}

/**
 * @retval 0      Success.
 * @retval -Exxx  Error.
 */
static int fsp_args_parse(struct m0_fsp_params *params, int argc, char ** argv)
{
	int rc = 0;

	params->spp_local_addr 	= NULL;
	params->spp_hare_addr   = NULL;
	params->spp_profile_fid = NULL;
	params->spp_process_fid = NULL;
	params->spp_fdmi_plugin_fid_s = NULL;
	params->spp_output_strings = false;
	params->spp_extra_ref = false;

	rc = M0_GETOPTS("fdmi_sample_plugin", argc, argv,
			M0_HELPARG('?'),
			M0_VOIDARG('i', "more verbose help",
				   LAMBDA(void, (void) {
						   fsp_usage();
						   exit(0);
					   })),
			M0_STRINGARG('l', "Local endpoint address",
				     LAMBDA(void, (const char *string) {
						     params->spp_local_addr = (char*)string;
					     })),
			M0_STRINGARG('h', "HA address",
				     LAMBDA(void, (const char *str) {
						     params->spp_hare_addr = (char*)str;
					     })),
			M0_STRINGARG('f', "Process FID",
				     LAMBDA(void, (const char *str) {
						     params->spp_process_fid = (char*)str;
					     })),
			M0_STRINGARG('p', "Profile options for client",
				     LAMBDA(void, (const char *str) {
						     params->spp_profile_fid = (char*)str;
					     })),
			M0_STRINGARG('g', "FDMI plugin fid",
				     LAMBDA(void, (const char *str) {
						     params->spp_fdmi_plugin_fid_s =
							     (char*)str;
					     })),
			M0_VOIDARG('s', "output key/val as string",
				   LAMBDA(void, (void) {
						     params->spp_output_strings = true;
					   })),
			M0_VOIDARG('r', "adding an extra ref count",
				   LAMBDA(void, (void) {
						     params->spp_extra_ref = true;
					   })));
	if (rc != 0)
		return M0_ERR(rc);

	/* All mandatory params must be defined. */
	if (params->spp_local_addr == NULL  || params->spp_hare_addr == NULL   ||
	    params->spp_profile_fid == NULL || params->spp_process_fid == NULL ||
	    params->spp_fdmi_plugin_fid_s == NULL) {
		fsp_usage();
		return M0_ERR(-EINVAL);
	}

	rc = m0_fid_sscanf(params->spp_fdmi_plugin_fid_s, &params->spp_fdmi_plugin_fid);
	if (rc != 0) {
		rc = M0_ERR_INFO(rc, "Invalid FDMI plugin fid format: fid=%s",
				 params->spp_fdmi_plugin_fid_s);
	}

	return rc;
}

/**
 * Callback function called by the FDMI service to deliver the FDMI event
 * to the plugin program.
 */
static int process_fdmi_record(struct m0_uint128 *rec_id,
			       struct m0_buf fdmi_rec,
			       struct m0_fid filter_id)
{
	struct m0_fdmi_record_reg *rreg;
	struct m0_fol_rec fol_rec;
	int               rc;

	m0_fol_rec_init(&fol_rec, NULL);
	rc = m0_fol_rec_decode(&fol_rec, &fdmi_rec);
	if (rc != 0)
		goto out;

	if (fsp_params.spp_extra_ref) {
		/*
		 * Adding an extra ref on this record.
		 * It will be released when plugin gets ack from consumer.
		 */
		rreg = m0_fdmi__pdock_record_reg_find(rec_id);
		if (rreg != NULL)
			m0_ref_get(&rreg->frr_ref);
	}

	dump_fol_rec_to_json(rec_id, &fol_rec);
out:
	m0_fol_rec_fini(&fol_rec);
	return rc;
}

static int fdmi_service_start(struct m0_client *m0c)
{
	struct m0_reqh *reqh = &m0c->m0c_reqh;
	struct m0_reqh_service_type *stype;
	bool start_service = false;
	int rc = 0;

	stype = m0_reqh_service_type_find("M0_CST_FDMI");
	if (stype == NULL) {
		M0_LOG(M0_ERROR, "FDMI service type is not found.");
		return M0_ERR_INFO(-EINVAL, "Unknown reqh service type: M0_CST_FDMI");
	}

	fsp_fdmi_service = m0_reqh_service_find(stype, reqh);
	if (fsp_fdmi_service == NULL) {
		rc = m0_reqh_service_allocate(&fsp_fdmi_service, &m0_fdmi_service_type, NULL);
		if (rc != 0)
			return M0_RC_INFO(rc, "Failed to allocate FDMI service.");
		m0_reqh_service_init(fsp_fdmi_service, reqh, NULL);
		start_service = true;
	}

	if (start_service)
		rc = m0_reqh_service_start(fsp_fdmi_service);
	return M0_RC(rc);
}

static void fdmi_service_stop(struct m0_client *m0c)
{
	struct m0_reqh *reqh = &m0c->m0c_reqh;

	if (fsp_fdmi_service != NULL) {
		m0_reqh_service_prepare_to_stop(fsp_fdmi_service);
		m0_reqh_idle_wait_for(reqh, fsp_fdmi_service);
		m0_reqh_service_stop(fsp_fdmi_service);
		m0_reqh_service_fini(fsp_fdmi_service);
		fsp_fdmi_service = NULL;
	}
}

static int init_fdmi_plugin(struct m0_fsp_params *params)
{
	const static struct m0_fdmi_plugin_ops pcb = {
		.po_fdmi_rec = process_fdmi_record
	};
	const struct m0_fdmi_filter_desc fd;
	int rc;

	fsp_pdo = m0_fdmi_plugin_dock_api_get();

	rc = fsp_pdo->fpo_register_filter(&params->spp_fdmi_plugin_fid, &fd, &pcb);
	fprintf(stderr, "Plugin registration: rc=%d\n", rc);
	if (rc != 0)
		return rc;

	fsp_pdo->fpo_enable_filters(true, &params->spp_fdmi_plugin_fid, 1);
	return rc;
}

static void fini_fdmi_plugin(struct m0_fsp_params *params)
{
	fsp_pdo->fpo_enable_filters(false, &params->spp_fdmi_plugin_fid, 1);
	fsp_pdo->fpo_deregister_plugin(&params->spp_fdmi_plugin_fid, 1);
}

/**
 * Reading from stdin to get FDMI record ID and then
 * release this record.
 */
static void fdmi_plugin_record_ack()
{
	struct m0_uint128 rec_id = { 0 };
	int len;

	setvbuf(stdin,  NULL, _IONBF, 0);
	while (1) {
		len = scanf(" %"SCNx64" : %"SCNx64, &rec_id.u_hi, &rec_id.u_lo);
		if (len == 2) {
			if (fsp_params.spp_extra_ref)
				fsp_pdo->fpo_release_fdmi_rec(&rec_id, NULL);
		} else {
			if (terminated != 0)
				break;
		}
	}
}

static int fsp_init(struct m0_fsp_params *params)
{
	int rc;

	fsp_conf.mc_local_addr            = params->spp_local_addr;
	fsp_conf.mc_ha_addr               = params->spp_hare_addr;
	fsp_conf.mc_profile               = params->spp_profile_fid;
	fsp_conf.mc_process_fid           = params->spp_process_fid;
	fsp_conf.mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	fsp_conf.mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	fsp_conf.mc_layout_id             = 1;
	fsp_conf.mc_is_oostore            = 1;
	fsp_conf.mc_is_read_verify        = 0;
	fsp_conf.mc_idx_service_id        = M0_IDX_DIX;

	fsp_dix_conf.kc_create_meta 	 = false;
	fsp_conf.mc_idx_service_conf 	 = &fsp_dix_conf;

	/* Client instance init */
	rc = m0_client_init(&fsp_client, &fsp_conf, true);
	if (rc != 0)
		return rc;

	M0_POST(fsp_client != NULL);

	rc = fdmi_service_start(fsp_client);
	if (rc != 0) {
		m0_client_fini(fsp_client, true);
		return rc;
	}

	rc = init_fdmi_plugin(params);
	if (rc != 0) {
		fdmi_service_stop(fsp_client);
		m0_client_fini(fsp_client, true);
	}
	return rc;
}

static void fsp_fini(struct m0_fsp_params *params)
{
	fini_fdmi_plugin(params);
	fdmi_service_stop(fsp_client);

	/* Client stops its services including FDMI */
	m0_client_fini(fsp_client, true);
}

/*
 * Signals handling
 */
static void fsp_sighandler(int signum)
{
	fprintf(stderr, "fdmi_sample_plugin interrupted by signal %d\n", signum);
	m0_semaphore_up(&fsp_sem);
	terminated = 1;

	/* Restore default handlers. */
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
}

static int fsp_sighandler_init(void)
{
	struct sigaction sa = { .sa_handler = fsp_sighandler };
	int              rc;

	sigemptyset(&sa.sa_mask);

	/* Block these signals while the handler runs. */
	sigaddset(&sa.sa_mask, SIGINT);
	sigaddset(&sa.sa_mask, SIGTERM);

	rc = sigaction(SIGINT, &sa, NULL) ?: sigaction(SIGTERM, &sa, NULL);
	return rc == 0 ? 0 : M0_ERR(errno);
}

static void fsp_print_params(struct m0_fsp_params *params)
{
	fprintf(stderr,
		"Starting params:\n"
		"  local_ep: %s\n"
		"  hare_ep : %s\n"
		"  profile_fid : %s\n"
		"  process_fid : %s\n"
		"  plugin_fid  : %s\n"
		"  output as string: %s\n",
		params->spp_local_addr, params->spp_hare_addr,
		params->spp_profile_fid, params->spp_process_fid,
		params->spp_fdmi_plugin_fid_s,
		params->spp_output_strings? "true":"false");
}

int main(int argc, char **argv)
{
	int rc = 0;

	if (argc == 1) {
		fsp_usage();
		exit(EXIT_FAILURE);
	}

	rc = fsp_args_parse(&fsp_params, argc, argv);
	if (rc != 0) {
		fprintf(stderr, "Args parse failed\n");
		return M0_ERR(errno);
	}

	rc = m0_semaphore_init(&fsp_sem, 0);
	if (rc != 0)
		return M0_ERR(errno);

	rc = fsp_init(&fsp_params);
	if (rc != 0) {
		rc = M0_ERR(errno);
		goto fini_sem;
	}

	rc = fsp_sighandler_init();
	if (rc != 0)
		goto fini_fsp;

	/* Main thread loop */
	fsp_print_params(&fsp_params);
	fprintf(stderr, "fdmi_sample_plugin waiting for signal...\n");
	fdmi_plugin_record_ack();
	m0_semaphore_down(&fsp_sem);

fini_fsp:
	fsp_fini(&fsp_params);
fini_sem:
	m0_semaphore_fini(&fsp_sem);
	return M0_RC(rc < 0 ? -rc : rc);
}

#undef M0_TRACE_SUBSYSTEM
/** @} fdmi_sample_plugin */
