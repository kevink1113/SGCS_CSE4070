#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include <threads/malloc.h>
#include <threads/palloc.h>
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "lib/kernel/list.h"
#include "filesys/file.h"

#include "vm/page.h"
#include "vm/frame.h"

struct virtual_page_entr *get_virtual_page_entr_by_vaddr(void *virtual_address) {
  struct virtual_page_entr search_entry;
  search_entry.vaddr = pg_round_down(virtual_address);
  struct hash_elem *found_entry = hash_find(&thread_current()->vm, &search_entry.elem);
  return found_entry ? hash_entry(found_entry, struct virtual_page_entr, elem) : NULL;
}

bool read_file_into_memory(void *kernel_addr, struct virtual_page_entr *virtual_page_entr) {
  size_t bytes_read = file_read_at(virtual_page_entr->backing_file, kernel_addr, virtual_page_entr->read_bytes, virtual_page_entr->file_offset);
  if (bytes_read == virtual_page_entr->read_bytes) {
      memset(kernel_addr + virtual_page_entr->read_bytes, 0, virtual_page_entr->zero_bytes);
      return true;
  }
  return false;
}

bool add_virtual_page_entr(struct hash *vm_table, struct virtual_page_entr *virtual_page_entr) {
  return hash_insert(vm_table, &virtual_page_entr->elem) == NULL;
}

bool remove_virtual_page_entr(struct hash *vm_table, struct virtual_page_entr *virtual_page_entr) {
  bool is_removed = hash_delete(vm_table, &virtual_page_entr->elem) != NULL;
  free(virtual_page_entr);
  return is_removed;
}