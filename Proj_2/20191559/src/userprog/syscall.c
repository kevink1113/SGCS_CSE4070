#include "syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

#define FOR(i, n) for(int i=0; i<n; i++)
#define FOR1(i, n) for(int i=1; i<=n; i++)
#define FOR_RANGE(i, start, end) for(int i = start; i < end; i++)
typedef int32_t off_t;
struct lock lock_file;

#define VERIFY_ADDR(ADDR) \
    do { \
        if (!is_user_vaddr(ADDR)) EXIT(-1); \
    } while(0)

static void syscall_handler (struct intr_frame *);

struct file {
  struct inode *inode;
  off_t pos;
  bool deny_write;
};

void
syscall_init (void) 
{
  lock_init(&lock_file);
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
      VERIFY_ADDR(f->esp + 4);
      EXIT(*(uint32_t *)(f->esp + 4)); //
      break;
    case SYS_EXEC:
      VERIFY_ADDR(f->esp + 4);
      f->eax = EXEC(*(uint32_t *)(f->esp + 4)); //
      break;
    case SYS_WAIT:
      VERIFY_ADDR(f->esp + 4);
      f->eax = WAIT(*(uint32_t *)(f->esp + 4)); //
      break;
    case SYS_CREATE:
    VERIFY_ADDR(f->esp + 4);
      f->eax = CREATE(*(uint32_t *)(f->esp + 4), *(uint32_t *)(f->esp + 8)); //
      break;
    case SYS_REMOVE:
      VERIFY_ADDR(f->esp + 4);
      f->eax = REMOVE(*(uint32_t *)(f->esp + 4)); //
      break;
    case SYS_OPEN:
      VERIFY_ADDR(f->esp + 4);
      f->eax = OPEN(*(uint32_t *)(f->esp + 4)); //
      break;
    case SYS_FILESIZE:
      VERIFY_ADDR(f->esp + 4);
      f->eax = FILESIZE(*(uint32_t *)(f->esp + 4)); //
      break;
    case SYS_READ:
      VERIFY_ADDR(f->esp + 4);
      f->eax = READ(*(uint32_t *)(f->esp + 4), *(uint32_t *)(f->esp + 8), *(uint32_t *)(f->esp + 12)); //
      break;
    case SYS_WRITE:
      f->eax = WRITE(*(uint32_t *)(f->esp + 4), *(uint32_t *)(f->esp + 8), *(uint32_t *)(f->esp + 12));
      break;
    case SYS_SEEK:
      VERIFY_ADDR(f->esp + 4);
      SEEK(*(uint32_t *)(f->esp + 4), *(uint32_t *)(f->esp + 8)); //
      break;
    case SYS_TELL:
      VERIFY_ADDR(f->esp + 4);
      f->eax = TELL(*(uint32_t *)(f->esp + 4)); //
      break;
    case SYS_CLOSE:
      VERIFY_ADDR(f->esp + 4);
      CLOSE(*(uint32_t *)(f->esp + 4)); //
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
  FOR_RANGE(i, 3, 128) if (thread_current()->FD[i]) CLOSE(i);
  thread_exit();
}

int EXEC (const char *cmd) {
  return process_execute(cmd);
}

int WAIT (int pid) {
  return process_wait(pid);
}

bool CREATE (const char *file, unsigned size) {
  if(!file) EXIT(-1);
  return filesys_create(file, size);
}

bool REMOVE (const char *file) {
  if(!file) EXIT(-1);
  return filesys_remove(file);
}

int OPEN (const char *file) {
  // Check file validation
  if(!file) EXIT(-1);
  VERIFY_ADDR(file);
  lock_acquire(&lock_file);
  struct file *f = filesys_open(file);
  int res = -1;
  if (!f) {
    lock_release(&lock_file);
    return res;
  }
  // Find empty FD and OPEN
  FOR_RANGE(i, 3, 128) {
    if (!thread_current()->FD[i]) {
      if(!strcmp(thread_current()->name, file)) file_deny_write(f);
      thread_current()->FD[i] = f;
      lock_release(&lock_file);
      return res = i;
    }
  }
  lock_release(&lock_file);
  return res;
}

int FILESIZE (int fd) {
  if(!thread_current()->FD[fd]) EXIT(-1);
  // Check FD validation
  if (!fd) return -1;
  return file_length(thread_current()->FD[fd]);
}

int READ (int fd, void *buffer, unsigned size) {
  if(!buffer) EXIT(-1);
  if(!thread_current()->FD[fd]) EXIT(-1);
  VERIFY_ADDR(buffer);
  if (!fd) {
    lock_acquire(&lock_file);
    FOR(i, size) *((uint8_t *)buffer + i) = input_getc();
    lock_release(&lock_file);
    return size;
  } else if (fd > 2) {
    lock_acquire(&lock_file);
    int res = file_read(thread_current()->FD[fd], buffer, size);
    lock_release(&lock_file);
    return res;
  } else return -1;
}

int WRITE (int fd, const void *buffer, unsigned size) {
  if(!buffer) EXIT(-1);
  VERIFY_ADDR(buffer);
  if (fd == 1) {
    lock_acquire(&lock_file);
    putbuf(buffer, size);
    lock_release(&lock_file);
    return size;
  } else if (fd > 2) {
    if(!thread_current()->FD[fd]) EXIT(-1); 
    lock_acquire(&lock_file);
    int res = file_write(thread_current()->FD[fd], buffer, size);
    lock_release(&lock_file);
    return res;
  } else return -1;
}

void SEEK (int fd, unsigned pos) {
  if(!thread_current()->FD[fd]) EXIT(-1);
  file_seek(thread_current()->FD[fd], pos);
}

unsigned TELL (int fd) {
  if(!thread_current()->FD[fd]) EXIT(-1);
  return file_tell(thread_current()->FD[fd]);
}

void CLOSE (int fd) {
  if(!thread_current()->FD[fd]) EXIT(-1);
  int res = file_close(thread_current()->FD[fd]);
  thread_current()->FD[fd] = NULL;
  return res;
}


int FIBONACCI(int n) {
  int a = 0, b = 1, c = 0;
  if (n == 0) return a;
  FOR_RANGE(i, 2, n+1) {
    c = a + b; a = b; b = c;
  }
  return b;
}

int MAX_OF_FOUR_INT(int a, int b, int c, int d) {
  int max_ab = a > b ? a : b;
  int max_cd = c > d ? c : d;
  return max_ab > max_cd ? max_ab : max_cd;
}
