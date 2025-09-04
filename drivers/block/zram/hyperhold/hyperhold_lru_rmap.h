// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022-2023 HMD. All rights reserved.
 */

#ifndef _HYPERHOLD_LRU_MAP_
#define _HYPERHOLD_LRU_MAP_

#define EXTENT_MAX_OBJ_CNT (30 * EXTENT_PG_CNT)

void zram_set_memcg(struct zram *zram, u32 index, struct mem_cgroup *mcg);
struct mem_cgroup *zram_get_memcg(struct zram *zram, u32 index);
int zram_get_memcg_coldest_index(struct hyperhold_area *area,
				 struct mem_cgroup *mcg,
				 int *index, int max_cnt);
int zram_rmap_get_extent_index(struct hyperhold_area *area,
			       int ext_id, int *index);
void zram_lru_add(struct zram *zram, u32 index, struct mem_cgroup *mcg);
void zram_lru_add_tail(struct zram *zram, u32 index, struct mem_cgroup *mcg);
void zram_lru_del(struct zram *zram, u32 index);
void zram_rmap_insert(struct zram *zram, u32 index);
void zram_rmap_erase(struct zram *zram, u32 index);

#endif
