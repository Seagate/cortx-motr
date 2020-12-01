/* -*- C -*- */
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


/**
 * @addtogroup be
 *
 * @{
 */

#define _FILE_OFFSET_BITS 64  /* for fseeko */
#include <err.h>
#include <stdio.h>
#include <string.h>           /* strerror */
#include <sysexits.h>         /* EX_CONFIG, EX_OK, EX_USAGE */
#include <fcntl.h>            /* open */
#include <unistd.h>           /* close */
#include <time.h>             /* localtime_r */
#include <pthread.h>
#include <signal.h>           /* signal() to register ctrl + C handler */
#include <yaml.h>

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "lib/getopts.h"
#include "lib/memory.h"
#include "lib/assert.h"
#include "lib/string.h"       /* m0_strdup */
#include "lib/thread.h"
#include "motr/version.h"
#include "lib/uuid.h"
#include "motr/magic.h"       /* M0_FORMAT_HEADER_MAGIC */
#include "motr/init.h"        /* m0_init */
#include "reqh/reqh.h"
#include "module/instance.h"
#include "format/format.h"
#include "format/format_xc.h"
#include "balloc/balloc.h"    /* M0_BALLOC_GROUP_DESC_FORMAT_VERSION */
#include "be/btree_internal.h"
#include "be/list.h"
#include "be/seg_internal.h"
#include "be/op.h"            /* m0_be_op_active */
#include "be/ut/helper.h"     /* m0_be_ut_backend_cfg_default */
#include "cas/ctg_store.h"
#include "cob/cob.h"          /* m0_cob_nsrec */
#include "pool/pool.h"
#include "stob/ad_private.h"
#include "dix/fid_convert.h"
#include "stob/ad.h"               /* m0_stob_ad_domain */
#include "ioservice/io_service.h"  /* m0_ios_cdom_get */
#include "ioservice/fid_convert.h" /* m0_fid_convert_cob2adstob */
#include "ioservice/cob_foms.h"    /* m0_cc_stob_cr_credit */
#include "be/extmap_internal.h"    /* m0_be_emap */
#include "be/tx_bulk.h"   /* m0_be_tx_bulk */

M0_TL_DESCR_DECLARE(ad_domains, M0_EXTERN);
M0_TL_DECLARE(ad_domains, M0_EXTERN, struct ad_domain_map);

struct queue {
	pthread_mutex_t q_lock;
	pthread_cond_t  q_cond;
	struct action  *q_head;
	struct action  *q_tail;
	uint64_t        q_nr;
	uint64_t        q_max;
};

struct scanner {
	FILE		    *s_file;
	/**
	 * It holds the BE segment offset of the bnode which is then used by
	 * scanner thread to read bnode.
	 */
	off_t		     s_start_off;
	/**
	 * Queue to store bnode offset in the BE segment.
	 */
	struct queue	     s_bnode_q;
	/** Scanner thread which processes the bnodes. */
	struct m0_thread     s_thread;
	struct m0_mutex      s_lock;
	off_t		     s_off;
	off_t		     s_pos;
	bool		     s_byte;
	/**
	 * This determines if invalid OIDs are logged instead silent discard
	 */
	bool                 s_print_invalid_oids;
	off_t		     s_size;
	struct m0_be_seg    *s_seg;
	struct queue	    *s_q;
	/**
	 * We use the following buffer as a cache to increase read performace.
	 */
	unsigned char        s_chunk[4 * 1024 * 1024];
	off_t                s_chunk_pos;
	/**
	 * This holds the maximum tx region size.
	 * It is used to ensure that each record written by the builder does not
	 * exceed this value.
	 */
	m0_bcount_t	     s_max_reg_size;
	/* Holds source metadata segment header generation identifier. */
	uint64_t	     s_gen;
	/* Set variable when correct generation identifier has been found */
	bool	             s_gen_found;
};

struct stats {
	uint64_t s_found;
	uint64_t s_chksum;
	uint64_t s_align[2];
	uint64_t s_version;
};

struct recops;
struct rectype {
	const char            *r_name;
	struct m0_format_tag   r_tag;
	const struct recops   *r_ops;
	struct stats           r_stats;
};

struct recops {
	int (*ro_proc) (struct scanner *s, struct rectype *r, char *buf);
	int (*ro_ver)  (struct scanner *s, struct rectype *r, char *buf);
	int (*ro_check)(struct scanner *s, struct rectype *r, char *buf);
};

struct bstats {
	uint64_t c_tree;
	uint64_t c_node;
	uint64_t c_leaf;
	uint64_t c_maxlevel;
	uint64_t c_fanout;
	uint64_t c_kv;
	uint64_t c_kv_bad;
};

struct btype { /* b-tree type */
	enum m0_be_btree_type  b_type;
	const char            *b_name;
	int                  (*b_proc)(struct scanner *s, struct btype *b,
				       struct m0_be_bnode *node);
	struct bstats          b_stats;
};

struct gen {
	uint64_t g_gen;
	uint64_t g_count;
};

enum action_opcode {
	AO_INIT       = 1,
	AO_DONE       = 2,
	AO_CTG        = 3,
	AO_COB        = 4,
	AO_EMAP_FIRST = 5,
	AO_NR         = 30
};

struct action_ops;
struct builder;

struct action {
	enum action_opcode       a_opc;
	struct builder          *a_builder;
	const struct action_ops *a_ops;
	struct action           *a_next;
	struct action           *a_prev;
};

struct bnode_act {
	struct action bna_act;
	off_t         bna_offset;
};

struct cob_action {
	struct action coa_act;
	/** Buffer to hold cob namespace key. */
	struct m0_buf coa_key;
	/** Buffer to hold cob namespace value. */
	struct m0_buf coa_val;
	/** Stores cob namespace record. */
	uint8_t       coa_valdata[sizeof(struct m0_cob_nsrec)];
	/** FID to identify cob namespace btree. */
	struct m0_fid coa_fid;
};

struct action_ops {
	int  (*o_prep)(struct action *act, struct m0_be_tx_credit *cred);
	void (*o_act) (struct action *act, struct m0_be_tx *tx);
	void (*o_fini)(struct action *act);
};

enum { CACHE_SIZE = 1000000 };

struct cache_slot {
        struct m0_fid       cs_fid;
        uint64_t            cs_flags;
        struct m0_be_btree *cs_tree;
};

/**
 * It is used to store btree pointers.
 * It prevents multiple allocation of credits for the same btree creation.
 */
struct cache {
	struct cache_slot c_slot[CACHE_SIZE];
	int               c_head;
};

struct ad_dom_info{
	/* pointer to m0_stob used by ad_domain */
	struct m0_stob           *stob;
	/* pointer to m0_stob_domain used by ad_domain */
	struct m0_stob_domain    *stob_dom;
};

struct builder {
	struct m0_be_ut_backend    b_backend;
	struct m0_reqh             b_reqh;
	struct m0_be_domain       *b_dom;
	struct m0_be_seg          *b_seg0;
	struct m0_be_seg          *b_seg;
	uint64_t                   b_ad_dom_count; /**< Number of ad domains */
	struct m0_stob_ad_domain **b_ad_domain;/**< AD domains pointer array */
	struct m0_stob_domain     *b_ad_dom; /**< stob domain pointer */
	struct ad_dom_info	 **b_ad_info; /**< ad_domain info array */
	struct m0_thread           b_thread;
	struct queue              *b_q;
	struct m0_be_tx_credit     b_cred;
	struct cache	           b_cache;
	uint64_t                   b_size;
	const char                *b_dom_path;
	const char                *b_stob_path; /**< stob path for ad_domain */
	const char                *b_be_config_file; /** BE configuration */

	uint64_t                   b_act;
	uint64_t                   b_data; /**< data throughput */
	/** ioservice cob domain. */
	struct m0_cob_domain      *b_ios_cdom;
	/** mdservice cob domain. */
	struct m0_cob_domain      *b_mds_cdom;
	/**
	 * It is the fid of config root pool version which is used to
	 * construct dix layout.
	 */
	struct m0_fid              b_pver_fid;
	struct m0_mutex            b_emaplock[AO_NR - AO_EMAP_FIRST];
	struct m0_mutex            b_coblock;
	struct m0_mutex            b_ctglock;
};

struct emap_action {
	struct action 		emap_act;	/**< action struct */
	struct m0_fid 		emap_fid; 	/**< FID of btree */
	struct m0_buf 		emap_key; 	/**< Buffer to hold emap key */
	struct m0_buf 		emap_val; 	/**< Buffer to hold emap val */
	struct m0_be_emap_key   emap_key_data;  /**< emap key data  */
	struct m0_be_emap_rec   emap_val_data;  /**< emap val data */
};

static int  init(void);
static void fini(void);
static int  scan (struct scanner *s);
static void stats_print(void);
static int  parse(struct scanner *s);
static int  get  (struct scanner *s, void *buf, size_t nob);
static int  getat(struct scanner *s, off_t off, void *buf, size_t nob);
static int  deref(struct scanner *s, const void *addr, void *buf, size_t nob);
static int  recdo(struct scanner *s, const struct m0_format_tag *tag,
		  struct rectype *rt);
static const char *recname(const struct rectype *rt);
static const char *bname  (const struct btype *bt);

static int btree (struct scanner *s, struct rectype *r, char *buf);
static int bnode (struct scanner *s, struct rectype *r, char *buf);
static int seghdr(struct scanner *s, struct rectype *r, char *buf);
static void *scanner_action(size_t len, enum action_opcode opc,
			    const struct action_ops *ops);

static void genadd(uint64_t gen);
static void generation_id_print(uint64_t gen);
static void generation_id_get(FILE *fp, uint64_t *gen_id);
static int  generation_id_verify(struct scanner *s, uint64_t gen);
static void seg_get(FILE *fp, struct m0_be_seg *out);

static int  scanner_cache_init   (struct scanner *s);

static int  scanner_init   (struct scanner *s);
static void scanner_fini   (struct scanner *s);
static int  builder_init   (struct builder *b);
static void builder_fini   (struct builder *b);
static void ad_dom_fini    (struct builder *b);
static void builder_thread (struct builder *b);
static void be_cfg_default_init(struct m0_be_domain_cfg  *dom_cfg,
				struct m0_be_tx_bulk_cfg *tb_cfg);

static int format_header_verify(const struct m0_format_header *h,
				uint16_t rtype);
static bool btree_node_pre_is_valid  (const struct m0_be_bnode *node,
				      struct scanner *s);
static bool btree_node_post_is_valid (const struct m0_be_bnode *node,
				      const struct m0_be_btree_kv_ops *ops);
static bool btree_kv_is_valid        (struct m0_be_bnode *node,
				      int index, struct m0_buf *key);
static void btree_bad_kv_count_update(uint64_t type, int count);
static void *scanner_action(size_t len, enum action_opcode opc,
			    const struct action_ops *ops);
static void *builder_action(struct builder *b, size_t len,
			    enum action_opcode opc,
			    const struct action_ops *ops);
static bool fid_without_type_eq(const struct m0_fid *fid0,
				const struct m0_fid *fid1);

static struct cache_slot *cache_lookup(struct cache *c, struct m0_fid *fid);
static struct cache_slot *cache_insert(struct cache *c,
				       const struct m0_fid *fid);
static void qinit(struct queue *q, uint64_t maxnr);
static void qfini(struct queue *q);
static void qput(struct queue *q, struct action *act);
static struct action *qget (struct queue *q);
static struct action *qtry (struct queue *q);
static struct action *qpeek(struct queue *q);

static int  ctg_proc(struct scanner *s, struct btype *b,
		    struct m0_be_bnode *node);
static int ctg_pver_fid_get(struct m0_fid *fid);

static void test(void);

static int cob_proc(struct scanner *s, struct btype *b,
		    struct m0_be_bnode *node);

static int   emap_proc(struct scanner *s, struct btype *b,
		       struct m0_be_bnode *node);
static int   emap_prep(struct action *act, struct m0_be_tx_credit *cred);
static void  emap_act(struct action *act, struct m0_be_tx *tx);
static void  emap_fini(struct action *act);
static int   emap_kv_get(struct scanner *s, const struct be_btree_key_val *kv,
		         struct m0_buf *key_buf, struct m0_buf *val_buf);
static void  sig_handler(int num);
static int   be_cfg_from_yaml_update(const char              *yaml_file,
				     struct m0_be_domain_cfg *cfg);

static void scanner_thread(struct scanner *s);
static const struct recops btreeops;
static const struct recops bnodeops;
static const struct recops seghdrops;

static const struct action_ops done_ops;
static const struct action_ops ctg_ops;
static const struct action_ops cob_ops;

#define _FT(name) M0_FORMAT_TYPE_ ## name
#define _TAG(name) M0_ ## name ## _FORMAT_VERSION , _FT(name)
#define _T(name, str, field, ops) [_FT(name)] = {			\
	.r_tag  = { _TAG(name), { offsetof(struct str, field) } },	\
	.r_ops = (ops)							\
}

