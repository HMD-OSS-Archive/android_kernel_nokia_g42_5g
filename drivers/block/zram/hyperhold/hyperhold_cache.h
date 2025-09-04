// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022-2023 HMD. All rights reserved.
 */


#ifndef _HYPERHOLD_CACHE_H
#define _HYPERHOLD_CACHE_H

#define HYPERHOLD_CACHE_SIZE_MAX (1UL << 20)
#define CACHE_SIZE_HIGH (EXTENT_SIZE * 3 / 4)
#define CACHE_SIZE_LOW (EXTENT_SIZE / 4)

struct extent_cache {
	struct page *pages[EXTENT_PG_CNT];
	int pin;
	struct rcu_head rcu_head;
};

struct hyperhold_cache {
	struct radix_tree_root tree;
	spinlock_t lock;
	int nr_cached_exts;
};

struct extent_cache *new_extent_cache(struct page *pages[EXTENT_PG_CNT],
		bool fast, bool nofail);
void free_extent_cache(struct extent_cache *ext);
struct hyperhold_cache *hyperhold_cache_alloc(int nr_exts);
void hyperhold_cache_clear(struct hyperhold_cache *cache);
void hyperhold_cache_free(struct hyperhold_cache *cache);

bool hyperhold_cache_pin(struct hyperhold_cache *cache, int ext_id);
void hyperhold_cache_unpin(struct hyperhold_cache *cache, int ext_id);
int hyperhold_cache_insert(struct hyperhold_cache *cache, int ext_id,
			struct extent_cache *ext, bool fast, bool nofail);
int hyperhold_cache_delete(struct hyperhold_cache *cache, int ext_id);
bool hyperhold_cache_lookup(struct hyperhold_cache *cache, int ext_id);

struct extent_cache *hyperhold_cache_read_extent(int ext_id);
int hyperhold_cache_read_obj(struct hyperhold_cache *cache,
			unsigned long addr, u8 *dst, int size);
#endif
