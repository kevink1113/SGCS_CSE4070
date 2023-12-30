#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
// Added in #Proj 4
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"

#define FOR(i, n) for(int i=0; i<n; i++)
#define FOR1(i, n) for(int i=1; i<=n; i++)
#define FOR_RANGE(i, start, end) for(int i = start; i < end; i++)
#define FOR_LIST(e, list) \
    for ((e) = list_begin(list); (e) != list_end(list); (e) = list_next(e))


static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

bool install_loaded_page(struct virtual_page_entr *vme, struct page *page) {
    if (!install_page(vme->vaddr, page->kernel_addr, vme->is_writable)) {
        free_and_remove_page(page->kernel_addr);
        return false;
    }

    vme->is_in_memory = true;
    return true;
}

bool expand_stack(void *addr) {
  void *rounded_addr = pg_round_down(addr);
  if (!is_valid_stack_expansion(rounded_addr)) return false;

  struct virtual_page_entry *vme = create_virtual_page_entry(rounded_addr);
  if (!vme) return false;
  struct page *stack_page = allocate_and_setup_page(vme);
  if (!stack_page) {
    free(vme);
    return false;
  }

  return add_page_to_process_vm(vme, stack_page);
}

bool is_valid_stack_expansion(void *addr) {
    return ((size_t)(PHYS_BASE - addr) <= MAX_STACK_SIZE);
}

struct virtual_page_entry *create_virtual_page_entry(void *addr) {
    struct virtual_page_entr *vme = malloc(sizeof(struct virtual_page_entr));
    if (!vme) return NULL;

    vme->is_in_memory = vme->is_writable = true;
    vme->type = false; 
    vme->vaddr = addr;
    return vme;
}

struct page *allocate_and_setup_page(struct virtual_page_entr *vme) {
    struct page *stack_page = page_allocation(PAL_USER);
    if (!stack_page) return NULL;
    
    stack_page->vme = vme;

    if (!install_page(vme->vaddr, stack_page->kernel_addr, vme->is_writable)) {
        free_and_remove_page(stack_page->kernel_addr);
        free(stack_page);
        return NULL;
    }
    return stack_page;
}

bool add_page_to_process_vm(struct virtual_page_entr *vme, struct page *stack_page) {
    if (!add_virtual_page_entr(&thread_current()->vm, vme)) {
        free_and_remove_page(stack_page->kernel_addr);
        free(stack_page);
        free(vme);
        return false;
    }
    return true;
}


/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  // Find command name by finding first space
  char cmd_name[128];
  strlcpy(cmd_name, file_name, 1+strlen(file_name));

  FOR(i, strlen(cmd_name)){ 
    if(cmd_name[i] == ' '){
      cmd_name[i] = '\0';
      break;
    }
  }
  if(filesys_open(cmd_name) == NULL) return TID_ERROR;
  
  
  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (cmd_name, PRI_DEFAULT, start_process, fn_copy);

  struct thread* child = NULL;

  struct thread* t;
  struct list_elem *e;
  FOR_LIST(e, &(thread_current()->child_list)) {
    t = list_entry(e, struct thread, child_elem);
    if(t->tid == tid) {
      child = t;
      break;
    }
  }

  sema_down(&child->load_lock);

  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 
  
  
  if(child->exit_status == -1) return process_wait(tid);

  return tid;
}

static unsigned hash_virtual_page_entr(const struct hash_elem *e, void *aux UNUSED) {
	struct virtual_page_entr *vme = hash_entry(e, struct virtual_page_entr, elem);
	return hash_int((int)vme->vaddr);
}

static bool smaller_virtual_page_entr(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
	struct virtual_page_entr *entry_a = hash_entry(a, struct virtual_page_entr, elem);
  struct virtual_page_entr *entry_b = hash_entry(b, struct virtual_page_entr, elem);
  return entry_a->vaddr < entry_b->vaddr;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  
  hash_init(&(thread_current()->vm), hash_virtual_page_entr, smaller_virtual_page_entr, NULL);
  
  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);

  /* If load failed, quit. */
  palloc_free_page (file_name);

  sema_up(&thread_current()->load_lock);
  if (!success) EXIT(-1);
  
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{
  // TODO
  
  struct thread *cur = thread_current();
  struct thread *child = NULL;
  int exit_status = -1; // Default: -1
    
  struct list_elem *e;
  FOR_LIST(e, &(cur->child_list)) {
    child = list_entry(e, struct thread, child_elem);
            
    if (child->tid == child_tid) { // If found child
      sema_down(&(child->child_lock));
      exit_status = child->exit_status;
      list_remove(&(child->child_elem));
      sema_up(&(child->mem_lock)); 
      break;  // Get out of loop
    }
}
    
  return exit_status;
}

