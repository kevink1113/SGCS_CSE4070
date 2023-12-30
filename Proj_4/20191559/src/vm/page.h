#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <hash.h>

// Virtual memory entry structure representing a page in the process's virtual address space.
struct virtual_page_entr {
	unsigned long file_offset;   // Offset within the backing file.
  unsigned long read_bytes;    // Number of bytes to read from the file.
  unsigned long zero_bytes;    // Number of bytes to be zero-filled.
  unsigned long swap_index;    // Swap slot index if the page is in swap space.
  
  bool type;                	 // Type of VM entry: 0: File / 1: Swap
  bool is_dirty;               // True if the page has been modified since it was loaded.
	bool is_writable;            // Indicates if the memory area is writable.
  bool is_in_memory;           // True if the page is loaded into physical memory.

  void *upage;                 // User virtual address of the page.
  void *vaddr;                 // Virtual address mapped by this entry.
  struct hash_elem elem;  		 // Hash table element for thread's VM hash table.
	struct file *backing_file;   // File backing this VM entry, if any.
};

struct virtual_page_entr *get_virtual_page_entr_by_vaddr(void *virtual_address);						        // Get a VM entry by its virtual address.
bool read_file_into_memory(void *kernel_addr, struct virtual_page_entr *virtual_page_entr);	        // Read a file into memory.
bool add_virtual_page_entr(struct hash *vm_table, struct virtual_page_entr *virtual_page_entr);		  // Add a VM entry to the VM hash table.
bool remove_virtual_page_entr(struct hash *vm_table, struct virtual_page_entr *virtual_page_entr);  // Remove a VM entry from the VM hash table.

// Page structure representing a physical frame.
struct page {
  void* user_addr;                  // User virtual address of the page.
	struct list_elem lru;					    // List element for the LRU list.
  struct frame *frame;              // Frame containing the page.
	struct thread *owner_thread;			// Thread that owns the page.
	void* kernel_addr;						    // Kernel virtual address of the page.
	struct virtual_page_entr *vme;		// VM entry corresponding to the page.

};

#endif