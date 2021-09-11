/* -*- C -*- */
/*
 * Copyright (c) 2012-2021 Seagate Technology LLC and/or its Affiliates
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


#include "lib/bitmap.h"
#include "lib/misc.h"   /* M0_SET0 */
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/memory.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

/**
   @defgroup bitmap Bitmap
   @{
 */

/**
   Number of bits in a word (m0_bitmap.b_words).
   And the number of bits to shift to convert bit offset to word index.
 */
enum {
	M0_BITMAP_BITS = (8 * sizeof ((struct m0_bitmap *)0)->b_words[0]),
	M0_BITMAP_BITSHIFT = 6
};

/*
  Note that the following assertion validates both the relationship between
  M0_BITMAP_BITS and M0_BITMAP_BITSHIFT, and that M0_BITMAP_BITS is
  a power of 2.
*/
M0_BASSERT(M0_BITMAP_BITS == (1UL << M0_BITMAP_BITSHIFT));

/**
   Number of words needed to be allocated to store nr bits.  This is
   an allocation macro.  For indexing, M0_BITMAP_SHIFT is used.

   @param nr number of bits to be allocated
 */
#define M0_BITMAP_WORDS(nr) (((nr) + (M0_BITMAP_BITS-1)) >> M0_BITMAP_BITSHIFT)

/* verify the bounds of size macro */
M0_BASSERT(M0_BITMAP_WORDS(0) == 0);
M0_BASSERT(M0_BITMAP_WORDS(1) == 1);
M0_BASSERT(M0_BITMAP_WORDS(63) == 1);
M0_BASSERT(M0_BITMAP_WORDS(64) == 1);
M0_BASSERT(M0_BITMAP_WORDS(65) == 2);

/**
   Shift a m0_bitmap bit index to get the word index.
   Use M0_BITMAP_SHIFT() to select the correct word, then use M0_BITMAP_MASK()
   to access the individual bit within that word.

   @param idx bit offset into the bitmap
 */
#define M0_BITMAP_SHIFT(idx) ((idx) >> M0_BITMAP_BITSHIFT)

/**
   Mask off a single bit within a word.
   Use M0_BITMAP_SHIFT() to select the correct word, then use M0_BITMAP_MASK()
   to access the individual bit within that word.

   @param idx bit offset into the bitmap
 */
#define M0_BITMAP_MASK(idx) (1UL << ((idx) & (M0_BITMAP_BITS-1)))

static uint64_t *words(const struct m0_bitmap *map)
{
	M0_CASSERT(sizeof map->b_words[0] == sizeof &map->b_words);
	if (M0_BITMAP_WORDS(map->b_nr) <= 1)
		return (void *)&map->b_words;
	else
		return (void *)map->b_words;
}

M0_INTERNAL int m0_bitmap_init(struct m0_bitmap *map, size_t nr)
{
	int nr_words = M0_BITMAP_WORDS(nr);
	if (nr_words > 1) {
		M0_ALLOC_ARR(map->b_words, nr_words);
		if (map->b_words == NULL)
			return M0_ERR(-ENOMEM);
	} else
		map->b_words = 0;
	map->b_nr = nr;
	return 0;
}
M0_EXPORTED(m0_bitmap_init);

M0_INTERNAL void m0_bitmap_fini(struct m0_bitmap *map)
{
	M0_ASSERT(words(map) != NULL);
	if (M0_BITMAP_WORDS(map->b_nr) > 1)
		m0_free(map->b_words);
	M0_SET0(map);
}
M0_EXPORTED(m0_bitmap_fini);

M0_INTERNAL bool m0_bitmap_get(const struct m0_bitmap *map, size_t idx)
{
	M0_PRE(idx < map->b_nr && words(map) != NULL);
	return words(map)[M0_BITMAP_SHIFT(idx)] & M0_BITMAP_MASK(idx);
}
M0_EXPORTED(m0_bitmap_get);

