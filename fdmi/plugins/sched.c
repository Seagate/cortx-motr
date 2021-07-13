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

#include <unistd.h>
#include <fcntl.h>
#include "fdmi/plugins/sched.h"

#define FDMI_PLUGIN_EP "/etc/fdmi_plugin_ep"

static struct m0_semaphore sched_sem;
static struct m0_config	m0_conf = {};
static struct m0_client	*m0c = NULL;
static struct m0_idx_dix_config	dix_conf = {};

const struct m0_fdmi_pd_ops *pdo;
static struct m0_reqh_service *sched_fdmi_service = NULL;

#define MAX_LEN 8192
static char buffer[MAX_LEN];


#if 0
M0_INTERNAL void m0_save_m0_xcode_type(int fd, char tab[], const struct m0_xcode_type *xf_type)
{
	if (xf_type == NULL)
		return;
	int buffer_len = 4096;
	char buffer[buffer_len];
	int i = 0;
	sprintf(buffer, "%sstruct m0_xcode_type: %p { \n", tab,xf_type);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t%sxct_aggr: %d\n", tab, xf_type->xct_aggr);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t%sxct_name: %s\n", tab, xf_type->xct_name);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t%sxct_ops: %p\n", tab, xf_type->xct_ops);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t%sxct_atype: %d\n", tab, xf_type->xct_atype);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t%sxct_flags: %d\n", tab, xf_type->xct_flags);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t%sxct_decor: %p\n", tab, xf_type->xct_decor);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t%sxct_sizeof: %d\n", tab, (int)xf_type->xct_sizeof);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t%sxct_nr: %d\n", tab, (int)xf_type->xct_nr);
	write(fd, buffer, strlen(buffer));

	for  ( i = 0;i < (int)xf_type->xct_nr; i++) {
		sprintf(buffer, "\t%sstruct m0_xcode_field: { \n", tab);
		write(fd, buffer, strlen(buffer));
		sprintf(buffer, "\t\t%sxf_name: %s\n", tab, xf_type->xct_child[i].xf_name);
		write(fd, buffer, strlen(buffer));

		strcat(tab,"\t");
		m0_save_m0_xcode_type(fd, tab, xf_type->xct_child[i].xf_type);

		sprintf(buffer, "\t\t%sxf_tag: %lu\n", tab, xf_type->xct_child[i].xf_tag);
		write(fd, buffer, strlen(buffer));
		sprintf(buffer, "\t\t%sxf_offset: %d\n", tab, xf_type->xct_child[i].xf_offset);
		write(fd, buffer, strlen(buffer));

		sprintf(buffer, "\t%s}\n", tab); //struct m0_xcode_field
		write(fd, buffer, strlen(buffer));
	}

	sprintf(buffer, "%s}\n", tab); //struct m0_xcode_type
	write(fd, buffer, strlen(buffer));

}