static struct rectype rt[] = {
	_T(BALLOC_GROUP_DESC, m0_balloc_group_desc, bgd_footer,       NULL),
	_T(BALLOC,            m0_balloc,            cb_footer,        NULL),
	_T(BE_BTREE,          m0_be_btree,          bb_footer,     &btreeops),
	_T(BE_BNODE,          m0_be_bnode,          bt_footer,     &bnodeops),
	_T(BE_EMAP_KEY,       m0_be_emap_key,       ek_footer,        NULL),
	_T(BE_EMAP_REC,       m0_be_emap_rec,       er_footer,        NULL),
	_T(BE_EMAP,           m0_be_emap,           em_footer,        NULL),
	_T(BE_LIST,           m0_be_list,           bl_format_footer, NULL),
	_T(BE_SEG_HDR,        m0_be_seg_hdr,        bh_footer,     &seghdrops),
	_T(CAS_CTG,           m0_cas_ctg,           cc_foot,          NULL),
	_T(CAS_STATE,         m0_cas_state,         cs_footer,        NULL),
	_T(COB_DOMAIN,        m0_cob_domain,        cd_footer,        NULL),
	_T(COB_NSREC,         m0_cob_nsrec,         cnr_footer,       NULL),
	_T(EXT,               m0_ext,               e_footer,         NULL),
	_T(POOLNODE,          m0_poolnode,          pn_footer,        NULL),
	_T(POOLDEV,           m0_pooldev,           pd_footer,        NULL),
	_T(POOL_SPARE_USAGE,  m0_pool_spare_usage,  psu_footer,       NULL),
	_T(STOB_AD_DOMAIN,    m0_stob_ad_domain,    sad_footer,       NULL),
	_T(STOB_AD_0TYPE_REC, stob_ad_0type_rec,    sa0_footer,       NULL),
	[M0_FORMAT_TYPE_NR] = {}
};

#undef _T
#undef _TAG
#undef _FV

M0_BASSERT(ARRAY_SIZE(rt) == M0_FORMAT_TYPE_NR + 1);

#define _B(t, proc) [t] = { .b_type = (t), .b_proc = (proc) }

static struct btype bt[] = {
	_B(M0_BBT_BALLOC_GROUP_EXTENTS, NULL),
	_B(M0_BBT_BALLOC_GROUP_DESC,    NULL),
	_B(M0_BBT_EMAP_EM_MAPPING,      NULL),
	_B(M0_BBT_COB_NAMESPACE,        NULL),
	_B(M0_BBT_EMAP_EM_MAPPING,      NULL),
	_B(M0_BBT_CAS_CTG,              NULL),
	_B(M0_BBT_COB_NAMESPACE,        NULL),
	_B(M0_BBT_EMAP_EM_MAPPING,      &emap_proc),
	_B(M0_BBT_CAS_CTG,              &ctg_proc),
	_B(M0_BBT_COB_NAMESPACE,        &cob_proc),
	_B(M0_BBT_COB_OBJECT_INDEX,     NULL),
	_B(M0_BBT_COB_FILEATTR_BASIC,   NULL),
	_B(M0_BBT_COB_FILEATTR_EA,      NULL),
	_B(M0_BBT_COB_FILEATTR_OMG,     NULL),
	_B(M0_BBT_CONFDB,               NULL),
	_B(M0_BBT_UT_KV_OPS,            NULL),
	[M0_BBT_NR] = {}
};
#undef _B

enum {
	MAX_GEN    	         = 256,
	MAX_SCAN_QUEUED	         = 10000000,
	MAX_QUEUED  	         = 1000000,
	MAX_REC_SIZE             = 64*1024,
	/**
	 * The value in MAX_GEN_DIFF_SEC is arrived on the basis of max time
	 * difference between mkfs run on local and remote node and the
	 * assumption that clock time delta between nodes is negligible.
	 */
	MAX_GEN_DIFF_SEC         = 30,
	MAX_KEY_LEN              = 256,
	MAX_VALUE_LEN            = 256,
	DEFAULT_BE_SEG_LOAD_ADDR = 0x400000100000,
	/**
	 * The value of 44MB for DEFAULT_BE_MAX_TX_REG_SZ was picked from the
	 * routine m0_be_ut_backend_cfg_default()
	 */
	DEFAULT_BE_MAX_TX_REG_SZ = (44 * 1024 * 1024ULL)
};

/** It is used to recover meta data of component catalogue store. */
struct ctg_action {
	struct action      cta_act;
	struct m0_buf      cta_key;
	struct m0_buf      cta_val;
	/** It is the fid of the btree in which key and value are inserted. */
	struct m0_fid      cta_fid;
	/** It is used to store dix layout. */
	struct m0_cas_id   cta_cid;
	/** True represents key and value are either meta or ctidx record */
	bool               cta_ismeta;
	/**
	 * It is used to store the btree pointers of "ctg stores".
	 * It is to avoid multiple cache lookup.
	 */
	struct cache_slot *cta_slot;
};

static struct scanner beck_scanner;
static struct builder beck_builder;
static struct gen g[MAX_GEN] = {};
static struct m0_be_seg s_seg = {}; /** Used only in dry-run mode. */

static bool  dry_run = false;
static bool  disable_directio = false;
static bool  signaled = false;

/**
 * These values provided the maximum builder performance after experiments on
 * hardware.
 */
static struct m0_be_tx_bulk_cfg default_tb_cfg = (struct m0_be_tx_bulk_cfg){
		.tbc_q_cfg = {
			.bqc_q_size_max       = 1000,
			.bqc_producers_nr_max = 1,
		},
			.tbc_workers_nr       = 0x40,
			.tbc_partitions_nr    = AO_NR,
			.tbc_work_items_per_tx_max = 1,
	};
#define FLOG(level, rc, s)						\
	M0_LOG(level, " rc=%d  at offset: %"PRId64" errno: %s (%i), eof: %i", \
	       (rc), (uint64_t)ftell(s->s_file), strerror(errno),	\
	       errno, feof(s->s_file))

#define RLOG(level, prefix, s, r, tag)					\
	M0_LOG(level, prefix " %"PRIu64" %s %hu:%hu:%u", s->s_off, recname(r), \
	       (tag)->ot_version, (tag)->ot_type, (tag)->ot_size)

static void sig_handler(int num)
{
	printf("Caught Signal %d \n", num);
	signaled = true;
}

int main(int argc, char **argv)
{
	struct m0              instance     = {};
	const char            *spath        = NULL;
	int                    sfd          = 0; /* Use stdin by default. */
	bool                   ut           = false;
	bool                   version      = false;
	bool                   print_gen_id = false;
	struct queue           q            = {};
	int                    result;
	uint64_t	       gen_id	    = 0;
	struct m0_be_tx_credit max;
	FILE                  *fp;

	m0_node_uuid_string_set(NULL);
	result = m0_init(&instance);
	if (result != 0)
		errx(EX_CONFIG, "Cannot initialise motr: %d.", result);
	result = init();
	if (result != 0)
		errx(EX_CONFIG, "Cannot initialise beck: %d.", result);
	result = scanner_init(&beck_scanner);
	if (result != 0) {
		errx(EX_CONFIG, "Cannot initialise scanner: %d.", result);
	}

	result = M0_GETOPTS("m0_beck", argc, argv,
		   M0_STRINGARG('s', "Path to snapshot (file or device).",
			LAMBDA(void, (const char *path) {
				spath = path;
			})),
		   M0_FORMATARG('S', "Snapshot size", "%"SCNi64,
				&beck_scanner.s_size),
		   M0_FLAGARG('b', "Scan every byte (10x slower).",
			      &beck_scanner.s_byte),
		   M0_FLAGARG('U', "Run unit tests.", &ut),
		   M0_FLAGARG('n', "Dry Run.", &dry_run),
		   M0_FLAGARG('I', "Disable directio.", &disable_directio),
		   M0_FLAGARG('p', "Print Generation Identifier.",
			      &print_gen_id),
		   M0_FORMATARG('g', "Generation Identifier.", "%"PRIu64,
				&beck_scanner.s_gen),
		   M0_FLAGARG('V', "Version info.", &version),
		   M0_FLAGARG('e', "Print errored OIDs.",
			      &beck_scanner.s_print_invalid_oids),
		   M0_STRINGARG('y', "YAML file path",
			   LAMBDA(void, (const char *s) {
				   beck_builder.b_be_config_file = s;
				   })),
		   M0_STRINGARG('a', "stob domain path",
			   LAMBDA(void, (const char *s) {
				   beck_builder.b_stob_path = s;
				   })),
		   M0_STRINGARG('d', "segment stob domain path path",
			LAMBDA(void, (const char *s) {
				beck_builder.b_dom_path = s;
			})));
	if (result != 0)
		errx(EX_USAGE, "Wrong option: %d.", result);
	if (ut) {
		test();
		return EX_OK;
	}
	if (version) {
		m0_build_info_print();
		return EX_OK;
	}
	if (dry_run)
		printf("Running in read-only mode.\n");

	if (!beck_scanner.s_print_invalid_oids)
		printf("Will not print INVALID GOB IDs if found since '-e'"
		       "option was not specified. \n");

	if (beck_builder.b_dom_path == NULL && !dry_run && !print_gen_id)
		errx(EX_USAGE, "Specify domain path (-d).");
	if (spath != NULL) {
		sfd = open(spath, O_RDONLY);
		if (sfd == -1)
			err(EX_NOINPUT, "Cannot open snapshot \"%s\".", spath);
	}
	beck_scanner.s_file = fdopen(sfd, "r");
	if (beck_scanner.s_file == NULL)
		err(EX_NOINPUT, "Cannot open snapshot.");
	if (beck_scanner.s_size == 0) {
		result = fseek(beck_scanner.s_file, 0, SEEK_END);
		if (result != 0)
			err(EX_NOINPUT, "Cannot seek snapshot to the end.");
		beck_builder.b_size = beck_scanner.s_size =
						ftello(beck_scanner.s_file);
		if (beck_scanner.s_size == -1)
			err(EX_NOINPUT, "Cannot tell snapshot size.");
		result = fseek(beck_scanner.s_file, 0, SEEK_SET);
		if (result != 0)
		      err(EX_NOINPUT, "Cannot seek snapshot to the beginning.");
		printf("Snapshot size: %"PRId64".\n", beck_scanner.s_size);
	}

	if (beck_builder.b_be_config_file && !dry_run && !print_gen_id) {
		fp = fopen(beck_builder.b_be_config_file, "r");
		if (fp == NULL)
			err(EX_NOINPUT, "Failed to open yaml file \"%s\".",
			    beck_builder.b_be_config_file);
		fclose(fp);
	}
	generation_id_get(beck_scanner.s_file, &gen_id);
	if (print_gen_id) {
		generation_id_print(gen_id);
		printf("\n");
		fclose(beck_scanner.s_file);
		return EX_OK;
	}
	if (gen_id != 0)
		beck_scanner.s_gen_found = true;
	if (beck_scanner.s_gen != 0) {
		printf("\nReceived source segment header generation id\n");
		generation_id_print(beck_scanner.s_gen);
		printf("\n");
		fflush(stdout);
	}

	beck_scanner.s_gen = gen_id ?: beck_scanner.s_gen;
	printf("\nSource segment header generation id to be used by beck.\n");
	generation_id_print(beck_scanner.s_gen);
	printf("\n");
	fflush(stdout);
	/*
	 *  If both segment's generation identifier is corrupted then abort
	 *  beck tool.
	 */
	if (!dry_run && beck_scanner.s_gen == 0) {
		printf("Cannot find any segment header generation identifer");
		return EX_DATAERR;
	}
	qinit(&beck_scanner.s_bnode_q, MAX_SCAN_QUEUED);
	result = M0_THREAD_INIT(&beck_scanner.s_thread, struct scanner *,
				NULL, &scanner_thread, &beck_scanner,
				"scannner");
	if (result != 0)
		err(EX_CONFIG, "Cannot start scanner thread.");
	/*
	 * Skip builder related calls for dry run as we will not be building a
	 * new segment.
	 */
	if (!dry_run) {
		qinit(&q, MAX_QUEUED);
		beck_scanner.s_q = beck_builder.b_q = &q;
		result = builder_init(&beck_builder);
		beck_scanner.s_seg = beck_builder.b_seg;
		m0_be_engine_tx_size_max(&beck_builder.b_dom->bd_engine,
					 &max, NULL);
		beck_scanner.s_max_reg_size = max.tc_reg_size;
		if (result != 0)
			err(EX_CONFIG, "Cannot initialise builder.");
	} else {
		/**
		 *  Since we do not have builder variables holding segment data,
		 *  we use the global variable s_seg for this purpose.
		 */
		seg_get(beck_scanner.s_file, &s_seg);
		beck_scanner.s_seg = &s_seg;
		beck_scanner.s_max_reg_size = DEFAULT_BE_MAX_TX_REG_SZ;
	}

	result = scanner_cache_init(&beck_scanner);
	if (result != 0)
		err(EX_CONFIG, "Cannot initialise scanner.");
	if (dry_run) {
		printf("Press CTRL+C to quit.\n");
		signal(SIGINT, sig_handler);
	}
	result = scan(&beck_scanner);
	printf("\n Pending to process bnodes=%"PRIu64 " It may take some time",
	       beck_scanner.s_bnode_q.q_nr);
	qput(&beck_scanner.s_bnode_q, scanner_action(sizeof(struct action),
					  AO_DONE, NULL));
	m0_thread_join(&beck_scanner.s_thread);
	m0_thread_fini(&beck_scanner.s_thread);
	qfini(&beck_scanner.s_bnode_q);
	if (result != 0)
		warn("Scan failed: %d.", result);

	stats_print();
	if (!dry_run) {
		qput(&q, builder_action(&beck_builder, sizeof(struct action),
					AO_DONE, &done_ops));
		builder_fini(&beck_builder);
		qfini(&q);
	}
	scanner_fini(&beck_scanner);
	fini();
	if (spath != NULL)
		close(sfd);
	m0_fini();
	return EX_OK;
}