M0_INTERNAL int m0_bitmap_ffs(const struct m0_bitmap *map)
{
	int i;
	int idx;
	uint64_t *w = words(map);

	for (i = 0; i < M0_BITMAP_WORDS(map->b_nr); i++) {
		idx = __builtin_ffsll(w[i]);
		if (idx > 0)
			return i * M0_BITMAP_BITS + (idx - 1);
	}
	return -1;
}
M0_EXPORTED(m0_bitmap_ffs);

M0_INTERNAL int m0_bitmap_ffz(const struct m0_bitmap *map)
{
	int idx;

	/* use linux find_first_zero_bit() ? */
	for (idx = 0; idx < map->b_nr; idx++) {
		if (!m0_bitmap_get(map, idx))
			return idx;
	}
	return -1;
}
M0_EXPORTED(m0_bitmap_ffz);

M0_INTERNAL void m0_bitmap_set(struct m0_bitmap *map, size_t idx, bool val)
{
	uint64_t *w = words(map);
	M0_ASSERT(idx < map->b_nr && w != NULL);
	if (val)
		w[M0_BITMAP_SHIFT(idx)] |= M0_BITMAP_MASK(idx);
	else
		w[M0_BITMAP_SHIFT(idx)] &= ~M0_BITMAP_MASK(idx);
}
M0_EXPORTED(m0_bitmap_set);

M0_INTERNAL void m0_bitmap_reset(struct m0_bitmap *map)
{
	size_t i;
	uint64_t *w = words(map);

	for (i = 0; i < M0_BITMAP_WORDS(map->b_nr); i++)
		w[i] = 0;
}
M0_EXPORTED(m0_bitmap_reset);

M0_INTERNAL void m0_bitmap_copy(struct m0_bitmap *dst,
				const struct m0_bitmap *src)
{
	int s = M0_BITMAP_WORDS(src->b_nr);
	int d = M0_BITMAP_WORDS(dst->b_nr);
	uint64_t *sw = words(src);
	uint64_t *dw = words(dst);

	M0_PRE(dst->b_nr >= src->b_nr && sw != NULL && dw != NULL);

	memcpy(dw, sw, s * sizeof sw[0]);
	if (d > s)
		memset(&dw[s], 0, (d - s) * sizeof dw[0]);
}

M0_INTERNAL size_t m0_bitmap_set_nr(const struct m0_bitmap *map)
{
	size_t i;
	size_t nr;
	M0_PRE(map != NULL);
	for (nr = 0, i = 0; i < map->b_nr; ++i)
		nr += m0_bitmap_get(map, i);
	return nr;
}

M0_INTERNAL int m0_bitmap_onwire_init(struct m0_bitmap_onwire *ow_map, size_t nr)
{
	ow_map->bo_size = M0_BITMAP_WORDS(nr);
	M0_ALLOC_ARR(ow_map->bo_words, M0_BITMAP_WORDS(nr));
	if (ow_map->bo_words == NULL)
		return M0_ERR(-ENOMEM);

	return 0;
}

M0_INTERNAL void m0_bitmap_onwire_fini(struct m0_bitmap_onwire *ow_map)
{
	M0_PRE(ow_map != NULL);

	m0_free(ow_map->bo_words);
	M0_SET0(ow_map);
}

M0_INTERNAL void m0_bitmap_store(const struct m0_bitmap *im_map,
				 struct m0_bitmap_onwire *ow_map)
{
	size_t s;

	M0_PRE(im_map != NULL && ow_map != NULL);
	M0_PRE(words(im_map) != NULL);
	s = M0_BITMAP_WORDS(im_map->b_nr);
	M0_PRE(s == ow_map->bo_size);

	memcpy(ow_map->bo_words, words(im_map), s * sizeof im_map->b_words[0]);
}

M0_INTERNAL void m0_bitmap_load(const struct m0_bitmap_onwire *ow_map,
				struct m0_bitmap *im_map)
{
	M0_PRE(ow_map != NULL && im_map != NULL);
	M0_PRE(M0_BITMAP_WORDS(im_map->b_nr) == ow_map->bo_size);

	/* copy onwire bitmap words to in-memory bitmap words. */
	memcpy(words(im_map), ow_map->bo_words,
	       ow_map->bo_size * sizeof ow_map->bo_words[0]);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of bitmap group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