M0_INTERNAL void m0_save_m0_fol_rec(struct m0_fol_rec *rec, const char *prefix)
{
	char filename[32];
	int buffer_len = 4096;
	char buffer[buffer_len];
	int fd = 0;
	static int fc = 0;
	sprintf(filename, "/tmp/fol_rec_%s_%p_%d", prefix, rec, fc);
	M0_ENTRY("m0_save_m0_fol_rec fol rec=%p\n ", rec);
	++fc;

	//open the file
	fd = open(filename, O_WRONLY | O_CREAT);

	//using m0_fol_rec_to_str
	//int len = m0_fol_rec_to_str(rec, buffer, buffer_len);
	//write(fd, buffer, len);

	//fill the buffer and write buffer
	sprintf(buffer, "\nstruct m0_fol_rec {\n");
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\tm0_fol: %p\n", rec->fr_fol);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\tfr_tid: %lu\n", rec->fr_tid);
	write(fd, buffer, strlen(buffer));

	//struct m0_fol_rec_header
	sprintf(buffer, "\tstruct m0_fol_rec_header {\n");
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t\trh_frags_nr: %u\n", rec->fr_header.rh_frags_nr);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t\trh_data_len: %u\n", rec->fr_header.rh_data_len);
	write(fd, buffer, strlen(buffer));

	//struct m0_update_id
	sprintf(buffer, "\t\tstruct m0_update_id {\n");
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t\t\tui_node: %u\n", rec->fr_header.rh_self.ui_node);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t\t\tui_update: %lu\n", rec->fr_header.rh_self.ui_update);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t\t}\n");
	write(fd, buffer, strlen(buffer));

	sprintf(buffer, "\t\trh_magic: %lu\n", rec->fr_header.rh_magic);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t}\n");
	write(fd, buffer, strlen(buffer));

	sprintf(buffer, "\tfr_epoch: %p\n", rec->fr_epoch);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\tfr_sibling: %p\n", rec->fr_sibling);
	write(fd, buffer, strlen(buffer));

	//m0_fol_frag:rp_link to this list
	struct m0_fol_frag     *frag;
	sprintf(buffer, "\tstruct m0_tl {\n");
	m0_tl_for(m0_rec_frag, &rec->fr_frags, frag) {
		sprintf(buffer, "\t\tstruct m0_fol_frag {\n");
		write(fd, buffer, strlen(buffer));
		sprintf(buffer, "\t\t\t struct m0_fol_frag_ops = %p {\n", frag->rp_ops);
		write(fd, buffer, strlen(buffer));
		sprintf(buffer, "\t\t\t\t struct m0_fol_frag_type : %p{\n", frag->rp_ops->rpo_type);
		write(fd, buffer, strlen(buffer));
		sprintf(buffer, "\t\t\t\t\trpt_index: %d\n", frag->rp_ops->rpo_type->rpt_index);
		write(fd, buffer, strlen(buffer));
		sprintf(buffer, "\t\t\t\t\trpt_name: %s\n", frag->rp_ops->rpo_type->rpt_name);
		write(fd, buffer, strlen(buffer));

		//const struct m0_xcode_type        *rpt_xt;
		char tab[100] = "\t\t\t\t\t";
		m0_save_m0_xcode_type(fd, tab, frag->rp_ops->rpo_type->rpt_xt);

		sprintf(buffer, "\t\t\t\t}\n");//struct m0_fol_frag_ops
		write(fd, buffer, strlen(buffer));
		sprintf(buffer, "\t\t\t}\n");
		write(fd, buffer, strlen(buffer));

		struct m0_fop_fol_frag *fp_frag = frag->rp_data;
		sprintf(buffer, "\t\t\tstruct m0_fop_fol_frag: %p{\n", fp_frag);
		write(fd, buffer, strlen(buffer));
		sprintf(buffer, "\t\t\t\tffrp_fop_code: %d\n", fp_frag->ffrp_fop_code);
		write(fd, buffer, strlen(buffer));
		sprintf(buffer, "\t\t\t\tffrp_rep_code: %d\n", fp_frag->ffrp_rep_code);
		write(fd, buffer, strlen(buffer));
		struct m0_xcode_obj obj = {
			//.xo_type = m0_fop_fol_frag_xc,
			.xo_type = m0_cas_op_xc,
			.xo_ptr = fp_frag->ffrp_fop
			//.xo_ptr = frag->rp_data
		};
		//m0_xcode_print(&obj, buffer, buffer_len);

		struct m0_cas_op *cas_op = fp_frag->ffrp_fop;
		M0_LOG(M0_DEBUG, "m0_save rec: %p cas_op=%p ", rec, cas_op);
		sprintf(buffer, "\t\t\t\tstruct m0_cas_op: %p {\n", cas_op);
		write(fd, buffer, strlen(buffer));
		sprintf(buffer, "\t\t\t\t\t\tfid:"FID_F"\n", FID_P(&cas_op->cg_id.ci_fid));
		write(fd, buffer, strlen(buffer));

		sprintf(buffer, "\t\t\t\t\tstruct m0_cas_recv: {\n");
		write(fd, buffer, strlen(buffer));
		sprintf(buffer, "\t\t\t\t\t\tcr_nr: %lu\n", cas_op->cg_rec.cr_nr);
		write(fd, buffer, strlen(buffer));
		int i=0;
		for (i = 0; i < cas_op->cg_rec.cr_nr; i++) {
			sprintf(buffer, "\n\t\t\t\t\t\tcr_key: %lu bytes ", cas_op->cg_rec.cr_rec[i].cr_key.u.ab_buf.b_nob);
			write(fd, buffer, strlen(buffer));
			write(fd, cas_op->cg_rec.cr_rec[i].cr_key.u.ab_buf.b_addr,
			      cas_op->cg_rec.cr_rec[i].cr_key.u.ab_buf.b_nob);
			sprintf(buffer, "\n\t\t\t\t\t\tcr_val: %lu bytes ", cas_op->cg_rec.cr_rec[i].cr_val.u.ab_buf.b_nob);
			write(fd, buffer, strlen(buffer));
			write(fd, cas_op->cg_rec.cr_rec[i].cr_val.u.ab_buf.b_addr,
			      cas_op->cg_rec.cr_rec[i].cr_val.u.ab_buf.b_nob);
			M0_LOG(M0_DEBUG, "op = %p key: %lu value=%lu ", cas_op, cas_op->cg_rec.cr_rec[i].cr_key.u.ab_buf.b_nob,
			       cas_op->cg_rec.cr_rec[i].cr_val.u.ab_buf.b_nob);
		}
		sprintf(buffer, "\n\t\t\t\t\t}\n"); //struct m0_cas_recv
		write(fd, buffer, strlen(buffer));

		sprintf(buffer, "\t\t\t\t\t\tcg_flags: %d\n", cas_op->cg_flags);
		write(fd, buffer, strlen(buffer));
		sprintf(buffer, "\t\t\t\t}\n"); //struct m0_cas_op
		write(fd, buffer, strlen(buffer));

		sprintf(buffer, "\t\t\t}\n"); //struct m0_fop_fol_frag
		write(fd, buffer, strlen(buffer));

		sprintf(buffer, "\t\t\trp_magic: %lu\n", frag->rp_magic);
		write(fd, buffer, strlen(buffer));
		sprintf(buffer, "\t\t\trp_flag: %d\n", frag->rp_flag);
		write(fd, buffer, strlen(buffer));

		sprintf(buffer, "\t\t}\n");
		write(fd, buffer, strlen(buffer));

	} m0_tl_endfor;
	sprintf(buffer, "\t}\n");

        //struct m0_fdmi_src_rec
	sprintf(buffer, "\tstruct m0_fdmi_src_rec {\n");
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t\tfsr_magic: %lu\n",rec->fr_fdmi_rec.fsr_magic);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t\tstruct *m0_fdmi_src fsr_src =%p{\n",rec->fr_fdmi_rec.fsr_src);
	write(fd, buffer, strlen(buffer));

	sprintf(buffer, "\t\t}\n");
	write(fd, buffer, strlen(buffer));

	sprintf(buffer, "\t\tfsr_rec_id:"U128X_F"\n",U128_P(&rec->fr_fdmi_rec.fsr_rec_id));
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t\tfsr_matched: %d\n",rec->fr_fdmi_rec.fsr_matched);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t\tfsr_dryrun: %d\n",rec->fr_fdmi_rec.fsr_dryrun);
	write(fd, buffer, strlen(buffer));
	sprintf(buffer, "\t}\n");
	write(fd, buffer, strlen(buffer));

	sprintf(buffer, "}\n");
	write(fd, buffer, strlen(buffer));

	close(fd);
	M0_LEAVE("fol rec ptr=%p\n", rec);
}
#endif

