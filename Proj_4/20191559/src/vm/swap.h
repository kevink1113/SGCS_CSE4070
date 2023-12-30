#ifndef SWAP_H
#define SWAP_H

void handle_block_io(bool is_read, size_t swap_index, void *physical_addr); // handle block io
void initialize_swap();                                                     // initialize swap table
void iterate_swap(size_t swap_index, void *aux, bool rw);                   // iterate swap table
void read_from_swap(size_t swap_index, void *physical_addr) ;               // read from swap table
size_t write_to_swap(void *physical_addr);                                  // write to swap table

#endif