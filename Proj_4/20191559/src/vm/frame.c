#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include <threads/malloc.h>
#include "threads/synch.h"
#include "threads/thread.h"
#include "filesys/file.h"

#include "vm/frame.h"
#include "vm/swap.h"

#define FOR(i, n) for(int i=0; i<n; i++)
#define FOR1(i, n) for(int i=1; i<=n; i++)
#define FOR_RANGE(i, start, end) for(int i = start; i < end; i++)
#define FOR_LIST(e, list) \
    for ((e) = list_begin(list); (e) != list_end(list); (e) = list_next(e))


void *try_alloc_physical_memory(enum palloc_flags flags) {
  void *page_kernel_addr;
  do {
    page_kernel_addr = palloc_get_page(flags);
    if (!page_kernel_addr) evict_pages_from_lru();
  } while (!page_kernel_addr);
  return page_kernel_addr;
}

struct page *page_allocation(enum palloc_flags flags) {
  if (flags & PAL_USER) {
    void *page_kernel_addr = try_alloc_physical_memory(flags);
    struct page *page_new_addr = malloc(sizeof(struct page));

    if (!page_new_addr) {
        palloc_free_page(page_kernel_addr);
        return NULL;
    }  
    page_new_addr->kernel_addr = page_kernel_addr;
    page_new_addr->owner_thread = thread_current();
    page_emplace_LRU(page_new_addr);
    
    return page_new_addr;
  }

  return NULL;
}

bool page_emplace_LRU(struct page *new_page) {
  if(!new_page) return false;

  lock_acquire(&lru_lock);
  list_push_back(&lru_list, &new_page->lru);
  lock_release(&lru_lock);
  
  return true;
}

bool page_out_LRU(struct page* target_page) {
  if (!target_page) return false;

  bool lru_clock_updated = false;
  bool clock_ended = (lru_clock == target_page);
  struct list_elem* e = list_remove(&target_page->lru);

  // lock_acquire(&lru_lock);
  
  if (clock_ended) {
      lru_clock = list_entry(e, struct page, lru);
      lru_clock_updated = true;
      // lock_release(&lru_lock);
      return lru_clock_updated;
  } 
  // lock_release(&lru_lock);

  return lru_clock_updated;
}

void free_and_remove_page (void *page_kernel_addr) {
  if (!page_kernel_addr) return; // Early exit if the address is NULL
  lock_acquire(&lru_lock);  
  struct list_elem *e;
  
  FOR_LIST(e, &lru_list) {
  	struct page* lru_page = list_entry(e, struct page, lru);
  	if(page_kernel_addr == lru_page->kernel_addr){
  	  palloc_free_page(lru_page->kernel_addr);
  	  page_out_LRU(lru_page);
  	  free(lru_page);
  	  break;
  	}
  }
  lock_release(&lru_lock);
}

struct list_elem* rotate_lru_pointer() {
  // Early exit if the list is empty
  if (list_empty(&lru_list)) return NULL;
  
  struct list_elem *next_elem;
  // If lru_clock is NULL, start from the beginning of the list
  if (!lru_clock) {
      next_elem = list_begin(&lru_list);
  } else {
    // Otherwise, get the next element in the list
    next_elem = list_next(&lru_clock->lru);
  	if (next_elem == list_end(&lru_list)) next_elem = list_begin(&lru_list);
      
  }
  // Update lru_clock to the next element
  lru_clock = list_entry(next_elem, struct page, lru);
  return next_elem;
}

void handle_dirty_page(struct page *lru_page, bool dirty) {
  if (lru_page->vme->type || dirty) {
    lru_page->vme->type = true;
    lru_page->vme->swap_index = write_to_swap(lru_page->kernel_addr);
  }
}

void evict_pages_from_lru() {
  lock_acquire(&lru_lock);
  if (list_empty(&lru_list)) {
    lock_release(&lru_lock);
    return;
  }

  struct list_elem *e = rotate_lru_pointer();
  while (e && e != list_end(&lru_list)) {
    struct page *lru_page = list_entry(e, struct page, lru);
    struct thread *page_thread = lru_page->owner_thread;
    bool accessed = pagedir_is_accessed(page_thread->pagedir, lru_page->vme->vaddr);
    bool dirty = pagedir_is_dirty(page_thread->pagedir, lru_page->vme->vaddr);  
    if (!accessed) {
      handle_dirty_page(lru_page, dirty);

      lru_page->vme->is_in_memory = false;
      pagedir_clear_page(page_thread->pagedir, lru_page->vme->vaddr);
	    palloc_free_page(lru_page->kernel_addr);
	    page_out_LRU(lru_page);
	    free(lru_page);  
      break;
    }  
    pagedir_set_accessed(page_thread->pagedir, lru_page->vme->vaddr, false);
    e = rotate_lru_pointer();
  }
  lock_release(&lru_lock);
}

void init_LRU () {
  list_init(&lru_list);
  lock_init(&lru_lock);
  lru_clock = NULL;
}