M0_INTERNAL char *to_hex(void *addr, int len)
{
	int i, j;
	if ((2 * len) > MAX_LEN) {
		fprintf(stderr, "to_hex() failed with (2 * len) > MAX_LEN\n");
		return NULL;
	}
	for(i = 0, j = 0; i < (2 * len) && j < len; ++j)
		i += sprintf(buffer + i, "%02x", ((char *)addr)[j]);
	buffer[2 * len - 2] = '\0';
	return buffer;
}

M0_INTERNAL void m0_dump_m0_fol_rec_to_json(struct m0_fol_rec *rec)
{
	struct m0_fol_frag *frag;
	int i;

	m0_tl_for(m0_rec_frag, &rec->fr_frags, frag) {
		struct m0_fop_fol_frag *fp_frag = frag->rp_data;
		struct m0_cas_op *cas_op = fp_frag->ffrp_fop;
		struct m0_cas_recv cg_rec = cas_op->cg_rec;
		struct m0_cas_rec *cr_rec = cg_rec.cr_rec;

		for (i = 0; i < cg_rec.cr_nr; i++) {
			int len = 0;

			m0_console_printf("{ ");

			len = sizeof(struct m0_fid);
			m0_console_printf("\"fid\": \"%s\", ",
					  to_hex((void *)&cas_op->cg_id.ci_fid, len));

			len = cr_rec[i].cr_key.u.ab_buf.b_nob;
			m0_console_printf("\"cr_key\": \"%s\", ",
					  to_hex(cr_rec[i].cr_key.u.ab_buf.b_addr, len));

			len = cr_rec[i].cr_val.u.ab_buf.b_nob;
			if (len > 0) {
				m0_console_printf("\"cr_val\": \"%s\"",
						  to_hex(cr_rec[i].cr_val.u.ab_buf.b_addr, len));
			} else {
				m0_console_printf("\"cr_val\": \"0\"");
			}
			m0_console_printf(" }\n");

		}
	} m0_tl_endfor;
}