static void scanner_thread(struct scanner *s)
{
	struct m0_be_bnode  node;
	struct bnode_act   *ba;
	struct btype       *b;
	int                 rc;

	do {
		ba  = (struct bnode_act *)qget(&s->s_bnode_q);
		if (ba->bna_act.a_opc != AO_DONE) {
			rc = getat(s, ba->bna_offset, &node, sizeof node);
			M0_ASSERT(rc == 0);
			b = &bt[node.bt_backlink.bli_type];
			b->b_proc(s, b, &node);
			m0_free(ba);
		}
	} while (ba->bna_act.a_opc != AO_DONE);
}

static char iobuf[4*1024*1024];

enum { DELTA = 60 };

static void generation_id_print(uint64_t gen)
{
	time_t    ts = m0_time_seconds(gen);
	struct tm tm;

	localtime_r(&ts, &tm);
	printf("%04d-%02d-%02d-%02d:%02d:%02d.%09"PRIu64"  (%"PRIu64")",
	       tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
	       tm.tm_hour, tm.tm_min, tm.tm_sec,
	       m0_time_nanoseconds(gen), gen);
}

static bool seg_hdr_get(FILE *fp, struct m0_be_seg_hdr *out)
{
	struct m0_format_tag  tag          = {};
	bool                  result       = false;
	struct m0_be_seg_hdr  seg_hdr;
	const char           *rt_be_cksum;
	off_t                 old_offset;

	old_offset = ftello(fp);
	fseeko(fp, 0, SEEK_SET);

	if (fread(&seg_hdr, 1, sizeof(seg_hdr), fp) == sizeof(seg_hdr)) {

		m0_format_header_unpack(&tag, &seg_hdr.bh_header);

		if (seg_hdr.bh_header.hd_magic == M0_FORMAT_HEADER_MAGIC &&
		    tag.ot_type == M0_FORMAT_TYPE_BE_SEG_HDR &&
		    memcmp(&tag, &rt[M0_FORMAT_TYPE_BE_SEG_HDR].r_tag,
			   sizeof tag) == 0 &&
		    m0_format_footer_verify(&seg_hdr, 0) == 0) {

			rt_be_cksum = m0_build_info_get()->
				      bi_xcode_protocol_be_checksum;
			if (strncmp(seg_hdr.bh_be_version, rt_be_cksum,
				    M0_BE_SEG_HDR_VERSION_LEN_MAX) == 0 &&
			    seg_hdr.bh_items_nr == 1) {
				*out = seg_hdr;
				result = true;
			}
		}
	}

	fseeko(fp, old_offset, SEEK_SET);
	return (result);
}

static void generation_id_get(FILE *fp, uint64_t *gen_id)
{
	struct m0_be_seg_hdr seg_hdr;

	if (seg_hdr_get(fp, &seg_hdr))
		*gen_id = seg_hdr.bh_items[0].sg_gen;
	else
		printf("Checksum error for segment header. "
		       "Could not extract Generation ID.\n");
}

static void seg_get(FILE *fp, struct m0_be_seg *out)
{
	struct m0_be_seg_hdr         seg_hdr;
	struct m0_be_seg             seg;
	const struct m0_be_seg_geom *g;

	/** Update the values from the segment in file passed as parameter */
	if (seg_hdr_get(fp, &seg_hdr)) {
		g = &seg_hdr.bh_items[0];

		seg.bs_reserved = sizeof(seg_hdr);
		seg.bs_size     = g->sg_size;
		seg.bs_addr     = g->sg_addr;
		seg.bs_offset   = g->sg_offset;
		seg.bs_gen      = g->sg_gen;
	} else {
		M0_ASSERT(beck_scanner.s_size != 0);
		/**
		 *  If file has corrupted segment then use these
		 *  hardcoded values.
		 */
		printf("Checksum error for segment header. "
		       "Could not extract Segment map information. "
		       "Using possible defaults\n");
		seg.bs_reserved = sizeof(seg_hdr);
		seg.bs_size     = beck_scanner.s_size;
		seg.bs_addr     = (void *)DEFAULT_BE_SEG_LOAD_ADDR;
		seg.bs_offset   = 0;
	}

	*out = seg;

}

static int generation_id_verify(struct scanner *s, uint64_t gen)
{
	if (gen == s->s_gen)
		return 0;
	else if (s->s_gen_found)
		return -EINVAL;
	else if (gen > s->s_gen &&
		 m0_time_seconds(m0_time_sub(gen, s->s_gen)) > MAX_GEN_DIFF_SEC)
		return -ETIME;
	else if (gen < s->s_gen &&
		 m0_time_seconds(m0_time_sub(s->s_gen, gen)) > MAX_GEN_DIFF_SEC)
		return -ETIME;
	return 0;
}

static int scanner_init(struct scanner *s)
{
	int rc = 0;

	m0_mutex_init(&s->s_lock);

	return rc;
}

static int scanner_cache_init(struct scanner *s)
{
	int rc;

	m0_mutex_init(&s->s_lock);
	rc = fseeko(s->s_file, 0, SEEK_SET);
	if (rc != 0) {
		M0_LOG(M0_FATAL, "Can not seek at the beginning of file");
		return rc;
	}
	rc = getat(s, 0, s->s_chunk, sizeof s->s_chunk);
	if (rc != 0)
		M0_LOG(M0_FATAL, "Can not read first chunk");
	return rc;
}

static void scanner_fini(struct scanner *s)
{
	m0_mutex_fini(&s->s_lock);
}

static int scan(struct scanner *s)
{
	uint64_t magic;
	int      result;
	time_t   lasttime = time(NULL);
	off_t    lastoff  = s->s_off;
	uint64_t lastrecord = 0;
	uint64_t lastdata = 0;

	setvbuf(s->s_file, iobuf, _IONBF, sizeof iobuf);
	while (!signaled && (result = get(s, &magic, sizeof magic)) == 0) {
		if (magic == M0_FORMAT_HEADER_MAGIC) {
			s->s_off -= sizeof magic;
			parse(s);
		} else if (s->s_byte)
			s->s_off -= sizeof magic - 1;
		if (!s->s_byte)
			s->s_off &= ~0x7;
		if (time(NULL) - lasttime > DELTA) {
			printf("\nOffset: %15lli     Speed: %7.2f MB/s     "
			       "Completion: %3i%%     "
			       "Action: %"PRIu64" records/s     "
			       "Data Speed: %7.2f MB/s",
			       (long long)s->s_off,
			       ((double)s->s_off - lastoff) /
			       DELTA / 1024.0 / 1024.0,
			       (int)(s->s_off * 100 / s->s_size),
			       (beck_builder.b_act - lastrecord) / DELTA,
			       ((double)beck_builder.b_data - lastdata) /
			       DELTA / 1024.0 / 1024.0);
			lasttime = time(NULL);
			lastoff  = s->s_off;
			lastrecord = beck_builder.b_act;
			lastdata = beck_builder.b_data;
		}
	}
	return feof(s->s_file) ? 0 : result;
}

static void stats_print(void)
{
	int i;

	printf("\n%25s : %9s %9s %9s %9s\n",
	       "record", "found", "bad csum", "unaligned", "version");
	for (i = 0; i < ARRAY_SIZE(rt); ++i) {
		struct stats *s = &rt[i].r_stats;
		if (recname(&rt[i]) == NULL)
			continue;
		printf("%25s : %9"PRId64" %9"PRId64" %9"PRId64" %9"PRId64"\n",
		       recname(&rt[i]), s->s_found, s->s_chksum, s->s_align[1],
		       s->s_version);
	}
	printf("\n%25s : %9s %9s %9s %9s %9s %9s %9s\n",
	       "btree", "tree", "node", "leaf", "maxlevel", "kv", "bad kv",
	       "fanout");
	for (i = 0; i < ARRAY_SIZE(bt); ++i) {
		struct bstats *s = &bt[i].b_stats;
		if (bname(&bt[i]) == NULL)
			continue;
		printf("%25s : %9"PRId64" %9"PRId64" %9"PRId64" "
		       "%9"PRId64" %9"PRId64" %9"PRId64" %9"PRId64"\n",
		       bname(&bt[i]),
		       s->c_tree, s->c_node, s->c_leaf, s->c_maxlevel,
		       s->c_kv, s->c_kv_bad, s->c_fanout);
	}

	printf("\ngenerations\n");
	for (i = 0; i < ARRAY_SIZE(g); ++i) {
		if (g[i].g_count > 0) {
			generation_id_print(g[i].g_gen);
			printf(" : %9"PRId64"\n", g[i].g_count);
		}
	}
}

static int parse(struct scanner *s)
{
	struct m0_format_header hdr = {};
	struct m0_format_tag    tag = {};
	struct rectype         *r;
	int                     result;
	int                     idx;
	off_t			lastoff;
	result = get(s, &hdr, sizeof hdr);
	if (result == 0) {
		M0_ASSERT(hdr.hd_magic == M0_FORMAT_HEADER_MAGIC);
		m0_format_header_unpack(&tag, &hdr);
		idx = tag.ot_type;
		if (IS_IN_ARRAY(idx, rt) && idx >= M0_FORMAT_TYPE_BEGIN &&
		    tag.ot_type == rt[idx].r_tag.ot_type) {
			r = &rt[idx];
		} else {
			r = &rt[M0_FORMAT_TYPE_NR];
			RLOG(M0_INFO, "U", s, r, &tag);
		}
		r->r_stats.s_found++;
		r->r_stats.s_align[!!(s->s_off & 07)]++;
		/* Only process btree, bnode and segment header records. */
		if (M0_IN(idx, (M0_FORMAT_TYPE_BE_BTREE,
				M0_FORMAT_TYPE_BE_BNODE,
				M0_FORMAT_TYPE_BE_SEG_HDR))) {
			lastoff = s->s_off;
			s->s_off -= sizeof hdr;
			result = recdo(s, &tag, r);
			if (result != 0)
				s->s_off = lastoff;
		}
	} else {
		M0_LOG(M0_FATAL, "Cannot read hdr->hd_bits.");
		FLOG(M0_FATAL, result, s);
		result = M0_ERR(-EIO);
	}
	return M0_RC(result);
}

static int init(void)
{
	int i;
	int j;
	const struct m0_xcode_enum *e = &m0_xc_m0_format_type_enum;
	const char fprefix[] = "M0_FORMAT_TYPE_";
	const char bprefix[] = "M0_BBT_";

	M0_ASSERT(m0_forall(i, ARRAY_SIZE(rt), ergo(rt[i].r_tag.ot_type != 0,
		  m0_forall(j, ARRAY_SIZE(rt),
		   (rt[i].r_tag.ot_type == rt[j].r_tag.ot_type) == (i == j)))));
	M0_ASSERT(m0_forall(i, ARRAY_SIZE(rt),
			    ergo(rt[i].r_tag.ot_type != 0,
				 rt[i].r_tag.ot_type == i)));
	for (i = 0; i < ARRAY_SIZE(rt) - 1; ++i) {
		if (rt[i].r_tag.ot_type == 0)
			continue;
		M0_ASSERT(rt[i].r_tag.ot_size <= MAX_REC_SIZE);
		M0_ASSERT(m0_xcode_enum_is_valid(e, rt[i].r_tag.ot_type));
		for (j = 0; j < e->xe_nr; ++j) {
			M0_ASSERT(strncmp(fprefix, e->xe_val[j].xev_name,
					  sizeof fprefix - 1) == 0);
			if (rt[i].r_tag.ot_type == e->xe_val[j].xev_val) {
				rt[i].r_name = e->xe_val[j].xev_name +
					sizeof fprefix - 1;
				break;
			}
		}
		M0_ASSERT(j < e->xe_nr);
	}
	e = &m0_xc_m0_be_btree_type_enum;
	for (i = 0; i < ARRAY_SIZE(bt) - 1; ++i) {
		if (bt[i].b_type == 0)
			continue;
		M0_ASSERT(m0_xcode_enum_is_valid(e, bt[i].b_type));
		for (j = 0; j < e->xe_nr; ++j) {
			M0_ASSERT(strncmp(bprefix, e->xe_val[j].xev_name,
					  sizeof bprefix - 1) == 0);
			if (bt[i].b_type == e->xe_val[j].xev_val) {
				bt[i].b_name = e->xe_val[j].xev_name +
					sizeof bprefix - 1;
				break;
			}
		}
		M0_ASSERT(j < e->xe_nr);
	}
	return 0;
}

static void fini(void)
{
}

static int recdo(struct scanner *s, const struct m0_format_tag *tag,
		 struct rectype *r)
{
	unsigned  size = tag->ot_size + sizeof(struct m0_format_footer);
	void 	 *buf;
	int       result;

	if (size > MAX_REC_SIZE)
		return M0_RC(-EINVAL);

