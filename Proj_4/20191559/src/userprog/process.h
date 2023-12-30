#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#define MAX_STACK_SIZE (1 << 23)                                            // 8MB

bool memory_fault_handler(struct virtual_page_entr *vme);                   // Handle a memory fault
bool load_page_content(struct page *page, struct virtual_page_entr *vme);   // Load the content of a page
bool install_loaded_page(struct virtual_page_entr *vme, struct page *page); // Install a loaded page

tid_t process_execute (const char *file_name);                              // Execute a process
int process_wait (tid_t);                                                   // Wait for a process to finish
void process_exit (void);                                                   // Exit the current process
void process_activate (void);                                               // Activate a new process


bool is_valid_stack_expansion(void *addr);
struct virtual_page_entry *create_virtual_page_entry(void *addr);
struct page *allocate_and_setup_page(struct virtual_page_entr *vme);
bool add_page_to_process_vm(struct virtual_page_entr *vme, struct page *stack_page);

bool expand_stack(void *addr);                                              // Expand the stack
static bool install_page (void *upage, void *kpage, bool writable);

#endif /* userprog/process.h */
