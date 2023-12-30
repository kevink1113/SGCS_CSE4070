#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "lib/user/syscall.h"

typedef int pid_t;

void syscall_init (void);

/* Added in #Proj 1. */
/**
 * HALT: 0
 * EXIT: 1
 * EXEC: 1
 * WAIT: 1
 * CREATE: 2
 * REMOVE: 1
 * OPEN: 1
 * FILESIZE: 1
 * READ: 3
 * WRITE: 3
 * SEEK: 2
 * TELL: 1
 * CLOSE: 1
*/
void HALT (void);
void EXIT (int status);
pid_t EXEC (const char *cmd_lime);
int WAIT (pid_t pid);
bool CREATE (const char *file, unsigned initial_size);
bool REMOVE (const char *file);
int OPEN (const char *file);
int FILESIZE (int fd);
int READ (int fd, void *buffer, unsigned size);
int WRITE (int fd, const void *buffer, unsigned size);
void SEEK (int fd, unsigned position);
unsigned TELL (int fd);
void CLOSE (int fd);

int FIBONACCI(int n);
int MAX_OF_FOUR_INT(int a, int b, int c, int d);

#endif /* userprog/syscall.h */

