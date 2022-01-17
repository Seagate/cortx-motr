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


#pragma once

#ifndef __MOTR_HELPERS_UFID_H__
#define __MOTR_HELPERS_UFID_H__

#include "motr/client.h"

/**
 * UFID - Unique FID Generator
 * UFID generates a unique ID of 128 bits across multiple client instances.
 * Notice: an UFID generator has to associate with one client instance as
 * UFID generator required unique process ID which is extracted from the
 * lowest 29 bits of client instance's process FID. Halon guarantes that
 * the lowest 29 bits of process FID are unique for each client instance.
 *
 * FID Format:
 * -----------
 *
 * 127            91|90   80|79           49|48           20|19           0
 *  +---------------+-------+---------------+---------------+-------------+
 *  |   Motr        | Salt  | Generation ID | Process ID    | Sequence ID |
 *  +---------------+-------+---------------+---------------+-------------+
 *  |  37 Bits      |11 Bits|   31 Bits     |    29 Bits    |     20 Bit  |
 *  +---------------+-------+---------------+---------------+-------------+
 *
 * Notes:
 * (1) Salt: 11 bits of random number which is set when UFID is initialised.
 * (2) Generation ID: it is taken from timestep (epoch) represented with 31
 *     bits. A new generation ID is set when UFID is initialised or Sequence
 *     ID rolls over 20 bits.
 * (3) Process ID: client instance process ID. Each client instance is assigned
 *     a cluster-wide unique FID by Halon and the lowest 29 bits of FID is used
 *     as process ID. As FID is 128 bit long and process ID uses only 29 bits,
 *     Halon must guarantee that all process IDs extracted from 128 bit FIDs
 *     are unique.
 * (4) Sequence ID: simple sequence number generated with the client instance.
 *
 *
 * UFID APIs
 * --------------------------
 *
 * UFID provides with the following APIs, see API specifications in
 * helpers/helpers.h for details.
 *
 * - int m0_ufid_init(struct m0_client *m0c)
 * - void m0_ufid_fini(struct m0_client *m0c)
 * - int m0_ufid_new(struct m0_client *m0c, uint32_t nr_seqs,
 *                   uint32_t nr_skip_seqs, m0_uint128 *id)
 * - int m0_ufid_next(struct m0_client *m0c, uint32_t nr_seqs,
 *                    m0_uint128 *id)
 *
 *
 * UFID Interface usage example
 * ---------------------------
 *
 * The following sample code shows how UFID generator is initialised and
 * finalised, particularly how it works with client, and 2 examples on
 * how to call m0_ufid_new() and m0_ufid_next(). Example 1 demonstrates
 * using m0_ufid_next() to get the next available FID, while example 2
 * shows how to get multiple FIDs in one m0_ufid_new() call.
 *
 * @code
 * #include <stdio.h>
 * #include "helpers/helpers.h"
 *
 * static struct m0_client *m0c;
 * static struct m0_config m0c_conf;
 * static struct m0_ufid_generator ufid_gr;
 *
 * static void example1(struct m0_client *m0c)
 * {
 *     int rc;
 *     struct m0_uint128 id128 = M0_UINT128(0, 0);
 *
 *     rc = m0_ufid_next(&ufid_gr, 1, &id128);
 *     if (rc < 0)
 *          printf("m0_ufid_next() returned error: %d", rc);
 * }
 *
 * static void example2()
 * {
 *     int rc;
 *     struct m0_uint128 id128 = M0_UINT128(0, 0);
 *
 *     rc = m0_ufid_new(&ufid_gr, 100, 100, &id128);
 *     if (rc < 0)
 *          printf("m0_ufid_new() returned error: %d", rc);
 * }
 *
 * int main()
 * {
 *     int rc;
 *
 *     rc = m0_client_init(&m0c, &m0c_conf, true)?:
 *          m0_ufid_init(m0c, &ufid_gr);
 *     if (rc != 0)
 *         return rc;
 *
 *     example1(m0c);
 *     example2(m0c);
 *
 *     m0_ufid_fini(ufid_gr);
 *     m0_client_fini(m0c, true);
 *     return 0;
 * }
 * @endcode
 */