static void usage(void)
{
	fprintf(stderr,
		"Usage: m0sched "
		"-l local_addr -h ha_addr -p profile_fid -f process_fid "
		"-g fdmi_plugin_fid\n"
		"Use -? or -i for more verbose help on common arguments.\n"
		"Usage example for common arguments: \n"
		"m0sched -l 192.168.52.53@tcp:12345:4:1 "
		"-h 192.168.52.53@tcp:12345:1:1 "
		"-p 0x7000000000000001:0x37 -f 0x7200000000000001:0x19"
		"-g 0x6c00000000000001:0x51"
		"\n");
}

/**
 * @retval 0      Success.
 * @retval -Exxx  Error.
 */
static int sched_args_parse(struct sched_conf *params, int argc, char ** argv)
{
	int rc = 0;

	params->local_addr 	= NULL;
	params->ha_addr    	= NULL;
	params->profile_fid     = NULL;
	params->process_fid   	= NULL;
	params->fdmi_plugin_fid_s = NULL;

	rc = M0_GETOPTS("m0sched", argc, argv,
			M0_HELPARG('?'),
			M0_VOIDARG('i', "more verbose help",
				   LAMBDA(void, (void) {
						   usage();
						   exit(0);
					   })),
			M0_STRINGARG('l', "Local endpoint address",
				     LAMBDA(void, (const char *string) {
						     params->local_addr = (char*)string;
					     })),
			M0_STRINGARG('h', "HA address",
				     LAMBDA(void, (const char *str) {
						     params->ha_addr = (char*)str;
					     })),
			M0_STRINGARG('f', "Process FID",
				     LAMBDA(void, (const char *str) {
						     params->process_fid = (char*)str;
					     })),
			M0_STRINGARG('p', "Profile options for client",
				     LAMBDA(void, (const char *str) {
						     params->profile_fid = (char*)str;
					     })),
			M0_STRINGARG('g', "FDMI plugin fid",
				     LAMBDA(void, (const char *str) {
						     params->fdmi_plugin_fid_s =
							     (char*)str;
					     })));
	if (rc != 0)
		return M0_ERR(rc);

        /* All mandatory params must be defined. */
	if (rc == 0 &&
	    (params->local_addr == NULL || params->ha_addr == NULL ||
	     params->profile_fid == NULL || params->process_fid == NULL ||
	     params->fdmi_plugin_fid_s == NULL)) {
		usage();
		return M0_ERR(-EINVAL);
	}

	rc = m0_fid_sscanf(params->fdmi_plugin_fid_s, &params->fdmi_plugin_fid);
	if (rc != 0) {
		rc = M0_ERR_INFO(rc, "Invalid fdmi plugin fid format: fdmi_plugin_fid=%s",
				 params->fdmi_plugin_fid_s);
	}

	return rc;
}

