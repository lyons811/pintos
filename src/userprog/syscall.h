#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>
#include "threads/thread.h"
typedef int pid_t;
/* System call initialization */
void syscall_init(void);

/* Process control */
void sys_exit(int status);
void sys_halt(void);
int sys_exec(const char *cmdline);
int sys_wait(pid_t pid);

/* File operations */
int sys_open(const char *file);
int sys_filesize(int fd);
int sys_read(int fd, void *buffer, unsigned size);
int sys_write(int fd, const void *buffer, unsigned size);
void sys_seek(int fd, unsigned position);
unsigned sys_tell(int fd);
void sys_close(int fd);

/* File system operations */
bool sys_create(const char *file, unsigned initial_size);
bool sys_remove(const char *file);

/* Helper functions */
void close_extra_files(int fd_num);
void close_thread_files(tid_t tid);

#endif /* userprog/syscall.h */