static void destroy_vm(struct hash_elem *elem, void *aux UNUSED) {
	struct virtual_page_entr *e = hash_entry(elem, struct virtual_page_entr, elem);
    if (e->is_in_memory) {
        free_and_remove_page(pagedir_get_page(thread_current()->pagedir, e->vaddr));
        pagedir_clear_page(thread_current()->pagedir, e->vaddr);
    }
    free(e);
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
  /* Added in #Proj 4 */
  hash_destroy(&cur->vm, destroy_vm);

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
    FOR_RANGE(i, 2, 128) {
      if(cur->FD[i] != NULL) file_close(cur->FD[i]);
    }

    sema_up(&(cur->child_lock));
    sema_down(&(cur->mem_lock));
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();
  
  int argc=0;
	char* argv[128];
	char cp_file_name[128];
	char *token_nxt;

	snprintf(cp_file_name, sizeof(cp_file_name), "%s", file_name);
  
  for(char* token = strtok_r(cp_file_name, " ", &token_nxt); 
      token != NULL; token = strtok_r(NULL, " ", &token_nxt)) {
    argv[argc++] = token;
  }

  char*  file_name_ptr = argv[0];

  /* Open executable file. */
  file = filesys_open (file_name_ptr);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name_ptr);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;
  // setup_stack() 다음에:
  // TODO: construct stack
  /* Manual p.40
  • In userprog/process.c, there is setup_stack() which allocates a minimal stack page (4KB).
  • Since the given code only allocates stack page, we need to make up the stack after setup_stack().
  • Make up the stack referring to "3.5 80x86 Calling Convention" in Pintos manual
  */
  make_stack(argv, esp, argc);

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}

/* load() helpers. */

void make_stack(char* argv[], void **esp, int argc) {
  int ptr_argv[128];
  int alignment= 0;
  int i = argc-1;

  while(i >= 0) {
    int argvl = 1 + strlen(argv[i]);
    *esp -= argvl;
    memcpy(*esp, argv[i], argvl);
    alignment += argvl;
    ptr_argv[i--] = *esp;
  }
  alignment = (alignment % 4 == 0) ? 0 : 4 - alignment % 4;
  *esp -= alignment;
  memset(*esp, 0, alignment); // NULL padding

  *esp -= 4;
  memset(*esp, 0, 4); // argv[argc] = NULL

  i = argc-1;
  while(i >= 0) { // argv
    *esp -= 4;
    memcpy(*esp, &ptr_argv[i--], 4);
  }

  int argv_addr_ptr = *esp; // argv
  *esp -= 4;
  memcpy(*esp, &argv_addr_ptr, 4); // argv

  *esp -= 4;
  memcpy(*esp, &argc, 4); // argc

  *esp -= 4;
  memset(*esp, 0, 4); // fake return address

  // hex_dump(*esp, *esp, 100, true); // FOR DEBUGGING

}

bool memory_fault_handler(struct virtual_page_entr *vme) {
  if (vme->is_in_memory) return false;

  struct page *new_page = page_allocation(PAL_USER);
  if (!new_page) return false;
  new_page->vme = vme;
  bool load_success = load_page_content(new_page, vme);
  
  if (!load_success) {
      free_and_remove_page(new_page->kernel_addr);
      return false;
  }
  return install_loaded_page(vme, new_page);
}

bool load_page_content(struct page *page, struct virtual_page_entr *vme) {
    if (!vme->type) {
        return read_file_into_memory(page->kernel_addr, vme);
    } else  {
        read_from_swap(vme->swap_index, page->kernel_addr);
        return true; // Assume swap_in always succeeds for this context
    }

    // For unsupported VM entry types
    return false;
}

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

static bool initialize_vm_entry(struct virtual_page_entr *entry, struct file *file, 
                                void *vaddr, off_t ofs, size_t read_bytes, size_t zero_bytes, bool writable) {
    entry->type = entry->is_in_memory = false;
    entry->backing_file = file;
    entry->vaddr = vaddr;
    entry->read_bytes = read_bytes;
    entry->zero_bytes = zero_bytes;
    entry->file_offset = ofs;
    entry->is_writable = writable;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  struct file *reopen_file = file_reopen(file);
  file_seek (file, ofs);

  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      struct virtual_page_entr *entry = malloc(sizeof(struct virtual_page_entr));
      
      if (!entry) return false;
      
      initialize_vm_entry(entry, reopen_file, upage, ofs, page_read_bytes, page_zero_bytes, writable);
      
      if (!add_virtual_page_entr(&thread_current()->vm, entry)) {
          free(entry);
          return false;
      }
      
      upage += PGSIZE;
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      ofs += page_read_bytes;
  }
  return true;
}

static bool allocate_stack_page(struct page **kpage, void **virtual_address) {
  *kpage = page_allocation(PAL_USER | PAL_ZERO);
  if (*kpage == NULL) 
    return false;
  *virtual_address = ((uint8_t *) PHYS_BASE) - PGSIZE;
  return true;
}

static bool initialize_stack_vme(struct virtual_page_entr **vme, void *virtual_address) {
  *vme = malloc(sizeof(struct virtual_page_entr));
  if (*vme == NULL) return false;
  
  (*vme)->vaddr = pg_round_down(virtual_address);
  (*vme)->is_in_memory = (*vme)->is_writable = true;
  (*vme)->type = false; 
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  void *vaddress;
  struct virtual_page_entr *vme;
  struct page *pg;
  
  if (!allocate_stack_page(&pg, &vaddress)) return false;
  
  if (!install_page(vaddress, pg->kernel_addr, true)) {
    free_and_remove_page(pg->kernel_addr);
    return false;
  }
  *esp = PHYS_BASE;
  if (!initialize_stack_vme(&vme, vaddress)) {
    free_and_remove_page(pg->kernel_addr);
    return false;
  }
  pg->vme = vme;
  return add_virtual_page_entr(&thread_current()->vm, vme);
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