static int classify_handle_fdmi_rec_not(struct m0_uint128 *rec_id,
					struct m0_buf fdmi_rec,
					struct m0_fid filter_id)
{
	struct m0_fol_rec fol_rec;

	m0_fol_rec_init(&fol_rec, NULL);
	m0_fol_rec_decode(&fol_rec, &fdmi_rec);
	m0_dump_m0_fol_rec_to_json(&fol_rec);
	m0_fol_rec_fini(&fol_rec);
	return 0;
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

	sched_fdmi_service = m0_reqh_service_find(stype, reqh);
	if (sched_fdmi_service == NULL) {
		rc = m0_reqh_service_allocate(&sched_fdmi_service, &m0_fdmi_service_type, NULL);
		if (rc != 0)
			return M0_RC_INFO(rc, "Failed to allocate FDMI service.");
		m0_reqh_service_init(sched_fdmi_service, reqh, NULL);
		start_service = true;
	}

	if (start_service)
        	rc = m0_reqh_service_start(sched_fdmi_service);
	return M0_RC(rc);
}

static void fdmi_service_stop(struct m0_client *m0c)
{
	if (sched_fdmi_service != NULL) {
		m0_reqh_service_stop(sched_fdmi_service);
		sched_fdmi_service = NULL;
	}
}

static int init_fdmi_plugin(struct sched_conf *conf)
{
	int rc;

	pdo = m0_fdmi_plugin_dock_api_get();
	const struct m0_fdmi_filter_desc fd;

	const static struct m0_fdmi_plugin_ops pcb = {
		.po_fdmi_rec = classify_handle_fdmi_rec_not
	};

	rc = pdo->fpo_register_filter(&conf->fdmi_plugin_fid, &fd, &pcb);
	fprintf(stderr, "Plugin registration failed: rc=%d\n", rc);
	if (rc != 0)
		return rc;

	pdo->fpo_enable_filters(true, &conf->fdmi_plugin_fid, 1);
	return rc;
}

static void fini_fdmi_plugin(struct sched_conf *conf)
{
	pdo->fpo_enable_filters(false, &conf->fdmi_plugin_fid, 1);
	pdo->fpo_deregister_plugin(&conf->fdmi_plugin_fid, 1);
}

#if 0
static int write_plugin_endpoint(char *fdmi_plugin_ep) {
	int fd;
	int rc;

	fd = open(FDMI_PLUGIN_EP, O_CREAT | O_WRONLY, 0666);
	if (fd < 0) {
		fprintf(stderr, "Failed to open %s: error=%d\n", FDMI_PLUGIN_EP, errno);
		return errno;
	}

	rc = write(fd, fdmi_plugin_ep, strlen(fdmi_plugin_ep));
	if (rc != 0) {
		fprintf(stderr, "Failed to write to %s\n", FDMI_PLUGIN_EP);
		close(fd);
		return rc;
	}
	fprintf(stderr, "%s written to %s\n", fdmi_plugin_ep, FDMI_PLUGIN_EP);
	close(fd);
	return rc;
}
#endif

