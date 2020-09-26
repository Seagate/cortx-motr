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

M0_TL_DESCR_DECLARE(ad_domains, M0_EXTERN);
M0_TL_DECLARE(ad_domains, M0_EXTERN, struct ad_domain_map);

struct queue;

struct scanner {
	FILE		    *s_file;
	off_t		     s_off;
	off_t		     s_pos;
	bool		     s_byte;
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
	AO_INIT = 1,
	AO_CTG,
	AO_DONE,
	AO_NR
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

struct queue {
	pthread_mutex_t q_lock;
	pthread_cond_t  q_cond;
	struct action  *q_head;
	struct action  *q_tail;
	uint64_t        q_nr;
	uint64_t        q_max;
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
	struct queue               b_qq;
	struct m0_be_tx_credit     b_cred;
	struct cache	           b_cache;
	uint64_t                   b_size;
	const char                *b_dom_path;
	const char                *b_stob_path; /**< stob path for ad_domain */

	uint64_t                   b_act;
	uint64_t                   b_tx;
	/** ioservice cob domain. */
	struct m0_cob_domain      *b_ios_cdom;
	/** mdservice cob domain. */
	struct m0_cob_domain      *b_mds_cdom;
	/**
	 * It is the fid of config root pool version which is used to
	 * construct dix layout.
	 */
	struct m0_fid              b_pver_fid;
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
static void generation_print(uint64_t gen);

static int  scanner_init   (struct scanner *s);
static int  builder_init   (struct builder *b);
static void builder_fini   (struct builder *b);
static void  ad_dom_fini    (struct builder *b);
static void builder_thread (struct builder *b);
static void builder_process(struct builder *b);

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

static void *action_alloc(size_t len, enum action_opcode opc,
			  const struct action_ops *ops);
static int   emap_proc(struct scanner *s, struct btype *b,
		       struct m0_be_bnode *node);
static int   emap_prep(struct action *act, struct m0_be_tx_credit *cred);
static void  emap_act(struct action *act, struct m0_be_tx *tx);
static void  emap_fini(struct action *act);
static int   emap_kv_get(struct scanner *s, const struct be_btree_key_val *kv,
		         struct m0_buf *key_buf, struct m0_buf *val_buf);
static void  sig_handler(int num);

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
	MAX_GEN    	=     256,
	MAX_QUEUED 	= 1000000,
	MAX_REC_SIZE    = 64*1024
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

static struct scanner s;
static struct builder b;
static struct gen g[MAX_GEN] = {};
static uint16_t gen_count = 0;

static bool  dry_run = false;
static bool  signaled = false;

#define FLOG(level, s)						\
	M0_LOG(level, "        at offset: %"PRId64" errno: %s (%i), eof: %i", \
	       ftell(s->s_file), strerror(errno), errno, feof(s->s_file))

#define RLOG(level, prefix, s, r, tag)					\
	M0_LOG(level, prefix " %li %s %hu:%hu:%u", s->s_off, recname(r), \
	       (tag)->ot_version, (tag)->ot_type, (tag)->ot_size)

static void sig_handler(int num)
{
	printf("Caught Signal %d \n", num);
	signaled = true;
}

int main(int argc, char **argv)
{
	struct m0              instance = {};
	const char            *spath    = NULL;
	int                    sfd      = 0; /* Use stdin by default. */
	bool                   ut       = false;
	bool                   version  = false;
	struct queue           q        = {};
	int                    result;
	struct m0_be_tx_credit max;

	m0_node_uuid_string_set(NULL);
	result = m0_init(&instance);
	if (result != 0)
		errx(EX_CONFIG, "Cannot initialise motr: %d.", result);
	result = init();
	if (result != 0)
		errx(EX_CONFIG, "Cannot initialise beck: %d.", result);
	result = M0_GETOPTS("m0_beck", argc, argv,
		   M0_STRINGARG('s', "Path to snapshot (file or device).",
			LAMBDA(void, (const char *path) {
				spath = path;
			})),
		   M0_FORMATARG('S', "Snapshot size", "%"SCNi64, &s.s_size),
		   M0_FLAGARG('b', "Scan every byte (10x slower).", &s.s_byte),
		   M0_FLAGARG('U', "Run unit tests.", &ut),
		   M0_FLAGARG('n', "Dry Run.", &dry_run),
		   M0_FLAGARG('V', "Version info.", &version),
		   M0_STRINGARG('a', "stob domain path",
			   LAMBDA(void, (const char *s) {
				   b.b_stob_path = s;
				   })),
		   M0_STRINGARG('d', "segment stob domain path path",
			LAMBDA(void, (const char *s) {
				b.b_dom_path = s;
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

	if (b.b_dom_path == NULL && !dry_run)
		errx(EX_USAGE, "Specify domain path (-d).");
	if (spath != NULL) {
		sfd = open(spath, O_RDONLY);
		if (sfd == -1)
			err(EX_NOINPUT, "Cannot open snapshot \"%s\".", spath);
	}
	s.s_file = fdopen(sfd, "r");
	if (s.s_file == NULL)
		err(EX_NOINPUT, "Cannot open snapshot.");
	if (s.s_size == 0) {
		result = fseek(s.s_file, 0, SEEK_END);
		if (result != 0)
			err(EX_NOINPUT, "Cannot seek snapshot to the end.");
		b.b_size = s.s_size = ftello(s.s_file);
		if (s.s_size == -1)
			err(EX_NOINPUT, "Cannot tell snapshot size.");
		result = fseek(s.s_file, 0, SEEK_SET);
		if (result != 0)
		      err(EX_NOINPUT, "Cannot seek snapshot to the beginning.");
		printf("Snapshot size: %"PRId64".\n", s.s_size);
	}
	/*
	 * Skip builder related calls for dry run as we will not be building a
	 * new segment.
	 */
	if (!dry_run) {
		qinit(&q, MAX_QUEUED);
		s.s_q = b.b_q = &q;
		result = builder_init(&b);
		s.s_seg = b.b_seg;
		m0_be_engine_tx_size_max(&b.b_dom->bd_engine, &max, NULL);
		s.s_max_reg_size = max.tc_reg_size;
		if (result != 0)
			err(EX_CONFIG, "Cannot initialise builder.");
	}
	result = scanner_init(&s);
	if (result != 0)
		err(EX_CONFIG, "Cannot initialise scanner.");
	if (dry_run) {
		printf("Press CTRL+C to quit.\n");
		signal(SIGINT, sig_handler);
	}
	result = scan(&s);
	if (result != 0)
		warn("Scan failed: %d.", result);

	if (!dry_run) {
		qput(&q, builder_action(&b, sizeof(struct action), AO_DONE,
					&done_ops));
		builder_fini(&b);
		qfini(&q);
	}
	fini();
	if (spath != NULL)
		close(sfd);
	m0_fini();
	return EX_OK;
}

static char iobuf[4*1024*1024];

enum { DELTA = 60 };

static void generation_print(uint64_t gen)
{
	time_t    ts = m0_time_seconds(gen);
	struct tm tm;

	localtime_r(&ts, &tm);
	printf("%04d-%02d-%02d-%02d:%02d:%02d.%09lu",
	       tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
	       tm.tm_hour, tm.tm_min, tm.tm_sec,
	       m0_time_nanoseconds(gen));
}

static int scanner_init(struct scanner *s)
{
	int rc;

	/** Initialising segment header generation identifier explicitly. */
	s->s_gen = 0;
	rc = getat(s, 0, s->s_chunk, sizeof s->s_chunk);
	if (rc != 0)
		M0_LOG(M0_FATAL, "Can not read first chunk");
	return rc;
}

static int scan(struct scanner *s)
{
	uint64_t magic;
	int      result;
	int      i;
	time_t   lasttime = time(NULL);
	off_t    lastoff  = s->s_off;

	setvbuf(s->s_file, iobuf, _IOFBF, sizeof iobuf);
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
			       "Completion: %3i%%",
			       (long long)s->s_off,
			       ((double)s->s_off - lastoff) /
			       DELTA / 1024.0 / 1024.0,
			       (int)(s->s_off * 100 / s->s_size));
			lasttime = time(NULL);
			lastoff  = s->s_off;
		}
	}
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
		if (dry_run)
			printf("%25s : %9"PRId64" %9"PRId64" %9"PRId64" "
			       "%9"PRId64" %9"PRId64" %9s %9"PRId64"\n",
			       bname(&bt[i]),
			       s->c_tree, s->c_node, s->c_leaf, s->c_maxlevel,
			       s->c_kv, "NA*", s->c_fanout);
		else
			printf("%25s : %9"PRId64" %9"PRId64" %9"PRId64" "
			       "%9"PRId64" %9"PRId64" %9"PRId64" %9"PRId64"\n",
			       bname(&bt[i]),
			       s->c_tree, s->c_node, s->c_leaf, s->c_maxlevel,
			       s->c_kv, s->c_kv_bad, s->c_fanout);
	}
	if (dry_run)
		printf("\n*bad kv count is not availabe in dry run mode\n");

	printf("\ngenerations\n");
	for (i = 0; i < gen_count; ++i) {
		generation_print(g[i].g_gen);
		printf(" : %9"PRId64"\n", g[i].g_count);
	}
	return feof(s->s_file) ? 0 : result;
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
		lastoff = s->s_off;
		s->s_off -= sizeof hdr;
		result = recdo(s, &tag, r);
		if (result != 0)
			s->s_off = lastoff;
	} else {
		M0_LOG(M0_FATAL, "Cannot read hdr->hd_bits.");
		FLOG(M0_FATAL, s);
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
			result = m0_format_footer_verify(buf, false);
			if (result != 0) {
				RLOG(M0_DEBUG, "С", s, r, tag);
				FLOG(M0_DEBUG, s);
				r->r_stats.s_chksum++;
				if (!dry_run && r->r_ops != NULL &&
				    r->r_ops->ro_check != NULL)
					result = r->r_ops->ro_check(s, r, buf);
			} else {
				RLOG(M0_DEBUG, "R", s, r, tag);
				if (r->r_ops != NULL &&
				    r->r_ops->ro_proc != NULL)
					result = r->r_ops->ro_proc(s, r, buf);
			}
		} else {
			RLOG(M0_DEBUG, "V", s, r, tag);
			FLOG(M0_DEBUG, s);
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
		FLOG(M0_FATAL, s);
		s->s_pos = -1;
	}
	return M0_RC(result);
}

static int deref(struct scanner *s, const void *addr, void *buf, size_t nob)
{
	off_t off = addr - s->s_seg->bs_addr;

	if (m0_be_seg_contains(s->s_seg, addr) &&
	    m0_be_seg_contains(s->s_seg, addr + nob - 1)) {
		if (off >= s->s_chunk_pos &&
		    off + nob < s->s_chunk_pos + sizeof s->s_chunk) {
			memcpy(buf, &s->s_chunk[off - s->s_chunk_pos],
			       nob);
			return 0;
		} else
			return getat(s, off, buf, nob);
	} else
		return M0_ERR(-EFAULT);
}

static int get(struct scanner *s, void *buf, size_t nob)
{
	int result = 0;
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
	b = &bt[idx];
	b->b_stats.c_tree++;
	return 0;
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

	if (!IS_IN_ARRAY(idx, bt) || bt[idx].b_type == 0)
		idx = ARRAY_SIZE(bt) - 1;
	genadd(node->bt_backlink.bli_gen);
	if (s->s_gen != 0 && s->s_gen != node->bt_backlink.bli_gen)
		return 0;
	b = &bt[idx];
	c = &b->b_stats;
	c->c_node++;
	if (!dry_run && b->b_proc != NULL)
		b->b_proc(s, b, node);
	c->c_kv += node->bt_num_active_key;
	if (node->bt_isleaf) {
		c->c_leaf++;
	} else
		c->c_fanout += node->bt_num_active_key + 1;
	c->c_maxlevel = max64(c->c_maxlevel, node->bt_level);
	return 0;
}

static struct m0_stob_ad_domain *emap_dom_find(const struct action *act,
					       const struct m0_fid *emap_fid)
{
	struct m0_stob_ad_domain *adom;
	int			  i;

	for (i = 0; i < act->a_builder->b_ad_dom_count; i++) {
		adom = act->a_builder->b_ad_domain[i];
		if (m0_fid_eq(emap_fid,
	            &adom->sad_adata.em_mapping.bb_backlink.bli_fid)) {
			break;
		}
	}
	return (i == act->a_builder->b_ad_dom_count) ? NULL: adom;
}

static const struct action_ops emap_ops = {
	.o_prep = &emap_prep,
	.o_act  = &emap_act,
	.o_fini = &emap_fini
};

static int emap_proc(struct scanner *s, struct btype *b,
		     struct m0_be_bnode *node)
{
	struct emap_action   *emap_act;
	int 		      i;
	int 		      ret = 0;