	buf = alloca(size);
	result = get(s, buf, size);
	if (result == 0) {
		if (memcmp(tag, &r->r_tag, sizeof *tag) == 0) {
			/**
			 * Check generation identifier before format footer
			 * verification. Only process the records whose
			 * generation identifier matches or is within
			 * +/- MAX_GEN_DIFF_SEC seconds of segment's generation
			 * identifier.
			 */
			if (r->r_ops != NULL && r->r_ops->ro_check != NULL) {
				result = r->r_ops->ro_check(s, r, buf);
			}
			if (result != 0)
				return M0_RC(result);
			result = m0_format_footer_verify(buf, false);
			if (result != 0) {
				RLOG(M0_DEBUG, "С", s, r, tag);
				FLOG(M0_DEBUG, result, s);
				r->r_stats.s_chksum++;
			} else {
				RLOG(M0_DEBUG, "R", s, r, tag);
				if (r->r_ops != NULL &&
				    r->r_ops->ro_proc != NULL)
					result = r->r_ops->ro_proc(s, r, buf);
			}
		} else {
			RLOG(M0_DEBUG, "V", s, r, tag);
			FLOG(M0_DEBUG, result, s);
			r->r_stats.s_version++;
			if (r->r_ops != NULL && r->r_ops->ro_ver != NULL)
				result = r->r_ops->ro_ver(s, r, buf);
		}
	}
	return M0_RC(result);
}

static const char *recname(const struct rectype *r)
{
	return r->r_name ?: r == &rt[ARRAY_SIZE(rt) - 1] ? "UNKNOWN" : NULL;
}

static const char *bname(const struct btype *b)
{
	return b->b_name ?: b == &bt[ARRAY_SIZE(bt) - 1] ? "UNKNOWN" : NULL;
}

static int getat(struct scanner *s, off_t off, void *buf, size_t nob)
{
	int   result = 0;
	FILE *f      = s->s_file;

	m0_mutex_lock(&s->s_lock);
	if (off != s->s_pos)
		result = fseeko(f, off, SEEK_SET);
	if (result != 0) {
		M0_LOG(M0_FATAL, "Cannot seek to %"PRId64".", off);
		result = M0_ERR(-errno);
	} else if (fread(buf, 1, nob, f) != nob) {
		if (feof(f))
			result = -ENOENT;
		else {
			M0_LOG(M0_FATAL, "Cannot read %d at %"PRId64".",
			       (int)nob, off);
			result = M0_ERR(-EIO);
		}
	} else {
		M0_ASSERT(ftello(f) == (off + nob));
		s->s_pos = off + nob;
		result = 0;
	}
	if (result != 0 && !feof(f)) {
		FLOG(M0_FATAL, result, s);
		s->s_pos = -1;
	}
	m0_mutex_unlock(&s->s_lock);
	return M0_RC(result);
}

static int deref(struct scanner *s, const void *addr, void *buf, size_t nob)
{
	off_t off = addr - s->s_seg->bs_addr;

	if (m0_be_seg_contains(s->s_seg, addr) &&
	    m0_be_seg_contains(s->s_seg, addr + nob - 1))
		return getat(s, off, buf, nob);
	else
		return M0_ERR(-EFAULT);
}

static int get(struct scanner *s, void *buf, size_t nob)
{
	int result = 0;
	s->s_start_off = s->s_off;
	if (!(s->s_off >= s->s_chunk_pos &&
	      s->s_off + nob < s->s_chunk_pos + sizeof s->s_chunk)) {
		result = getat(s, s->s_off, s->s_chunk, sizeof s->s_chunk);
		if (result == 0)
			s->s_chunk_pos = s->s_off;
	}
	if (result == 0) {
		memcpy(buf, &s->s_chunk[s->s_off - s->s_chunk_pos], nob);
		s->s_off += nob;
	}
	return result;
}

static int btree(struct scanner *s, struct rectype *r, char *buf)
{
	struct m0_be_btree *tree = (void *)buf;
	int                 idx  = tree->bb_backlink.bli_type;
	struct btype       *b;

	if (!IS_IN_ARRAY(idx, bt) || bt[idx].b_type == 0)
		idx = ARRAY_SIZE(bt) - 1;

	genadd(tree->bb_backlink.bli_gen);
	if (!s->s_gen_found) {
		s->s_gen_found = true;
		s->s_gen = tree->bb_backlink.bli_gen;
		printf("\nBeck will use latest generation id found in btree\n");
		generation_id_print(s->s_gen);
	}
	b = &bt[idx];
	b->b_stats.c_tree++;
	return 0;
}

static int btree_check(struct scanner *s, struct rectype *r, char *buf)
{
	struct m0_be_btree *tree = (void *)buf;
	int                 idx  = tree->bb_backlink.bli_type;

	if (!IS_IN_ARRAY(idx, bt) || bt[idx].b_type == 0)
		return -ENOENT;

	return generation_id_verify(s, tree->bb_backlink.bli_gen);
}

static void *scanner_action(size_t len, enum action_opcode opc,
			    const struct action_ops *ops)
{
	struct action *act;

	M0_PRE(len >= sizeof *act);

	act = m0_alloc(len);
	M0_ASSERT(act != NULL); /* Can we handle this? */
	act->a_opc = opc;
	act->a_ops = ops;
	return act;
}

static int bnode(struct scanner *s, struct rectype *r, char *buf)
{
	struct m0_be_bnode *node = (void *)buf;
	int                 idx  = node->bt_backlink.bli_type;
	struct btype       *b;
	struct bstats      *c;
	struct bnode_act   *ba;

	if (!IS_IN_ARRAY(idx, bt) || bt[idx].b_type == 0)
		idx = ARRAY_SIZE(bt) - 1;

	genadd(node->bt_backlink.bli_gen);
	if (!s->s_gen_found) {
		s->s_gen_found = true;
		s->s_gen = node->bt_backlink.bli_gen;
		printf("\nBeck will use latest generation id found in bnode\n");
		generation_id_print(s->s_gen);
	}
	b = &bt[idx];
	c = &b->b_stats;
	c->c_node++;
	if (b->b_proc != NULL) {
		ba = scanner_action(sizeof *ba, AO_INIT, NULL);
		ba->bna_offset = s->s_start_off;
		qput(&s->s_bnode_q, &ba->bna_act);
	}
	c->c_kv += node->bt_num_active_key;
	if (node->bt_isleaf) {
		c->c_leaf++;
	} else
		c->c_fanout += node->bt_num_active_key + 1;
	c->c_maxlevel = max64(c->c_maxlevel, node->bt_level);
	return 0;
}

static int bnode_check(struct scanner *s, struct rectype *r, char *buf)
{
	struct m0_be_bnode *node = (void *)buf;
	int                 idx  = node->bt_backlink.bli_type;

	if (!IS_IN_ARRAY(idx, bt) || bt[idx].b_type == 0)
		return -ENOENT;

	return generation_id_verify(s, node->bt_backlink.bli_gen);
}

static struct m0_stob_ad_domain *emap_dom_find(const struct action *act,
					       const struct m0_fid *emap_fid,
					       int  *lockid)
{
	struct m0_stob_ad_domain *adom = NULL;
	int			  i;

	for (i = 0; i < act->a_builder->b_ad_dom_count; i++) {
		adom = act->a_builder->b_ad_domain[i];
		if (m0_fid_eq(emap_fid,
	            &adom->sad_adata.em_mapping.bb_backlink.bli_fid)) {
			break;
		}
	}
	*lockid = i;
	return (i == act->a_builder->b_ad_dom_count) ? NULL: adom;
}

static const struct action_ops emap_ops = {
	.o_prep = &emap_prep,
	.o_act  = &emap_act,
	.o_fini = &emap_fini
};

static void emap_to_gob_convert(const struct m0_uint128 *emap_prefix,
				struct m0_fid           *out)
{
	struct m0_fid      dom_id = {};
	struct m0_stob_id  stob_id = {};
	struct m0_fid      cob_fid;
	struct m0_fid      gob_fid;

	m0_fid_tassume(&dom_id, &m0_stob_ad_type.st_fidt);

	/** Convert emap to stob_id */
	m0_stob_id_make(emap_prefix->u_hi, emap_prefix->u_lo, &dom_id,
			&stob_id);

	/** Convert stob_id to cob id */
	cob_fid = stob_id.si_fid;
	m0_fid_tassume(&cob_fid, &m0_cob_fid_type);

	/** Convert COB id to GOB id */
	m0_fid_convert_cob2gob(&cob_fid, &gob_fid);

	*out = gob_fid;
}

static int emap_proc(struct scanner *s, struct btype *btype,
		     struct m0_be_bnode *node)
{
	struct m0_stob_ad_domain *adom = NULL;
	struct emap_action       *ea;
	int                       id;
	int                       i;
	int                       ret = 0;
	struct m0_be_emap_key    *ek;
	struct m0_fid             gob_id;
	uint64_t                  inv_emap_off;

	for (i = 0; i < node->bt_num_active_key; i++) {
		ea = scanner_action(sizeof *ea, AO_EMAP_FIRST, &emap_ops);
		ea->emap_fid = node->bt_backlink.bli_fid;
		ea->emap_key = M0_BUF_INIT_PTR(&ea->emap_key_data);
		ea->emap_val = M0_BUF_INIT_PTR(&ea->emap_val_data);

		ret = emap_kv_get(s, &node->bt_kv_arr[i],
				  &ea->emap_key, &ea->emap_val);
		if (ret != 0) {
			if (s->s_print_invalid_oids) {
				ek = ea->emap_key.b_addr;
				inv_emap_off = node->bt_kv_arr[i].btree_key -
					       s->s_seg->bs_addr;
				emap_to_gob_convert(&ek->ek_prefix, &gob_id);
				M0_LOG(M0_ERROR, "Found incorrect EMAP entry at"
				       " segment offset %"PRIx64" for GOB "
				       FID_F, inv_emap_off, FID_P(&gob_id));
			}
			btree_bad_kv_count_update(node->bt_backlink.bli_type,
						  1);
			m0_free(ea);
			continue;
		}

		if (!dry_run) {
			ea->emap_act.a_builder = &beck_builder;
			adom = emap_dom_find(&ea->emap_act, &ea->emap_fid, &id);
			if (adom != NULL) {
				ea->emap_act.a_opc += id;
				qput(s->s_q, &ea->emap_act);
			} else {
				btree_bad_kv_count_update(
						node->bt_backlink.bli_type, 1);
				m0_free(ea);
				continue;
			}
		} else
			m0_free(ea);
	}
	return ret;
}

static int emap_entry_lookup(struct m0_stob_ad_domain  *adom,
			     struct m0_uint128          prefix,
			     m0_bindex_t               offset,
			     struct m0_be_emap_cursor  *it)
{
	int                       rc;

	M0_LOG(M0_DEBUG, U128X_F, U128_P(&prefix));
	rc = M0_BE_OP_SYNC_RET_WITH( &it->ec_op,
				  m0_be_emap_lookup(&adom->sad_adata,
						    &prefix, offset, it),
				  bo_u.u_emap.e_rc);
	return rc == -ESRCH ? -ENOENT : rc;
}

static int emap_prep(struct action *act, struct m0_be_tx_credit *credit)
{
	struct m0_stob_ad_domain *adom = NULL;
	struct emap_action   	 *emap_ac =  M0_AMB(emap_ac, act, emap_act);
	struct m0_be_emap_rec    *emap_val;
	struct m0_be_emap_key    *emap_key;
	int 			  rc;
	struct m0_be_emap_cursor  it = {};
	int                       id;

	adom = emap_dom_find(act, &emap_ac->emap_fid, &id);
	if (adom == NULL || id < 0 || id >= AO_NR - AO_EMAP_FIRST) {
		M0_LOG(M0_ERROR, "Invalid FID for emap record found !!!");
		m0_free(act);
		return M0_RC(-EINVAL);
	}

	m0_mutex_lock(&beck_builder.b_emaplock[id]);
	emap_val = emap_ac->emap_val.b_addr;
	if (emap_val->er_value != AET_HOLE) {
		adom->sad_ballroom->ab_ops->bo_alloc_credit(adom->sad_ballroom,
							    1, credit);
		emap_key = emap_ac->emap_key.b_addr;
		rc = emap_entry_lookup(adom, emap_key->ek_prefix, 0, &it);
		if (rc == 0)
			m0_be_emap_close(&it);
		else
			m0_be_emap_credit(&adom->sad_adata,
					  M0_BEO_INSERT, 1, credit);
		m0_be_emap_credit(&adom->sad_adata, M0_BEO_PASTE,
				  BALLOC_FRAGS_MAX + 1, credit);
	}
	m0_mutex_unlock(&beck_builder.b_emaplock[id]);
	return 0;
}

