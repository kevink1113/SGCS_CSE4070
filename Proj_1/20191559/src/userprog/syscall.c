#include "syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

void
verify_addr(void *addr) {
  if (false == is_user_vaddr(addr)) EXIT(-1);
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // printf ("system call!\n");
  // hex_dump(f->esp, f->esp, 100, 1); // Added to print stack
  switch (*(uint32_t *)(f->esp)) {
    case SYS_HALT:
      HALT();
      break;
    case SYS_EXIT:
      verify_addr(f->esp + 4);
      EXIT(*(uint32_t *)(f->esp + 4)); //
      break;
    case SYS_EXEC:
      verify_addr(f->esp + 4);
      f->eax = EXEC(*(uint32_t *)(f->esp + 4)); //
      break;
    case SYS_WAIT:
      verify_addr(f->esp + 4);
      f->eax = WAIT(*(uint32_t *)(f->esp + 4)); //
      break;
    case SYS_CREATE:
      // f->eax = CREATE(*(uint32_t *)(f->esp + 4), *(uint32_t *)(f->esp + 8)); //
      break;
    case SYS_REMOVE:
      // f->eax = REMOVE(*(uint32_t *)(f->esp + 4)); //
      break;
    case SYS_OPEN:
      // f->eax = OPEN(*(uint32_t *)(f->esp + 4)); //
      break;
    case SYS_FILESIZE:
      // f->eax = FILESIZE(*(uint32_t *)(f->esp + 4)); //
      break;
    case SYS_READ:
      verify_addr(f->esp + 4);
      f->eax = READ(*(uint32_t *)(f->esp + 4), *(uint32_t *)(f->esp + 8), *(uint32_t *)(f->esp + 12)); //
      break;
    case SYS_WRITE:
      f->eax = WRITE(*(uint32_t *)(f->esp + 4), *(uint32_t *)(f->esp + 8), *(uint32_t *)(f->esp + 12));
      break;
    case SYS_SEEK:
      // SEEK(*(uint32_t *)(f->esp + 4), *(uint32_t *)(f->esp + 8)); //
      break;
    case SYS_TELL:
      // f->eax = TELL(*(uint32_t *)(f->esp + 4)); //
      break;
    case SYS_CLOSE:
      // CLOSE(*(uint32_t *)(f->esp + 4)); //
      break;
    case SYS_FIBONACCI:
      f->eax = FIBONACCI(*(uint32_t *)(f->esp + 4)); //
      break;
    case SYS_MAX_OF_FOUR_INT:
      f->eax = MAX_OF_FOUR_INT(*(uint32_t *)(f->esp + 4), *(uint32_t *)(f->esp + 8), *(uint32_t *)(f->esp + 12), *(uint32_t *)(f->esp + 16)); //
      break;
  }
  // thread_exit ();
}
void HALT (void) {
  shutdown_power_off();
}

void EXIT (int status) {
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_current()->exit_status = status; // Added to print like 'exit(81)'
  thread_exit();
}

int EXEC (const char *cmd_lime) {
  return process_execute(cmd_lime);
}

int WAIT (int pid) {
  return process_wait(pid);
}

// Reliability check needed
int READ (int fd, void *buffer, unsigned size) {
  if (!fd) {
    for (unsigned i = 0; i < size; i++) {
      *((uint8_t *)buffer + i) = input_getc();
    }
    return size;
  }
  return -1;
}

int WRITE (int fd, const void *buffer, unsigned size) {

  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }
  return -1; 
}

int FIBONACCI(int n) {
  int a = 0, b = 1, c = 0;
  if (n == 0) 
    return a;

  for (int i = 2; i <= n; i++) {
    c = a + b;
    a = b;
    b = c;
  }
  return b;
}

int MAX_OF_FOUR_INT(int a, int b, int c, int d) {
  int max_ab = a > b ? a : b;
  int max_cd = c > d ? c : d;
  return max_ab > max_cd ? max_ab : max_cd;
}
