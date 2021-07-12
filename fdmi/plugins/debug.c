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