static void emap_act(struct action *act, struct m0_be_tx *tx)
{
	struct emap_action   	 *emap_ac =  M0_AMB(emap_ac, act, emap_act);
	struct m0_stob_ad_domain *adom = NULL;
	int     	    	  rc = 0;
	struct m0_ext 		  ext;
	struct m0_be_emap_key    *emap_key;
	struct m0_be_emap_rec    *emap_val;
	struct m0_be_emap_cursor  it = {};
	struct m0_ext             in_ext;
	int                       id;

	adom = emap_dom_find(act, &emap_ac->emap_fid, &id);
	emap_val = emap_ac->emap_val.b_addr;
	if (emap_val->er_value != AET_HOLE) {
		emap_key = emap_ac->emap_key.b_addr;
		ext.e_start = emap_val->er_value >> adom->sad_babshift;
		ext.e_end   = (emap_val->er_value + emap_key->ek_offset -
			       emap_val->er_start) >> adom->sad_babshift;
		m0_ext_init(&ext);

		m0_mutex_lock(&beck_builder.b_emaplock[id]);
		rc = adom->sad_ballroom->ab_ops->
			bo_reserve_extent(adom->sad_ballroom,
					  tx, &ext,
					  M0_BALLOC_NORMAL_ZONE);
		if (rc != 0) {
			m0_mutex_unlock(&beck_builder.b_emaplock[id]);
			M0_LOG(M0_ERROR, "Failed to reserve extent rc=%d", rc);
			return;
		}

		beck_builder.b_data +=
			((ext.e_end - ext.e_start) << adom->sad_babshift)
			<< adom->sad_bshift;

		rc = emap_entry_lookup(adom, emap_key->ek_prefix, 0, &it);
		/* No emap entry found for current stob, insert hole */
		rc = rc ? M0_BE_OP_SYNC_RET(op,
				m0_be_emap_obj_insert(&adom->sad_adata,
						      tx, &op,
						      &emap_key->ek_prefix,
						      AET_HOLE),
				bo_u.u_emap.e_rc) : 0;

		M0_SET0(&it);
		rc = emap_entry_lookup(adom, emap_key->ek_prefix,
				       emap_val->er_start, &it);
		if (it.ec_seg.ee_val == AET_HOLE) {
			in_ext.e_start = emap_val->er_start;
			in_ext.e_end   = emap_key->ek_offset;
			m0_ext_init(&in_ext);
			M0_SET0(&it.ec_op);
			m0_be_op_init(&it.ec_op);
			m0_be_emap_paste(&it, tx, &in_ext, emap_val->er_value,
			NULL, /*No need to delete */
			LAMBDA(void, (struct m0_be_emap_seg *seg,
				      struct m0_ext *ext,
				      uint64_t val) {
				/* cut left */
				M0_ASSERT(in_ext.e_start > seg->ee_ext.e_start);

				seg->ee_val = val;
				}),
			LAMBDA(void, (struct m0_be_emap_seg *seg,
				      struct m0_ext *ext,
				      uint64_t val) {
					/* cut right */
				M0_ASSERT(seg->ee_ext.e_end > in_ext.e_end);
				if (val < AET_MIN)
					seg->ee_val = val +
					(in_ext.e_end - seg->ee_ext.e_start);
				else
					seg->ee_val = val;
				}));
			M0_ASSERT(m0_be_op_is_done(&it.ec_op));
			rc = it.ec_op.bo_u.u_emap.e_rc;
			m0_be_op_fini(&it.ec_op);
			m0_be_emap_close(&it);
		}
		m0_mutex_unlock(&beck_builder.b_emaplock[id]);
	}

	if (rc != 0)
		M0_LOG(M0_ERROR, "Failed to insert emap record rc=%d", rc);
}

static void emap_fini(struct action *act)
{
}

static int emap_kv_get(struct scanner *s, const struct be_btree_key_val *kv,
		       struct m0_buf *key, struct m0_buf *val)
{
	return deref(s, kv->btree_key, key->b_addr,
		     sizeof(struct m0_be_emap_key))		   ?:
		format_header_verify(key->b_addr,
				     M0_FORMAT_TYPE_BE_EMAP_KEY)   ?:
		m0_format_footer_verify(key->b_addr, false)   	   ?:
		deref(s, kv->btree_val, val->b_addr,
		      sizeof(struct m0_be_emap_rec)) 		   ?:
		format_header_verify(val->b_addr,
				     M0_FORMAT_TYPE_BE_EMAP_REC)   ?:
		m0_format_footer_verify(val->b_addr, false);
}

static int seghdr(struct scanner *s, struct rectype *r, char *buf)
{
	return 0;
}

static int seghdr_ver(struct scanner *s, struct rectype *r, char *buf)
{
	struct m0_be_seg_hdr *h   = (void *)buf;
	struct m0_format_tag  tag = {};

	m0_format_header_unpack(&tag, &h->bh_header);
	M0_LOG(M0_INFO, "Segment header format: %hu:%hu:%u vs. %hu:%hu:%u.",
	       tag.ot_version, tag.ot_version, tag.ot_footer_offset,
	       r->r_tag.ot_version, r->r_tag.ot_version,
	       r->r_tag.ot_footer_offset);
	M0_LOG(M0_INFO, "Protocol: %s", (char *)h->bh_be_version);
	M0_LOG(M0_INFO, "     vs.: %s",
	       m0_build_info_get()->bi_xcode_protocol_be_checksum);
	genadd(h->bh_gen);
	return 0;
}

static int seghdr_check(struct scanner *s, struct rectype *r, char *buf)
{
	struct m0_be_seg_hdr *h   = (void *)buf;

	if (s->s_gen != h->bh_gen) {
		genadd(h->bh_gen);
		printf("\nFound another segment header generation\n");
		generation_id_print(h->bh_gen);
		return M0_ERR(-ETIME);
	}
	/*
	 * Flush immediately to avoid losing this information within other lines
	 * coming on the screen at the same time.
	 */
	fflush(stdout);

	return 0;
}

static void genadd(uint64_t gen)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g); ++i) {
		if (g[i].g_gen == gen || g[i].g_count == 0) {
			g[i].g_count++;
			g[i].g_gen = gen;
			break;
		}
	}
}

static void builder_do(struct m0_be_tx_bulk   *tb,
		       struct m0_be_tx        *tx,
		       struct m0_be_op        *op,
		       void                   *datum,
		       void                   *user,
		       uint64_t                worker_index,
		       uint64_t                partition)
{
	struct action  *act;
	struct builder *b = datum;

	m0_be_op_active(op);
	act = user;
	if (act != NULL) {
		b->b_act++;
		act->a_ops->o_act(act, tx);
		act->a_ops->o_fini(act);
		m0_free(act);
	}
	m0_be_op_done(op);
}

static void builder_done(struct m0_be_tx_bulk   *tb,
			 void                   *datum,
			 void                   *user,
			 uint64_t                worker_index,
			 uint64_t                partition)
{

}

static void builder_work_put(struct m0_be_tx_bulk *tb, struct builder *b)
{
	struct action          *act;
	struct m0_be_tx_credit  credit;
	bool                    put_successful;
	int                     rc;

	do {
		act = qget(b->b_q);
		if (act->a_opc != AO_DONE) {
			credit = M0_BE_TX_CREDIT(0, 0);
			act->a_builder = b;
			rc = act->a_ops->o_prep(act, &credit);
			if (rc != 0)
				continue;
			M0_BE_OP_SYNC(op, put_successful =
				      m0_be_tx_bulk_put(tb, &op, &credit, 0,
							act->a_opc, act));
			if (!put_successful)
				break;
		}
	} while (act->a_opc != AO_DONE);
	m0_be_tx_bulk_end(tb);
}

static void builder_thread(struct builder *b)
{
	struct m0_be_tx_bulk_cfg tb_cfg;
	struct m0_be_tx_bulk     tb = {};
	int                      rc;

	tb_cfg           =  default_tb_cfg;
	tb_cfg.tbc_dom   =  b->b_dom;
	tb_cfg.tbc_datum =  b;
	tb_cfg.tbc_do    = &builder_do,
	tb_cfg.tbc_done  = &builder_done,

	rc = m0_be_tx_bulk_init(&tb, &tb_cfg);
	if (rc == 0) {
		M0_BE_OP_SYNC(op, ({
				   m0_be_tx_bulk_run(&tb, &op);
				   builder_work_put(&tb, b);
				   }));
		rc = m0_be_tx_bulk_status(&tb);
		m0_be_tx_bulk_fini(&tb);
	}

	/**
	 * Below clean up used as m0_be_ut_backend_fini()  fails because of
	 * unlocked thread's sm group. Simplify this task and call the exit
	 * function for builder thread.
	 */
	if (&b->b_backend != NULL) {
		(void)m0_be_ut_backend_sm_group_lookup(&b->b_backend);
		m0_be_ut_backend_thread_exit(&b->b_backend);
	}
}

static int ad_dom_init(struct builder *b)
{
	int                      result;
	struct m0_stob_ad_module *module = &m0_get()->i_stob_ad_module;
	struct ad_domain_map     *ad;
	char                     *cfg_init;
	char 			  location[64];
	int                       len;
	struct m0_stob           *stob;
	struct m0_stob_id         stob_id;
	struct m0_stob_domain    *dom;
	struct m0_stob_domain    *stob_dom;
	char 			 *stob_location;
	char                     *str_cfg_init = "directio=true";
	struct ad_dom_info       *adom_info;
	uint64_t		  ad_dom_count;

	if (disable_directio)
		str_cfg_init = "directio=false";

	stob_location = m0_alloc(strlen(b->b_stob_path) + 20);
	if (stob_location == NULL)
		return M0_ERR(-ENOMEM);

	sprintf(stob_location, "linuxstob:%s%s",
		b->b_stob_path[0] == '/' ? "" : "./", b->b_stob_path);
	result = m0_stob_domain_init(stob_location, str_cfg_init, &dom);
	if (result != 0) {
		m0_free(stob_location);
		return M0_ERR(result);
	}
	b->b_ad_dom = dom;

	ad_dom_count = ad_domains_tlist_length(&module->sam_domains);
	M0_ALLOC_ARR(b->b_ad_domain, ad_dom_count);
	M0_ALLOC_ARR(b->b_ad_info, ad_dom_count);
	b->b_ad_dom_count = 0;
	m0_tl_for(ad_domains, &module->sam_domains, ad) {
		M0_ASSERT(b->b_ad_dom_count < ad_dom_count);
		M0_ALLOC_PTR(adom_info);
		if (adom_info == NULL) {
			result = -ENOMEM;
			break;
		}
		stob = NULL;
		stob_dom = NULL;
		b->b_ad_domain[b->b_ad_dom_count] = ad->adm_dom;

		m0_stob_id_make(0, ad->adm_dom->sad_bstore_id.si_fid.f_key,
				&dom->sd_id, &stob_id);
		result = m0_stob_find(&stob_id, &stob);
		M0_ASSERT(result == 0);

		if (m0_stob_state_get(stob) == CSS_UNKNOWN) {
			result = m0_stob_locate(stob);
			if (result != 0)
				break;
		}

		if (m0_stob_state_get(stob) == CSS_NOENT) {
			result = m0_stob_create(stob, NULL, NULL);
			if (result != 0)
				break;
		}

		len = snprintf(NULL, 0, "adstob:%"PRIu64,
			       ad->adm_dom->sad_bstore_id.si_fid.f_key);
		result = snprintf(location, len + 1, "adstob:%"PRIu64,
			      ad->adm_dom->sad_bstore_id.si_fid.f_key);
		M0_ASSERT_INFO(result == len, "result=%d", result);

		m0_stob_ad_init_cfg_make(&cfg_init,
				b->b_seg->bs_domain);
		result = m0_stob_domain_init(location, cfg_init, &stob_dom);
		if (result != 0)
			break;

		adom_info->stob = stob;
		adom_info->stob_dom = stob_dom;
		b->b_ad_info[b->b_ad_dom_count] = adom_info;
		b->b_ad_dom_count++;
	} m0_tl_endfor;

	/* In case of initializing ad_dom failure, cleanup meomory allocated*/
	if (result != 0)
		ad_dom_fini(b);
	m0_free(stob_location);
	return M0_RC(result);
}

static int builder_init(struct builder *b)
{
	struct m0_be_ut_backend *ub = &b->b_backend;
	static struct m0_fid     fid = M0_FID_TINIT('r', 1, 1);
	int                      result;
	int                      i;

	result = M0_REQH_INIT(&b->b_reqh,
			      .rhia_dtm     = (void *)1,
			      .rhia_mdstore = (void *)1,
			      .rhia_fid     = &fid);
	if (result != 0)
		return M0_ERR(result);
	ub->but_stob_domain_location = m0_alloc(strlen(b->b_dom_path) + 20);
	if (ub->but_stob_domain_location == NULL)
		return M0_ERR(-ENOMEM); /* No cleanup, fatal anyway. */
	sprintf(ub->but_stob_domain_location, "linuxstob:%s%s",
		b->b_dom_path[0] == '/' ? "" : "./", b->b_dom_path);
	ub->but_dom_cfg.bc_engine.bec_reqh = &b->b_reqh;
	m0_be_ut_backend_cfg_default(&ub->but_dom_cfg);
	be_cfg_default_init(&ub->but_dom_cfg, &default_tb_cfg);
	/* Check for any BE configuration overrides. */
	if (b->b_be_config_file) {
		result = be_cfg_from_yaml_update(b->b_be_config_file,
					         &ub->but_dom_cfg);
		if (result != 0)
			return M0_ERR(result);
	}
	result = m0_be_ut_backend_init_cfg(ub, &ub->but_dom_cfg, false);
	if (result != 0)
		return M0_ERR(result);
	b->b_dom  = &ub->but_dom;
	b->b_seg0 = m0_be_domain_seg0_get(b->b_dom);
	M0_ASSERT(b->b_seg0 != NULL);
	b->b_seg = m0_be_domain_seg_first(b->b_dom);
	if (b->b_seg == NULL) {
		m0_be_ut_backend_seg_add2(ub, b->b_size, true, NULL, &b->b_seg);
		M0_ASSERT(b->b_seg != NULL);
	}
	printf("\nDestination segment header generation\n");
	generation_id_print(b->b_seg->bs_gen);
	printf("\n");
	/*
	 * Flush immediately to avoid losing this information within other lines
	 * coming on the screen at the same time.
	 */
	fflush(stdout);

	result = m0_reqh_be_init(&b->b_reqh, b->b_seg);
	if (result != 0)
		return M0_ERR(result);

	result = m0_ctg_store_init(b->b_dom);
	if (result != 0)
		return M0_ERR(result);

	result = ctg_pver_fid_get(&b->b_pver_fid);
	if (result != 0)
		return M0_ERR(result);
	/** Initialise cob ios domain. */
	m0_ios_cdom_get(&b->b_reqh, &b->b_ios_cdom);
	m0_cob_domain_init(b->b_ios_cdom, b->b_seg);

	/** Initialise cob mds domain. */
	b->b_mds_cdom = m0_reqh_lockers_get(&b->b_reqh,
					    m0_get()->i_mds_cdom_key);
	m0_cob_domain_init(b->b_mds_cdom, b->b_seg);

	for (i = 0; i < AO_NR - AO_EMAP_FIRST; i++)
		m0_mutex_init(&b->b_emaplock[i]);
	m0_mutex_init(&b->b_coblock);
	m0_mutex_init(&b->b_ctglock);
	result = ad_dom_init(b);
	if (result != 0)
		return M0_ERR(result);
	result = M0_THREAD_INIT(&b->b_thread, struct builder *,
				NULL, &builder_thread, b, "builder");
	return M0_RC(result);
}