	for (i = 0; i < node->bt_num_active_key; i++) {
		emap_act = action_alloc(sizeof *emap_act,
					AO_INIT,
					&emap_ops);
		emap_act->emap_fid = node->bt_backlink.bli_fid;
		emap_act->emap_key = M0_BUF_INIT_PTR(&emap_act->emap_key_data);
		emap_act->emap_val = M0_BUF_INIT_PTR(&emap_act->emap_val_data);

		ret = emap_kv_get(s, &node->bt_kv_arr[i],
				  &emap_act->emap_key, &emap_act->emap_val);
		if (ret != 0) {
			btree_bad_kv_count_update(node->bt_backlink.bli_type, 1);
			m0_free(emap_act);
			continue;
		}

		qput(s->s_q, &emap_act->emap_act);
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

	adom = emap_dom_find(act, &emap_ac->emap_fid);
	if (adom == NULL) {
		M0_LOG(M0_ERROR, "Invalid FID for emap record found !!!");
		m0_free(act);
		return M0_RC(-EINVAL);
	}

	emap_val = emap_ac->emap_val.b_addr;
	if (emap_val->er_value != AET_HOLE) {
		adom->sad_ballroom->ab_ops->bo_alloc_credit(adom->sad_ballroom,
							    1, credit);
		emap_key = emap_ac->emap_key.b_addr;
		rc = emap_entry_lookup(adom, emap_key->ek_prefix, 0, &it);
		if ( rc == 0 )
			m0_be_emap_close(&it);
		else
			m0_be_emap_credit(&adom->sad_adata,
					  M0_BEO_INSERT, 1, credit);
		m0_be_emap_credit(&adom->sad_adata, M0_BEO_PASTE,
				  BALLOC_FRAGS_MAX + 1, credit);
	}
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

	adom = emap_dom_find(act, &emap_ac->emap_fid);
	emap_val = emap_ac->emap_val.b_addr;
	if (emap_val->er_value != AET_HOLE) {
		emap_key = emap_ac->emap_key.b_addr;
		ext.e_start = emap_val->er_start >> adom->sad_babshift;
		ext.e_end =  emap_key->ek_offset >> adom->sad_babshift;
		m0_ext_init(&ext);

		rc = adom->sad_ballroom->ab_ops->
			bo_reserve_extent(adom->sad_ballroom,
					  tx, &ext,
					  M0_BALLOC_NORMAL_ZONE);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "Failed to reseve extent rc=%d", rc);
			return;
		}

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

static void *action_alloc(size_t len, enum action_opcode opc,
			  const struct action_ops *ops)
{
	struct action     *act;
	M0_PRE(len >= sizeof *act);

	act = m0_alloc(len);
	M0_ASSERT(act != NULL);
	act->a_opc = opc;
	act->a_ops = ops;
	return act;
}

static int seghdr(struct scanner *s, struct rectype *r, char *buf)
{
	struct m0_be_seg_hdr *h   = (void *)buf;

	if (s->s_gen == 0) {
		s->s_gen = h->bh_gen;
		printf("\nSource segment header generation\n");
	} else {
		genadd(h->bh_gen);
		printf("\nFound another segment header generation\n");
	}
	generation_print(h->bh_gen);

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

static void genadd(uint64_t gen)
{
	int i;

	if (gen_count >= ARRAY_SIZE(g))
		return;

	for (i = 0; i < gen_count; ++i) {
		if (g[i].g_gen == gen) {
			g[i].g_count++;
			return;
		}
	}
	g[gen_count].g_gen = gen;
	g[gen_count].g_count = 1;
	gen_count++;
}

static void builder_process(struct builder *b)
{
	struct action      *act;
	struct m0_be_tx     tx = {};
	struct m0_sm_group *grp = m0_locality0_get()->lo_grp;
	int                 result;

	m0_sm_group_lock(grp);
	m0_be_tx_init(&tx, 0, b->b_dom, grp, NULL, NULL, NULL, NULL);
	m0_be_tx_prep(&tx, &b->b_cred);
	result = m0_be_tx_open_sync(&tx);
	M0_ASSERT(result == 0); /* Anything else we can do? */
	while ((act = qtry(&b->b_qq)) != NULL) {
		act->a_ops->o_act(act, &tx);
		act->a_ops->o_fini(act);
		m0_free(act);
	}
	m0_be_tx_close_sync(&tx);
	m0_be_tx_fini(&tx);
	m0_sm_group_unlock(grp);
	b->b_cred = M0_BE_TX_CREDIT(0, 0);
	b->b_tx++;
}

static void builder_thread(struct builder *b)
{
	struct m0_be_tx_credit delta = {};
	struct action         *act;
	int		       ret;

	do {
		delta = M0_BE_TX_CREDIT(0, 0);
		act = qget(b->b_q);
		act->a_builder = b;
		ret = act->a_ops->o_prep(act, &delta);
		// if o_prep() returns non-zero status, move to next record
		if (ret != 0)
			continue;
		if (m0_be_should_break(&b->b_dom->bd_engine,
				       &b->b_cred, &delta) ||
		    act->a_opc == AO_DONE) {
			builder_process(b);
		}
		if (act->a_opc != AO_DONE) {
			m0_be_tx_credit_add(&b->b_cred, &delta);
			qput(&b->b_qq, act);
			b->b_act++;
		}
	} while (act->a_opc != AO_DONE);
	M0_ASSERT(b->b_qq.q_nr == 0);

	/* Below clean up used as m0_be_ut_backend_fini()  fails because of
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
	char 			 *str_cfg_init = "directio=false";
	struct ad_dom_info       *adom_info;
	uint64_t		  ad_dom_count;

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
	generation_print(b->b_seg->bs_gen);

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

	result = ad_dom_init(b);
	if (result != 0)
		return M0_ERR(result);
	qinit(&b->b_qq, UINT64_MAX);
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
	m0_thread_join(&b->b_thread);
	m0_thread_fini(&b->b_thread);
	qfini(&b->b_qq);
	m0_ctg_store_fini();
	m0_reqh_be_fini(&b->b_reqh);
	ad_dom_fini(b);
	m0_be_ut_backend_fini(&b->b_backend);
	m0_reqh_fini(&b->b_reqh);
	m0_free(b->b_backend.but_stob_domain_location);

	printf("builder: actions: %9"PRId64" txs: %9"PRId64"\n",
	       b->b_act, b->b_tx);
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
	M0_LOG(M0_DEBUG, "Discarded kv = %d from btree = %lu", count, type);
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
	bool			     ismeta;

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
						   &vl[n.bt_num_active_key])) {
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
		} else
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
	if (slot == NULL) {
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
	}
	return slot;
}

static int ctg_prep(struct action *act, struct m0_be_tx_credit *cred)
{
	struct ctg_action      *ca    = M0_AMB(ca, act, cta_act);
	struct m0_be_btree      tree  = {};
	struct m0_be_tx_credit  accum = {};

	ca->cta_slot = ctg_getslot_insertcred(ca, act->a_builder,
					      &ca->cta_fid, &accum);
	if (!ca->cta_ismeta)
		m0_be_btree_insert_credit(&tree, 1,
					  ca->cta_key.b_nob,
					  ca->cta_val.b_nob,
					  &accum);
	*cred = accum;
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
	.ro_proc = &btree
};

static const struct recops bnodeops = {
	.ro_proc = &bnode
};

static const struct recops seghdrops = {
	.ro_proc = &seghdr,
	.ro_ver  = &seghdr_ver
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

	M0_PRE(bb->bli_type == M0_BBT_COB_NAMESPACE);

	for (i = 0; i < node->bt_num_active_key; i++) {

		ca = scanner_action(sizeof *ca, AO_INIT, &cob_ops);
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
		    (m0_format_footer_verify(ca->coa_valdata, false) == 0))
			qput(s->s_q, (struct action *)ca);
		else {
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
static int cob_prep(struct action *act, struct m0_be_tx_credit *cred)
{
	struct m0_be_tx_credit  accum = {};
	struct cob_action      *ca = container_of(act, struct cob_action,
						  coa_act);
	struct m0_cob_nsrec    *nsrec = ca->coa_val.b_addr;
	struct m0_stob_id       stob_id;

 	if (m0_fid_validate_cob(&nsrec->cnr_fid)) {
		m0_fid_convert_cob2adstob(&nsrec->cnr_fid, &stob_id);
		m0_cc_stob_cr_credit(&stob_id, &accum);
	}
	m0_cob_tx_credit(act->a_builder->b_ios_cdom, M0_COB_OP_NAME_ADD,
			 &accum);
	m0_cob_tx_credit(act->a_builder->b_ios_cdom, M0_COB_OP_NAME_DEL,
			 &accum);
	*cred = accum;
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
	}
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