enum {
	/**
	 * Epoch value as on 2018-01-01 00:00:00
 	 * CAUTION: Don't change this constant, ever.
 	 *
 	 * Changing this constant may lead to conflict FIDs allocated as
 	 * the generation ID is calculated based on this constant.
	 */
	M0_UFID_BASE_TS          = 1514764800L,

	/** Max attempts before general failure. */
	M0_UFID_RETRY_MAX        = 1000L,

	/** Max allowable clock skew in seconds. */
	M0_UFID_CLOCK_SKEW_LIMIT = 300
};

enum m0_ufid_format {
	M0_UFID_SEQID_BITS       = 20UL,
	M0_UFID_PROCID_BITS      = 29UL,
	M0_UFID_GENID_BITS       = 31UL,
	M0_UFID_SALT_BITS        = 11UL,
	M0_UFID_RESERVED_BITS    = 37UL,
	M0_UFID_MAX_BITS         = 91UL
};

enum m0_ufid_gen_id_format {
	M0_UFID_GENID_HI_BITS    = 16UL,
	M0_UFID_GENID_LO_BITS    = 15UL
};

/**
 * M0_UFID_PROCID_SAFE_BITS ==
 *    M0_UFID_PROCID_BITS - M0_UFID_PROCID_WARN_BITS
 */
enum m0_ufid_procid_format {
	M0_UFID_PROCID_WARN_BITS = 1UL,
	M0_UFID_PROCID_SAFE_BITS = 28UL
};

#define UFID_MASK(N)          ((1UL << N) - 1UL)
#define M0_UFID_GENID_MASK    UFID_MASK(M0_UFID_GENID_BITS)
#define M0_UFID_GENID_HI_MASK UFID_MASK(M0_UFID_GENID_HI_BITS)
#define M0_UFID_GENID_LO_MASK UFID_MASK(M0_UFID_GENID_LO_BITS)
#define M0_UFID_SALT_MASK     UFID_MASK(M0_UFID_SALT_BITS)
#define M0_UFID_SEQID_MASK    UFID_MASK(M0_UFID_SEQID_BITS)
#define M0_UFID_PROCID_MASK   UFID_MASK(M0_UFID_PROCID_BITS)
#define M0_UFID_RESERVED_MASK UFID_MASK(M0_UFID_RESERVED_BITS)

#define M0_UFID_HI_RESERVED \
	(UINT64_MAX << (64UL - M0_UFID_RESERVED_BITS))
#define M0_UFID_SEQID_MAX     (1UL << M0_UFID_SEQID_BITS)
#define M0_UFID_PROCID_MAX    (1UL << M0_UFID_PROCID_BITS)

struct m0_ufid {
	uint64_t uf_reserved;   /* 37 bits reserver by MOTR */
	uint32_t uf_salt;       /* 11 bit random salt */
	uint32_t uf_gen_id;     /* 31 bit generation timestamp */
	uint32_t uf_proc_id;    /* 29 bit service uid */
	uint32_t uf_seq_id;     /* 20 bit Sequence No */
};

/**
 * Data structure for unique FID generator which maintains the current
 * available ufid.
 */
struct m0_ufid_generator {
	/**
	 * Back pointer to the client instance this generator is
	 * associated with.
	 */
	struct m0_client     *ufg_m0c;
	bool                  ufg_is_initialised;
	/**
	 * Generation ID and sequence ID can be modified in one
	 * single FID request, so a lock is used here.
	 */
	struct m0_mutex       ufg_lock;
	struct m0_ufid        ufg_ufid_cur;
};

#endif /* __MOTR_HELPERS_UFID_H__ */

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
