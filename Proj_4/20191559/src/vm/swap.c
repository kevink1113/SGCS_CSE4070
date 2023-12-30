#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bitmap.h"
#include "threads/synch.h"
#include "devices/block.h"

#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

#define FOR(i, n) for(int i=0; i<n; i++)
#define FOR1(i, n) for(int i=1; i<=n; i++)
#define FOR_RANGE(i, start, end) for(int i = start; i < end; i++)
#define FOR_LIST(e, list) \
    for ((e) = list_begin(list); (e) != list_end(list); (e) = list_next(e))

struct block *block_swp;
void handle_block_io(bool is_read, size_t swap_index, void *physical_addr) {
  FOR(i, 8) {
    size_t sector_idx = swap_index * 8 + i;
    uint8_t *sector_addr = (uint8_t *)physical_addr + i * BLOCK_SECTOR_SIZE;
    if (is_read) block_read(block_swp, sector_idx, sector_addr);
    else block_write(block_swp, sector_idx, sector_addr);
  }
}

struct lock lock_swp;
struct bitmap *map;
void read_from_swap(size_t swap_index, void *physical_addr) {
  lock_acquire(&lock_swp);
  
  if (bitmap_test(map, swap_index)) {
      handle_block_io(true, swap_index, physical_addr);
      bitmap_flip(map, swap_index);
  }
  
  lock_release(&lock_swp);
}

size_t write_to_swap(void *physical_addr) {
  lock_acquire(&lock_swp);

  size_t swap_index = bitmap_scan_and_flip(map, 0, 1, false);
  if (swap_index != BITMAP_ERROR) handle_block_io(false, swap_index, physical_addr);
  
  lock_release(&lock_swp);
  
  return swap_index;
}

void initialize_swap() {
  block_swp = block_get_role(BLOCK_SWAP);
  if (!block_swp) return;

  size_t swap_size = block_size(block_swp) / 8;
  map = bitmap_create(swap_size);
  if (!map) return;

  bitmap_set_all(map, 0);
  lock_init(&lock_swp);
}