static void ad_dom_fini(struct builder *b)
{
	int i;
	int rc;

	/* Cleanup m0_stob_domains for AD domain initialized in ad_dom_init() */
	for (i = 0; i < b->b_ad_dom_count; ++i) {
		m0_stob_domain_fini(b->b_ad_info[i]->stob_dom);
		rc = m0_stob_destroy(b->b_ad_info[i]->stob, NULL);
		if (rc != 0)
			warn("Failed m0_stob_destroy for AD domain rc=%d", rc);
		m0_free(b->b_ad_info[i]);
	}
	m0_stob_domain_fini(b->b_ad_dom);
	m0_free(b->b_ad_info);
	m0_free(b->b_ad_domain);

}

static void builder_fini(struct builder *b)
{
	int i;

	m0_thread_join(&b->b_thread);
	m0_thread_fini(&b->b_thread);

	for (i = 0; i < AO_NR - AO_EMAP_FIRST; i++)
		m0_mutex_fini(&b->b_emaplock[i]);
	m0_mutex_fini(&b->b_coblock);
	m0_mutex_fini(&b->b_ctglock);

	m0_ctg_store_fini();
	m0_reqh_be_fini(&b->b_reqh);
	ad_dom_fini(b);
	m0_be_ut_backend_fini(&b->b_backend);
	m0_reqh_fini(&b->b_reqh);
	m0_free(b->b_backend.but_stob_domain_location);

	printf("builder: actions: %9"PRId64"\n", b->b_act);
}

/**
 * These values provided the maximum builder performance after experiments on
 * hardware.
 */
static void  be_cfg_default_init(struct m0_be_domain_cfg  *dom_cfg,
				 struct m0_be_tx_bulk_cfg *tb_cfg)
{
	dom_cfg->bc_engine.bec_tx_active_max = 256;
	dom_cfg->bc_engine.bec_group_nr = 5;
	dom_cfg->bc_engine.bec_group_cfg.tgc_tx_nr_max = 128;
	dom_cfg->bc_engine.bec_group_cfg.tgc_size_max =
					M0_BE_TX_CREDIT(5621440, 961373440);
	dom_cfg->bc_engine.bec_group_cfg.tgc_payload_max = 367772160;
	dom_cfg->bc_engine.bec_tx_size_max =
					M0_BE_TX_CREDIT(1 << 18, 44UL << 20);
	dom_cfg->bc_engine.bec_tx_payload_max = 1 << 21;
	dom_cfg->bc_engine.bec_group_freeze_timeout_min =
						1ULL * M0_TIME_ONE_MSEC;
	dom_cfg->bc_engine.bec_group_freeze_timeout_max =
						50ULL * M0_TIME_ONE_MSEC;
	dom_cfg->bc_engine.bec_group_freeze_timeout_limit =
						60000ULL * M0_TIME_ONE_MSEC;
	dom_cfg->bc_log.lc_full_threshold = 20 * (1 << 20);
	dom_cfg->bc_pd_cfg.bpdc_seg_io_nr = 5;
	dom_cfg->bc_log_discard_cfg.ldsc_items_max = 0x100;
	dom_cfg->bc_log_discard_cfg.ldsc_items_threshold = 0x80;
	dom_cfg->bc_log_discard_cfg.ldsc_sync_timeout =
						M0_TIME_ONE_SECOND * 60ULL;
	tb_cfg->tbc_workers_nr = 64;
	tb_cfg->tbc_work_items_per_tx_max = 100;
}

static void be_cfg_update(struct m0_be_domain_cfg *cfg,
			  const char              *str_key,
			  const char              *str_value)
{
	uint64_t  value1_64;
	uint64_t  value2_64;
	uint64_t  value1_32;
	char     *s1;
	char     *s2;
	bool      value_overridden = true;

	if (m0_streq(str_key, "tgc_size_max")  ||
	    m0_streq(str_key, "bec_tx_size_max")) {

		/** Cover variables accepting two comma-separated values. */

		s1 = m0_strdup(str_value);
		s2 = strchr(s1, ',');

		*s2 = '\0';
		s2++;

		value1_64 = m0_strtou64(s1, NULL, 10);
		value2_64 = m0_strtou64(s2, NULL, 10);

		m0_free(s1);

		M0_ASSERT_INFO((value1_64 != ULLONG_MAX) &&
				(value2_64 != ULLONG_MAX),
				"Invalid value %s for variable %s in yaml file.",
			        str_value, str_key);

		if (m0_streq(str_key, "tgc_size_max")) {
			cfg->bc_engine.bec_group_cfg.tgc_size_max =
					M0_BE_TX_CREDIT(value1_64, value2_64);
		} else {
			cfg->bc_engine.bec_tx_size_max =
					M0_BE_TX_CREDIT(value1_64, value2_64);
		}
	} else if (m0_streq(str_key, "bpdc_seg_io_nr") ||
		   m0_streq(str_key, "ldsc_items_max") ||
		   m0_streq(str_key, "ldsc_items_threshold")) {

		/** Cover variables accepting a single 32-bit value */

		value1_32 =m0_strtou32(str_value, NULL, 10);

		M0_ASSERT_INFO((value1_32 != ULONG_MAX),
				"Invalid value %s for variable %s in yaml file.",
			        str_value, str_key);

		if (m0_streq(str_key, "bpdc_seg_io_nr")) {
			cfg->bc_pd_cfg.bpdc_seg_io_nr = value1_32;
		} else if (m0_streq(str_key, "ldsc_items_max")) {
			cfg->bc_log_discard_cfg.ldsc_items_max = value1_32;
		} else if (m0_streq(str_key, "ldsc_items_threshold")) {
			cfg->bc_log_discard_cfg.ldsc_items_threshold =
								value1_32;
		}
	} else {

		/** These variables accept a single 64-bit value. */
		value1_64 = m0_strtou64(str_value, NULL, 10);

		M0_ASSERT_INFO((value1_64 != ULLONG_MAX),
				"Invalid value %s for variable %s in yaml file.",
			        str_value, str_key);

		if (m0_streq(str_key, "bec_tx_active_max"))
			cfg->bc_engine.bec_tx_active_max = value1_64;
		else if (m0_streq(str_key, "bec_group_nr"))
			cfg->bc_engine.bec_group_nr = value1_64;
		else if (m0_streq(str_key, "tgc_tx_nr_max"))
			cfg->bc_engine.bec_group_cfg.tgc_tx_nr_max = value1_64;
		else if (m0_streq(str_key, "tgc_payload_max"))
			cfg->bc_engine.bec_group_cfg.tgc_payload_max =
								value1_64;
		else if (m0_streq(str_key, "bec_tx_payload_max"))
			cfg->bc_engine.bec_tx_payload_max = value1_64;
		else if (m0_streq(str_key, "bec_group_freeze_timeout_min"))
			cfg->bc_engine.bec_group_freeze_timeout_min = value1_64;
		else if (m0_streq(str_key, "bec_group_freeze_timeout_max"))
			cfg->bc_engine.bec_group_freeze_timeout_max = value1_64;
		else if (m0_streq(str_key, "bec_group_freeze_timeout_limit"))
			cfg->bc_engine.bec_group_freeze_timeout_limit =
								value1_64;
		else if (m0_streq(str_key, "lc_full_threshold"))
			cfg->bc_log.lc_full_threshold = value1_64;
		else if (m0_streq(str_key, "ldsc_sync_timeout"))
			cfg->bc_log_discard_cfg.ldsc_sync_timeout = value1_64;
		else
			value_overridden = false;
	}

	if (value_overridden) {
		printf("%s = %s\n", str_key, str_value);
		fflush(stdout);
	}
}

static int  be_cfg_from_yaml_update(const char              *yaml_file,
				    struct m0_be_domain_cfg *cfg)
{
	FILE         *fp;
	yaml_parser_t parser;
	yaml_token_t  token;
	char         *scalar_value;
	char          key[MAX_KEY_LEN];
	char          value[MAX_VALUE_LEN];
	int           rc;
	bool          is_key = false;
	bool          key_received = false;
	bool          value_received = false;

	fp = fopen(yaml_file, "r");
	M0_ASSERT(fp != NULL);

	if (!yaml_parser_initialize(&parser)) {
		printf("Failed to initialize yaml parser.\n");
		fclose(fp);
		return -ENOMEM;
	}

	yaml_parser_set_input_file(&parser, fp);

	printf("Changed following BE defaults:\n");

	do {
		rc = 0;
		yaml_parser_scan(&parser, &token);
		switch (token.type) {
			case YAML_KEY_TOKEN:
				is_key = true;
				break;
			case YAML_VALUE_TOKEN:
				is_key = false;
				break;
			case YAML_SCALAR_TOKEN:
				scalar_value = (char *)token.data.scalar.value;
				if (is_key) {
					strncpy(key, scalar_value, sizeof(key));
					key[sizeof(key) - 1] = '\0';
					key_received = true;
				} else {
					strncpy(value, scalar_value,
								sizeof(value));
					value[sizeof(value) - 1] = '\0';
					value_received = true;
				}

				if (key_received && value_received) {
					be_cfg_update(cfg, key, value);
					key_received = false;
					value_received = false;
				}
				break;
			case YAML_NO_TOKEN:
				rc = -EINVAL;
				break;
			default:
				break;
		}

		if (rc != 0) {
			fclose(fp);
			printf("Failed to parse %s\n", key);
			return rc;
		}

		if (token.type != YAML_STREAM_END_TOKEN)
			yaml_token_delete(&token);

	} while (token.type != YAML_STREAM_END_TOKEN);

	yaml_token_delete(&token);

	yaml_parser_delete(&parser);

	fclose(fp);

	return 0;
}

static void *builder_action(struct builder *b, size_t len,
			    enum action_opcode opc,
			    const struct action_ops *ops)
{
	struct action *act;

	M0_PRE(len >= sizeof *act);

	act = m0_alloc(len);
	M0_ASSERT(act != NULL); /* Can we handle this? */
	act->a_opc = opc;
	act->a_ops = ops;
	act->a_builder = b;
	return act;
}

static int format_header_verify(const struct m0_format_header *h,
				uint16_t rtype)
{
	struct m0_format_tag    tag = {};

	m0_format_header_unpack(&tag, h);
	if (memcmp(&tag, &rt[rtype].r_tag, sizeof tag) != 0)
		return M0_ERR(-EPROTO);
	return 0;
}

static bool btree_node_pre_is_valid(const struct m0_be_bnode *node,
				    struct scanner *s)
{
	M0_PRE(node != NULL);
	return
		(node->bt_num_active_key < ARRAY_SIZE(node->bt_kv_arr))    &&
		(m0_fid_tget(&node->bt_backlink.bli_fid) == 'b')       	   &&
		(node->bt_isleaf == (node->bt_level == 0))                 &&
		m0_forall(i, node->bt_num_active_key,
			  node->bt_kv_arr[i].btree_key != NULL &&
			  node->bt_kv_arr[i].btree_val != NULL &&
			  m0_be_seg_contains(s->s_seg,
					     node->bt_kv_arr[i].btree_key) &&
			  m0_be_seg_contains(s->s_seg,
					     node->bt_kv_arr[i].btree_val));
}

static bool btree_node_post_is_valid(const struct m0_be_bnode *node,
				     const struct m0_be_btree_kv_ops *ops)
{
	M0_PRE(node != NULL);
	return	ergo(node->bt_num_active_key > 1,
		     m0_forall(i, node->bt_num_active_key - 1,
			       ops->ko_compare(node->bt_kv_arr[i+1].btree_key,
					       node->bt_kv_arr[i].btree_key)));
}

static bool btree_kv_is_valid(struct m0_be_bnode *node,
			      int index, struct m0_buf *key)
{
	M0_PRE(node != NULL);
	return m0_align(key->b_nob, sizeof(void *)) ==
		(uint64_t)node->bt_kv_arr[index].btree_val -
		(uint64_t)node->bt_kv_arr[index].btree_key;
}

static bool btree_kv_post_is_valid(struct scanner *s,
				   struct m0_buf *key, struct m0_buf *val)
{
	return s->s_max_reg_size > key->b_nob + val->b_nob;
}

static void btree_bad_kv_count_update(uint64_t type, int count)
{
	M0_LOG(M0_DEBUG, "Discarded kv = %d from btree = %"PRIu64, count, type);
	bt[type].b_stats.c_kv_bad += count;
}