static int sched_init(struct sched_conf *conf)
{
	int rc;

	m0_conf.mc_local_addr            = conf->local_addr;
	m0_conf.mc_ha_addr               = conf->ha_addr;
	m0_conf.mc_profile               = conf->profile_fid;
	m0_conf.mc_process_fid           = conf->process_fid;
	m0_conf.mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	m0_conf.mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	m0_conf.mc_layout_id             = 1;
	m0_conf.mc_is_oostore            = 1;
	m0_conf.mc_is_read_verify        = 0;
	m0_conf.mc_idx_service_id        = M0_IDX_DIX;

	dix_conf.kc_create_meta 	 = false;
	m0_conf.mc_idx_service_conf 	 = &dix_conf;

	/* Client instance */
	rc = m0_client_init(&m0c, &m0_conf, true);
	if (rc != 0)
		return rc;

	M0_POST(m0c != NULL);

	rc = fdmi_service_start(m0c);
	if (rc != 0) {
		m0_client_fini(m0c, true);
		return rc;
	}

	rc = init_fdmi_plugin(conf);
	if (rc != 0) {
		fdmi_service_stop(m0c);
		m0_client_fini(m0c, true);
	}
	return rc;
}

static void sched_fini(struct sched_conf *conf)
{
	fini_fdmi_plugin(conf);

	/* Client stops its services including FDMI */
	m0_client_fini(m0c, true);
}

/*
 * Signals handling
 */
static void sched_sighandler(int signum)
{
	fprintf(stderr, "m0sched interrupted by signal %d\n", signum);
	m0_semaphore_up(&sched_sem);

        /* Restore default handlers. */
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
}

static int sched_sighandler_init(void)
{
	struct sigaction sa = { .sa_handler = sched_sighandler };
	int              rc;

	sigemptyset(&sa.sa_mask);

	/* Block these signals while the handler runs. */
	sigaddset(&sa.sa_mask, SIGINT);
	sigaddset(&sa.sa_mask, SIGTERM);

	rc = sigaction(SIGINT, &sa, NULL) ?: sigaction(SIGTERM, &sa, NULL);
	return rc == 0 ? 0 : M0_ERR(errno);
}

static void print_params(struct sched_conf *params)
{
	fprintf(stderr,
		"Starting params:\n"
		"  local_ep: %s\n"
		"  hare_ep : %s\n"
		"  profile_fid : %s\n"
		"  process_fid : %s\n"
		"  plugin_fid  : %s\n",
		params->local_addr, params->ha_addr,
		params->profile_fid, params->process_fid,
		params->fdmi_plugin_fid_s);
}

int main(int argc, char **argv)
{
	int rc = 0;

	if (argc == 1) {
		usage();
		exit(EXIT_FAILURE);
	}

	rc = sched_args_parse(&c_params, argc, argv);
	if (rc != 0) {
		fprintf(stderr, "Args parse failed\n");
		return M0_ERR(errno);
	}

	rc = m0_semaphore_init(&sched_sem, 0);
	if (rc != 0)
		return M0_ERR(errno);

	rc = sched_init(&c_params);
	if (rc != 0) {
		rc = M0_ERR(errno);
		goto sem_fini;
	}

	rc = sched_sighandler_init();
	if (rc != 0)
		goto sched_fini;

        /* Main thread loop */
	print_params(&c_params);
	fprintf(stderr, "m0sched waiting for signal...\n");
	m0_semaphore_down(&sched_sem);

sched_fini:
	sched_fini(&c_params);
sem_fini:
	m0_semaphore_fini(&sched_sem);
	return M0_RC(rc < 0 ? -rc : rc);
}

#undef M0_TRACE_SUBSYSTEM
/** @} m0sched */
