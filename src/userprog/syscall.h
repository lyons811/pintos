#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "userprog/process.h"

struct lock filesys_lock;

void sys_exit (int);
void sys_halt(void);
int sys_exec (const char *cmdline);
int sys_open(char * file);
int sys_filesize(int fd_num);
void syscall_init (void);
int sys_write(int fd, const void *buffer, unsigned size);
int sys_read(int fd, const void *buffer, unsigned size);
void sys_seek(int fd, unsigned position);
unsigned sys_tell(int fd);
void close_extra_files(int fd_num);
void close_thread_files(tid_t tid);
void sys_close(int fd);
static mapid_t mmap (int, void *);
static void munmap (mapid_t);

#endif /* userprog/syscall.h */