static bool fid_without_type_eq(const struct m0_fid *fid0,
				const struct m0_fid *fid1)
{
	return m0_fid_eq(&M0_FID_TINIT(0, fid0->f_container, fid0->f_key),
			 &M0_FID_TINIT(0, fid1->f_container, fid1->f_key));
}

static struct cache_slot *cache_lookup(struct cache *c, struct m0_fid *fid)
{
	int i;
	int idx;

	for (i = 0, idx = c->c_head - 1; i < CACHE_SIZE; ++i, --idx) {
		if (idx < 0)
			idx = CACHE_SIZE - 1;
		if (m0_fid_eq(&c->c_slot[idx].cs_fid, fid))
			return &c->c_slot[idx];
		if (!m0_fid_is_set(&c->c_slot[idx].cs_fid))
			break;
	}
	return NULL;
}

static struct cache_slot *cache_insert(struct cache *c,
				       const struct m0_fid *fid)
{
	struct cache_slot *slot =  &c->c_slot[c->c_head];

	slot->cs_fid   = *fid;
	slot->cs_flags =  0;
	slot->cs_tree  =  NULL;

	if (++c->c_head == CACHE_SIZE)
		c->c_head = 0;
	return slot;
}

/**
 * Get config root pool version fid from key and value which are generated by
 * mkfs. In EES, Layout is fixed with this pool fid.
 */
static int ctg_pver_fid_get(struct m0_fid *fid)
{
	struct m0_buf key;
	struct m0_buf val;
	uint8_t       kdata[M0_CAS_CTG_KV_HDR_SIZE + sizeof(struct m0_fid)];
	uint8_t       vdata[M0_CAS_CTG_KV_HDR_SIZE +
		            sizeof(struct m0_dix_layout)];
	uint64_t      i;
	int	      rc;

	*((uint64_t *)(kdata)) = sizeof(struct m0_fid);
	key = M0_BUF_INIT(ARRAY_SIZE(kdata), kdata);
	val = M0_BUF_INIT(ARRAY_SIZE(vdata), vdata);

	M0_CASSERT(M0_DIX_FID_DEVICE_ID_BITS > 0);
	for (i = 0; i < M0_DIX_FID_DEVICE_ID_BITS; i++) {
		*((struct m0_fid *)(kdata + M0_CAS_CTG_KV_HDR_SIZE)) =
			M0_FID_TINIT('T', i << M0_DIX_FID_DEVICE_ID_OFFSET,
				     0x02);
		rc = M0_BE_OP_SYNC_RET(op,
				       m0_be_btree_lookup(&m0_ctg_ctidx()->
							  cc_tree, &op,
							  &key, &val),
				       bo_u.u_btree.t_rc);
		if (rc == 0)
			break;
	}
	if (rc == 0) {
		*fid = ((struct m0_dix_layout *)
			(vdata + M0_CAS_CTG_KV_HDR_SIZE))->u.dl_desc.ld_pver;
	} else
		M0_LOG(M0_ERROR, "Failed to get pool version fid, rc = %d", rc);
	return rc;
}

static int ctg_kv_get(struct scanner *s, const void *addr, struct m0_buf *kv)
{
	uint64_t len;
	int	 result;

	M0_CASSERT(sizeof len == M0_CAS_CTG_KV_HDR_SIZE);
	result = deref(s, addr, &len, M0_CAS_CTG_KV_HDR_SIZE);
	if (result != 0)
		return M0_ERR(result);
	if (len > s->s_max_reg_size)
		return M0_ERR(-EPERM);
	result = m0_buf_alloc(kv, len + M0_CAS_CTG_KV_HDR_SIZE);
	if (result != 0)
		return M0_ERR(result);
	*(uint64_t *)kv->b_addr = len;
	result = deref(s, addr + M0_CAS_CTG_KV_HDR_SIZE,
		       kv->b_addr + M0_CAS_CTG_KV_HDR_SIZE, len);
	if (result != 0)
		m0_buf_free(kv);
	return result;
}

static int ctg_btree_fid_get(struct m0_buf *kbuf, struct m0_fid *fid)
{
	if (*(uint64_t *)(kbuf->b_addr) != sizeof (struct m0_fid))
		return -EINVAL;

	*fid = *(struct m0_fid *)(kbuf->b_addr + M0_CAS_CTG_KV_HDR_SIZE);
	m0_fid_tchange(fid, 'b');
	return 0;
}

static int ctg_proc(struct scanner *s, struct btype *b,
		    struct m0_be_bnode *node)
{
	struct m0_be_bnode           n = {};
	struct m0_be_btree_backlink *bl = &node->bt_backlink;
	struct m0_buf                kl[ARRAY_SIZE(node->bt_kv_arr)] = {};
	struct m0_buf                vl[ARRAY_SIZE(node->bt_kv_arr)] = {};
	struct ctg_action           *ca;
	struct m0_fid                fid;
	int                          i;
	int                          rc;
	bool                         ismeta;

	M0_PRE(bl->bli_type == M0_BBT_CAS_CTG);
	if (!btree_node_pre_is_valid(node, s)                          ||
	    fid_without_type_eq(&bl->bli_fid, &m0_cas_dead_index_fid)) {
		btree_bad_kv_count_update(bl->bli_type,
					  node->bt_num_active_key);
		return 0;
	}
	for (i = 0; i < node->bt_num_active_key; i++) {
		if (ctg_kv_get(s, node->bt_kv_arr[i].btree_key,
			       &kl[n.bt_num_active_key]) != 0) {
			btree_bad_kv_count_update(bl->bli_type, 1);
			continue;
		}
		if (btree_kv_is_valid(node, i, &kl[n.bt_num_active_key]) &&
		    ctg_kv_get(s, node->bt_kv_arr[i].btree_val,
			       &vl[n.bt_num_active_key]) == 0) {

			if (btree_kv_post_is_valid(s, &kl[n.bt_num_active_key],
						   &vl[n.bt_num_active_key]) &&
			    !dry_run) {
				n.bt_kv_arr[n.bt_num_active_key].btree_key =
					kl[n.bt_num_active_key].b_addr;
				n.bt_num_active_key++;
			} else {
				m0_buf_free(&kl[n.bt_num_active_key]);
				m0_buf_free(&vl[n.bt_num_active_key]);
			}
		} else {
			btree_bad_kv_count_update(bl->bli_type, 1);
			m0_buf_free(&kl[n.bt_num_active_key]);
		}
	}
	if (!btree_node_post_is_valid(&n, m0_ctg_btree_ops())) {
		btree_bad_kv_count_update(bl->bli_type, n.bt_num_active_key);
		for (i = 0; i < n.bt_num_active_key; i++) {
			m0_buf_free(&kl[i]);
			m0_buf_free(&vl[i]);
		}
		return  0;
	}
	ismeta = fid_without_type_eq(&bl->bli_fid, &m0_cas_meta_fid) ||
		fid_without_type_eq(&bl->bli_fid, &m0_cas_ctidx_fid);
	for (i = 0; i < n.bt_num_active_key; i++) {
		if (ismeta) {
			rc = ctg_btree_fid_get(&kl[i], &fid);
			m0_buf_free(&kl[i]);
			m0_buf_free(&vl[i]);
			if (rc != 0                                       ||
			    fid_without_type_eq(&fid, &M0_FID_INIT(0, 0)) ||
			    fid_without_type_eq(&fid, &m0_cas_meta_fid)   ||
			    fid_without_type_eq(&fid, &m0_cas_ctidx_fid)  ||
			    fid_without_type_eq(&fid, &m0_cas_dead_index_fid))
				continue;
			if (rc != 0)
				btree_bad_kv_count_update(bl->bli_type, 1);
		} else if (dry_run)
			continue;
		 else
			fid = bl->bli_fid;
		ca = scanner_action(sizeof *ca, AO_CTG, &ctg_ops);
		ca->cta_fid = fid;
		ca->cta_key = kl[i];
		ca->cta_val = vl[i];
		ca->cta_ismeta = ismeta;
		qput(s->s_q, (struct action *)ca);
	}
	return 0;
}

static struct cache_slot *ctg_getslot_insertcred(struct ctg_action *ca,
						 struct builder *b,
						 struct m0_fid *cas_ctg_fid,
						 struct m0_be_tx_credit *accum)
{
	struct cache_slot *slot;

	slot = cache_lookup(&b->b_cache, cas_ctg_fid);
	if (slot == NULL)
		slot = cache_insert(&b->b_cache, cas_ctg_fid);
	m0_ctg_create_credit(accum);
	ca->cta_cid.ci_fid = ca->cta_fid;
	m0_fid_tchange(&ca->cta_cid.ci_fid, 'T');
	ca->cta_cid.ci_layout.dl_type = DIX_LTYPE_DESCR;
	m0_dix_ldesc_init(&ca->cta_cid.ci_layout.u.dl_desc,
			  &(struct m0_ext) { .e_start = 0,
			  .e_end = IMASK_INF },
			  1, HASH_FNC_CITY, &b->b_pver_fid);
	m0_ctg_ctidx_insert_credits(&ca->cta_cid, accum);
	return slot;
}

static int ctg_prep(struct action *act, struct m0_be_tx_credit *accum)
{
	struct ctg_action      *ca    = M0_AMB(ca, act, cta_act);
	struct m0_be_btree      tree  = {};

	m0_mutex_lock(&beck_builder.b_ctglock);
	ca->cta_slot = ctg_getslot_insertcred(ca, act->a_builder,
					      &ca->cta_fid, accum);
	m0_be_btree_insert_credit(&tree, 1, ca->cta_key.b_nob,
				  ca->cta_val.b_nob, accum);
	m0_mutex_unlock(&beck_builder.b_ctglock);
	return 0;
}

static struct m0_cas_ctg *ctg_create_meta(struct ctg_action *ca,
					  struct m0_be_tx *tx)
{
	struct m0_cas_ctg *cas_ctg;
	int		   rc;
	struct m0_fid      cfid;

	cfid = ca->cta_fid;
	m0_fid_tchange(&cfid, 'T');
	rc = m0_ctg_create(ca->cta_act.a_builder->b_seg, tx, &cas_ctg, &cfid);
	if (rc == 0) {
		rc = m0_ctg__meta_insert(&m0_ctg_meta()->cc_tree, &cfid,
					 cas_ctg, tx)  ?:
			m0_ctg_ctidx_insert_sync(&ca->cta_cid, tx);
		if (rc != 0)
			M0_LOG(M0_DEBUG, "Failed to insert meta record rc=%d",
			       rc);
	} else {
		M0_LOG(M0_DEBUG, "BTree not created rc=%d", rc);
		return NULL;
	}
	return cas_ctg;
}

static void ctg_act(struct action *act, struct m0_be_tx *tx)
{
	struct ctg_action *ca = M0_AMB(ca, act, cta_act);
	struct m0_cas_ctg *cc;
	int                rc;

	m0_mutex_lock(&beck_builder.b_ctglock);
	if (ca->cta_slot->cs_tree == NULL) {
		rc = m0_ctg_meta_find_ctg(m0_ctg_meta(),
					  &M0_FID_TINIT('T',
							ca->cta_fid.f_container,
							ca->cta_fid.f_key),
					  &cc);
		if (rc == -ENOENT) {
			cc = ctg_create_meta(ca, tx);
		} else if (rc != 0) {
			M0_LOG(M0_DEBUG, "Btree not found rc=%d", rc);
			m0_mutex_unlock(&beck_builder.b_ctglock);
			return;
		}
		if (cc != NULL) {
			m0_ctg_try_init(cc);
			ca->cta_slot->cs_tree = &cc->cc_tree;
		}
	}
	if (!ca->cta_ismeta && ca->cta_slot->cs_tree != NULL) {
		rc = M0_BE_OP_SYNC_RET(op,
				       m0_be_btree_insert(ca->cta_slot->cs_tree,
							  tx, &op, &ca->cta_key,
							  &ca->cta_val),
				       bo_u.u_btree.t_rc);

		if (rc == 0) {
			m0_ctg_state_inc_update(tx, ca->cta_key.b_nob -
						M0_CAS_CTG_KV_HDR_SIZE +
						ca->cta_val.b_nob);
		} else
			M0_LOG(M0_DEBUG, "Failed to insert record rc=%d", rc);
	}
	m0_mutex_unlock(&beck_builder.b_ctglock);
}

static void ctg_fini(struct action *act)
{
	struct ctg_action *ca = M0_AMB(ca, act, cta_act);

	if (!ca->cta_ismeta) {
		m0_buf_free(&ca->cta_key);
		m0_buf_free(&ca->cta_val);
	}
}

static const struct action_ops ctg_ops = {
	.o_prep = &ctg_prep,
	.o_act  = &ctg_act,
	.o_fini = &ctg_fini
};

static bool qinvariant(const struct queue *q)
{
	return  _0C((q->q_nr == 0) == (q->q_head == NULL &&
				       q->q_tail == NULL)) &&
		_0C((q->q_nr == 1) == (q->q_head == q->q_tail &&
				       q->q_head != NULL)) &&
		_0C((q->q_nr >  1) == (q->q_head != q->q_tail &&
				       q->q_head != NULL &&
				       q->q_tail != NULL)) &&
		_0C(q->q_nr <= q->q_max);
}

