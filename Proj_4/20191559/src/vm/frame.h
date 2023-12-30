#ifndef FILE_H
#define FILE_H
#include <threads/palloc.h>
#include "page.h"

struct page* page_allocation(enum palloc_flags flags);  // Allocate a page of memory to be used as a user page

bool page_emplace_LRU(struct page *new_page);           // Add a page to the LRU list
bool page_out_LRU(struct page *target_page);            // Remove a page from the LRU list

void free_and_remove_page(void *page_kernel_addr);      // Free a page and remove it from the LRU list
struct list_elem* rotate_lru_pointer();                 // Rotate the LRU clock pointer
void evict_pages_from_lru();                            // Evict pages from the LRU list until a page can be allocated

bool should_evict(struct page *page);                   // Determine if a page should be evicted
void evict_page(struct page *page);                     // Evict a page from physical memory
void advance_lru_clock();                               // Advance the LRU clock pointer

void init_LRU(void);                                    // Initialize the LRU list and clock pointer
#endif 