static void qinit(struct queue *q, uint64_t maxnr)
{
	int result;
	M0_PRE(M0_IS0(q));
	result = pthread_mutex_init(&q->q_lock, NULL);
	M0_ASSERT(result == 0);
	result = pthread_cond_init(&q->q_cond, NULL);
	M0_ASSERT(result == 0);
	q->q_max = maxnr;
	M0_POST(qinvariant(q));
}

static void qfini(struct queue *q)
{
	int result;
	M0_PRE(q->q_nr == 0 && qinvariant(q));
	result = pthread_cond_destroy(&q->q_cond);
	M0_ASSERT(result == 0);
	result = pthread_mutex_destroy(&q->q_lock);
	M0_ASSERT(result == 0);
	M0_SET0(q);
}

static void qput(struct queue *q, struct action *act)
{
	M0_ASSERT(act->a_next == NULL);
	M0_ASSERT(act->a_prev == NULL);

	pthread_mutex_lock(&q->q_lock);
	M0_PRE(qinvariant(q));
	while (q->q_nr == q->q_max)
		pthread_cond_wait(&q->q_cond, &q->q_lock);
	act->a_next = q->q_head;
	if (q->q_head != NULL)
		q->q_head->a_prev = act;
	q->q_head = act;
	if (q->q_tail == NULL) {
		q->q_tail = act;
		pthread_cond_broadcast(&q->q_cond);
	}
	q->q_nr++;
	M0_POST(qinvariant(q));
	pthread_mutex_unlock(&q->q_lock);
}

static struct action *qpeek(struct queue *q)
{
	struct action *act;

	act = q->q_tail;
	if (act != NULL) {
		q->q_tail = act->a_prev;
		if (q->q_tail == NULL)
			q->q_head = NULL;
		else
			q->q_tail->a_next = NULL;
		act->a_next = NULL;
		act->a_prev = NULL;
		if (q->q_nr == q->q_max)
			pthread_cond_broadcast(&q->q_cond);
		q->q_nr--;
	}
	return act;
}

static struct action *qget(struct queue *q)
{
	struct action *act;

	pthread_mutex_lock(&q->q_lock);
	M0_PRE(qinvariant(q));
	while (q->q_nr == 0)
		pthread_cond_wait(&q->q_cond, &q->q_lock);
	act = qpeek(q);
	M0_POST(qinvariant(q));
	pthread_mutex_unlock(&q->q_lock);
	return act;
}

static struct action *qtry(struct queue *q)
{
	struct action *act;

	pthread_mutex_lock(&q->q_lock);
	M0_PRE(qinvariant(q));
	act = qpeek(q);
	M0_POST(qinvariant(q));
	pthread_mutex_unlock(&q->q_lock);
	return act;
}

static const struct recops btreeops = {
	.ro_proc  = &btree,
	.ro_check = &btree_check
};

static const struct recops bnodeops = {
	.ro_proc  = &bnode,
	.ro_check = &bnode_check
};

static const struct recops seghdrops = {
	.ro_proc  = &seghdr,
	.ro_ver   = &seghdr_ver,
	.ro_check = &seghdr_check,
};

/**
 * Reads and stores cob namespace tree key value pair.
 * @param[in]  s   scanner.
 * @param[in]  kv  key value pair read by scanner.
 * @param[out] key stores cob namespace key.
 * @param[out] val stores cob namespace record.
 */
static int cob_kv_get(struct scanner *s, const struct be_btree_key_val  *kv,
		      struct m0_buf *key, struct m0_buf *val)
{
	int		    result;
	struct m0_cob_nskey nskey;

	/** Read cob nskey. */
	result = deref(s, kv->btree_key, &nskey, sizeof nskey);
	if (result != 0)
		return M0_ERR(result);
	if (m0_cob_nskey_size(&nskey) > s->s_max_reg_size)
		return M0_ERR(-EPERM);
	result = m0_buf_alloc(key, m0_cob_nskey_size(&nskey));
	if (result != 0)
		return M0_ERR(result);
	*(struct m0_cob_nskey *)key->b_addr = nskey;
	result = deref(s, kv->btree_key + sizeof nskey,
		       key->b_addr + sizeof nskey,
		       nskey.cnk_name.b_len);
	if (result != 0) {
		m0_buf_free(key);
		return M0_ERR(result);
	}
	/** Read cob nsvalue. */
	result = deref(s, kv->btree_val,
		       val->b_addr, sizeof(struct m0_cob_nsrec));
	if (result != 0)
		m0_buf_free(key);
	return result;
}

/**
 * Reads the cob namespace key and record from cob namespace btree key value
 * pair. Puts the read cob namespae key and record in queue.
 * @param s scanner.
 * @param b btree type.
 * @param node btree node.
 */
static int cob_proc(struct scanner *s, struct btype *b,
		    struct m0_be_bnode *node)
{
	struct cob_action           *ca;
	int                          i;
	int                          rc;
	struct m0_be_btree_backlink *bb  = &node->bt_backlink;
	struct m0_cob_nskey         *nskey;
	uint64_t                     inv_cob_off;

	M0_PRE(bb->bli_type == M0_BBT_COB_NAMESPACE);

	for (i = 0; i < node->bt_num_active_key; i++) {
		ca = scanner_action(sizeof*ca, AO_COB,&cob_ops);
		ca->coa_fid = bb->bli_fid;

		ca->coa_val = M0_BUF_INIT(sizeof(struct m0_cob_nsrec),
					  ca->coa_valdata);
		rc = cob_kv_get(s, &node->bt_kv_arr[i], &ca->coa_key,
				&ca->coa_val);
		if (rc != 0) {
			btree_bad_kv_count_update(bb->bli_type, 1);
			m0_free(ca);
			continue;
		}

		if ((format_header_verify(ca->coa_val.b_addr,
					  M0_FORMAT_TYPE_COB_NSREC) == 0) &&
		    (m0_format_footer_verify(ca->coa_valdata, false) == 0)) {
			if (!dry_run)
				qput(s->s_q, (struct action *)ca);
			else {
				m0_buf_free(&ca->coa_key);
				m0_free(ca);
			}
		} else {
			if (s->s_print_invalid_oids) {
				nskey = ca->coa_key.b_addr;
				inv_cob_off = node->bt_kv_arr[i].btree_val -
					       s->s_seg->bs_addr;
				M0_LOG(M0_ERROR, "Found incorrect COB entry at "
				       "segment offset %"PRIx64" for GOB "
				       FID_F, inv_cob_off,
				       FID_P(&nskey->cnk_pfid));
			}

			btree_bad_kv_count_update(bb->bli_type, 1);
			m0_buf_free(&ca->coa_key);
			m0_free(ca);
		}
	}

	return 0;
}

/**
 * Allocates the credits for cob addition and deletion operation.
 * @param[in]  act  builder action.
 * @param[out] cred credits allocated for addition and deletion operation.
 */
static int cob_prep(struct action *act, struct m0_be_tx_credit *accum)
{
	struct cob_action      *ca = container_of(act, struct cob_action,
						  coa_act);
	struct m0_cob_nsrec    *nsrec = ca->coa_val.b_addr;
	struct m0_stob_id       stob_id;

	m0_mutex_lock(&beck_builder.b_coblock);
 	if (m0_fid_validate_cob(&nsrec->cnr_fid)) {
		m0_fid_convert_cob2adstob(&nsrec->cnr_fid, &stob_id);
		m0_cc_stob_cr_credit(&stob_id, accum);
	}
	m0_cob_tx_credit(act->a_builder->b_ios_cdom, M0_COB_OP_NAME_ADD,
			 accum);
	m0_cob_tx_credit(act->a_builder->b_ios_cdom, M0_COB_OP_NAME_DEL,
			 accum);
	m0_mutex_unlock(&beck_builder.b_coblock);
	return 0;
}

/**
 * Inserts the valid cob namespace key and records to cob namepace and object
 * index btree.
 * @param[in] act builder action.
 * @param[in] tx  backend transaction.
 */
static void cob_act(struct action *act, struct m0_be_tx *tx)
{
	struct cob_action        *ca = container_of(act, struct cob_action,
						    coa_act);
	struct m0_be_btree       *ios_ns = &act->a_builder->b_ios_cdom->
								cd_namespace;
	struct m0_be_btree       *mds_ns = &act->a_builder->b_mds_cdom->
								cd_namespace;
	struct m0_cob             cob = {};
	struct m0_cob_nskey      *nskey = ca->coa_key.b_addr;
	struct m0_cob_nsrec      *nsrec = ca->coa_val.b_addr;
	int                       rc = 0;
	struct m0_stob_id         stob_id;
	struct m0_stob_domain    *sdom;
	struct m0_stob_ad_domain *adom;
	struct m0_be_emap_cursor  it = {};
	struct m0_uint128         prefix;
	int			  id;

	m0_mutex_lock(&beck_builder.b_coblock);

	if (m0_fid_eq(&ca->coa_fid, &ios_ns->bb_backlink.bli_fid))
		m0_cob_init(act->a_builder->b_ios_cdom, &cob);
	else if (m0_fid_eq(&ca->coa_fid, &mds_ns->bb_backlink.bli_fid))
		m0_cob_init(act->a_builder->b_mds_cdom, &cob);
	else
		rc = -ENODATA;

	cob.co_nsrec = *nsrec;
	rc = rc ?: m0_cob_name_add(&cob, nskey, nsrec, tx);

	if (rc == 0 && m0_fid_validate_cob(&nsrec->cnr_fid)) {
		m0_fid_convert_cob2adstob(&nsrec->cnr_fid, &stob_id);
		sdom = m0_stob_domain_find_by_stob_id(&stob_id);
		adom = stob_ad_domain2ad(sdom);
		prefix = M0_UINT128(stob_id.si_fid.f_container,
				    stob_id.si_fid.f_key);
		emap_dom_find(&ca->coa_act,
			      &adom->sad_adata.em_mapping.bb_backlink.bli_fid,
			      &id);
		m0_mutex_lock(&beck_builder.b_emaplock[id]);
		rc = M0_BE_OP_SYNC_RET_WITH(&it.ec_op,
					    m0_be_emap_lookup(&adom->sad_adata,
							      &prefix, 0, &it),
					    bo_u.u_emap.e_rc);
		if (rc == 0)
			m0_be_emap_close(&it);
		else {
			rc = M0_BE_OP_SYNC_RET(op,
					       m0_be_emap_obj_insert(&adom->
								     sad_adata,
								     tx, &op,
								     &prefix,
								     AET_HOLE),
					       bo_u.u_emap.e_rc);
		}
		m0_mutex_unlock(&beck_builder.b_emaplock[id]);
	}
	m0_mutex_unlock(&beck_builder.b_coblock);
}

/**
 * Finalises the cob namespace operation.
 * @param[in] act builder action.
 */
static void cob_fini(struct action *act)
{
	struct cob_action *ca = container_of(act, struct cob_action, coa_act);

	m0_buf_free(&ca->coa_key);
}

static int noop_prep(struct action *act, struct m0_be_tx_credit *cred)
{
	return 0;
}

static void noop_act(struct action *act, struct m0_be_tx *tx)
{
}

static void noop_fini(struct action *act)
{
}

static const struct action_ops cob_ops = {
	.o_prep = &cob_prep,
	.o_act  = &cob_act,
	.o_fini = &cob_fini
};

static const struct action_ops done_ops = {
	.o_prep = &noop_prep,
	.o_act  = &noop_act,
	.o_fini = &noop_fini
};

static void test_queue(void)
{
	enum { NR = 10000, THREADS = 51, MAX = 40 };
	struct m0_thread tp[THREADS] = {};
	struct m0_thread tg[THREADS] = {};
	struct m0_thread tt[THREADS] = {};
	struct queue     q           = {};
	int              i;


	printf("\tQueue...");
	qinit(&q, MAX);
	for (i = 0; i < THREADS; ++i) {
		M0_THREAD_INIT(&tp[i], void *, NULL,
			LAMBDA(void, (void *nonce) {
				int n;
				struct action *act;

				for (n = 0; n < 2 * NR; ++n) { /* Twice. */
					M0_ALLOC_PTR(act);
					M0_ASSERT(act != NULL);
					qput(&q, act);
				}
				printf(" +%"PRIi64, q.q_nr);
			}), NULL, "qp-%d", i);
		M0_THREAD_INIT(&tg[i], void *, NULL,
			LAMBDA(void, (void *nonce) {
				int n = NR;
				struct action *act;

				while (n > 0) {
					act = qget(&q);
					M0_ASSERT(act != NULL);
					n--;
					m0_free(act);
				}
				printf(" -%"PRIi64, q.q_nr);
			}), NULL, "qg-%d", i);
		M0_THREAD_INIT(&tt[i], void *, NULL,
			LAMBDA(void, (void *nonce) {
				int n = NR;
				struct action *act;

				while (n > 0) {
					act = qtry(&q);
					if (act != NULL) {
						n--;
						m0_free(act);
					}
				}
				printf(" ?%"PRIi64, q.q_nr);
			}), NULL, "qt-%d", i);
	}
	for (i = 0; i < THREADS; ++i) {
		m0_thread_join(&tp[i]);
		m0_thread_fini(&tp[i]);
		m0_thread_join(&tg[i]);
		m0_thread_fini(&tg[i]);
		m0_thread_join(&tt[i]);
		m0_thread_fini(&tt[i]);
	}
	M0_ASSERT(q.q_nr == 0);
	qfini(&q);
	printf(" ok.\n");
}

static void test(void)
{
	test_queue();
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of be group */

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
