#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include "lib/kernel/list.h"
#include "devices/shutdown.h"
#include "devices/input.h"

static void syscall_handler(struct intr_frame *);
static bool is_valid_ptr(const void *user_ptr);
static struct file_descriptor *retrieve_file(int fd);

static struct lock filesys_lock;

struct file_descriptor {
  int fd_num;
  tid_t owner;
  struct file *file_struct;
  struct list_elem elem;
};

void
syscall_init(void) 
{
  lock_init(&filesys_lock);
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f) 
{
  uint32_t *esp = f->esp;

  if (!is_valid_ptr(esp)) {
    sys_exit(-1);
  }

  switch (*esp) {
    case SYS_HALT:
      sys_halt();
      break;
    case SYS_EXIT:
      if (!is_valid_ptr(esp + 1)) sys_exit(-1);
      sys_exit(*(esp + 1));
      break;
    case SYS_EXEC:
      if (!is_valid_ptr(esp + 1) || !is_valid_ptr(*(esp + 1))) sys_exit(-1);
      f->eax = sys_exec((const char *)*(esp + 1));
      break;
    case SYS_WAIT:
      if (!is_valid_ptr(esp + 1)) sys_exit(-1);
      f->eax = process_wait(*(esp + 1));
      break;
    case SYS_CREATE:
      if (!is_valid_ptr(esp + 4) || !is_valid_ptr(esp + 5) || !is_valid_ptr(*(esp + 4))) sys_exit(-1);
      lock_acquire(&filesys_lock);
      f->eax = filesys_create((const char *)*(esp + 4), (off_t)*(esp + 5));
      lock_release(&filesys_lock);
      break;
    case SYS_REMOVE:
      if (!is_valid_ptr(esp + 1) || !is_valid_ptr(*(esp + 1))) sys_exit(-1);
      lock_acquire(&filesys_lock);
      f->eax = filesys_remove((const char *)*(esp + 1));
      lock_release(&filesys_lock);
      break;
    case SYS_OPEN:
      if (!is_valid_ptr(esp + 1) || !is_valid_ptr(*(esp + 1))) sys_exit(-1);
      f->eax = sys_open((const char *)*(esp + 1));
      break;
    case SYS_FILESIZE:
      if (!is_valid_ptr(esp + 1)) sys_exit(-1);
      f->eax = sys_filesize(*(esp + 1));
      break;
    case SYS_READ:
      if (!is_valid_ptr(esp + 5) || !is_valid_ptr(esp + 6) || !is_valid_ptr(esp + 7) ||
          !is_valid_ptr(*(esp + 6)) || !is_valid_ptr(*(esp + 6) + *(esp + 7) - 1)) sys_exit(-1);
      f->eax = sys_read(*(esp + 5), (void *)*(esp + 6), *(esp + 7));
      break;
    case SYS_WRITE:
      if (!is_valid_ptr(esp + 5) || !is_valid_ptr(esp + 6) || !is_valid_ptr(esp + 7) ||
          !is_valid_ptr(*(esp + 6)) || !is_valid_ptr(*(esp + 6) + *(esp + 7) - 1)) sys_exit(-1);
      f->eax = sys_write(*(esp + 5), (const void *)*(esp + 6), *(esp + 7));
      break;
    case SYS_SEEK:
      if (!is_valid_ptr(esp + 4) || !is_valid_ptr(esp + 5)) sys_exit(-1);
      sys_seek(*(esp + 4), *(esp + 5));
      break;
    case SYS_TELL:
      if (!is_valid_ptr(esp + 1)) sys_exit(-1);
      f->eax = sys_tell(*(esp + 1));
      break;
    case SYS_CLOSE:
      if (!is_valid_ptr(esp + 1)) sys_exit(-1);
      sys_close(*(esp + 1));
      break;
    default:
      printf("[ERROR] Unimplemented system call!\n");
      sys_exit(-1);
  }
}

void
sys_halt(void) 
{
  shutdown_power_off();
}

void
sys_exit(int status) 
{
  struct thread *curr = thread_current();
  printf("%s: exit(%d)\n", curr->name, status);
  
  struct thread *parent = thread_get_by_id(curr->parent_tid);
  if (parent != NULL) {
    struct list_elem *e;
    for (e = list_begin(&parent->children); e != list_end(&parent->children); e = list_next(e)) {
      struct child_status *child = list_entry(e, struct child_status, elem_child_status);
      if (child->child_tid == curr->tid) {
        lock_acquire(&parent->child_lock);
        child->exited = true;
        child->child_exit_status = status;
        lock_release(&parent->child_lock);
        break;
      }
    }
  }
  
  thread_exit();
}

int
sys_exec(const char *cmdline) 
{
  lock_acquire(&filesys_lock);
  struct file *f = filesys_open(cmdline);
  if (f == NULL) {
    lock_release(&filesys_lock);
    return -1;
  }
  file_close(f);
  lock_release(&filesys_lock);

  tid_t tid = process_execute(cmdline);
  struct thread *cur = thread_current();
  
  lock_acquire(&cur->child_lock);
  cur->child_load = 0;
  while (cur->child_load == 0)
    cond_wait(&cur->child_condition, &cur->child_lock);
  if (cur->child_load == -1)
    tid = -1;
  lock_release(&cur->child_lock);
  
  return tid;
}

int
sys_open(const char *file) 
{
  lock_acquire(&filesys_lock);
  struct file *file_struct = filesys_open(file);
  if (file_struct == NULL) {
    lock_release(&filesys_lock);
    return -1;
  }

  struct file_descriptor *fd = malloc(sizeof(struct file_descriptor));
  fd->file_struct = file_struct;
  fd->fd_num = thread_current()->next_fd++;
  fd->owner = thread_current()->tid;
  list_push_back(&thread_current()->open_files, &fd->elem);

  lock_release(&filesys_lock);
  return fd->fd_num;
}

int
sys_filesize(int fd) 
{
  lock_acquire(&filesys_lock);
  struct file_descriptor *file_desc = retrieve_file(fd);
  int size = -1;
  if (file_desc != NULL) {
    size = file_length(file_desc->file_struct);
  }
  lock_release(&filesys_lock);
  return size;
}

int
sys_read(int fd, void *buffer, unsigned size) 
{
  lock_acquire(&filesys_lock);
  int bytes_read = 0;

  if (fd == STDIN_FILENO) {
    uint8_t *buf = buffer;
    for (unsigned i = 0; i < size; i++) {
      if ((*buf++ = input_getc()) == '\0') break;
    }
    bytes_read = size;
  } else if (fd != STDOUT_FILENO) {
    struct file_descriptor *file_desc = retrieve_file(fd);
    if (file_desc != NULL) {
      bytes_read = file_read(file_desc->file_struct, buffer, size);
    }
  }

  lock_release(&filesys_lock);
  return bytes_read;
}

int
sys_write(int fd, const void *buffer, unsigned size) 
{
  lock_acquire(&filesys_lock);
  int bytes_written = 0;

  if (fd == STDOUT_FILENO) {
    putbuf(buffer, size);
    bytes_written = size;
  } else if (fd != STDIN_FILENO) {
    struct file_descriptor *file_desc = retrieve_file(fd);
    if (file_desc != NULL) {
      bytes_written = file_write(file_desc->file_struct, buffer, size);
    }
  }

  lock_release(&filesys_lock);
  return bytes_written;
}

void
sys_seek(int fd, unsigned position) 
{
  lock_acquire(&filesys_lock);
  struct file_descriptor *file_desc = retrieve_file(fd);
  if (file_desc != NULL) {
    file_seek(file_desc->file_struct, position);
  }
  lock_release(&filesys_lock);
}

unsigned
sys_tell(int fd) 
{
  lock_acquire(&filesys_lock);
  unsigned position = 0;
  struct file_descriptor *file_desc = retrieve_file(fd);
  if (file_desc != NULL) {
    position = file_tell(file_desc->file_struct);
  }
  lock_release(&filesys_lock);
  return position;
}

void
sys_close(int fd) 
{
  lock_acquire(&filesys_lock);
  struct file_descriptor *file_desc = retrieve_file(fd);
  if (file_desc != NULL && file_desc->owner == thread_current()->tid) {
    close_extra_files(fd);
  }
  lock_release(&filesys_lock);
}

static bool
is_valid_ptr(const void *user_ptr) 
{
  return user_ptr != NULL && is_user_vaddr(user_ptr) &&
         pagedir_get_page(thread_current()->pagedir, user_ptr) != NULL;
}

static struct file_descriptor *
retrieve_file(int fd) 
{
  struct thread *t = thread_current();
  struct list_elem *e;

  for (e = list_begin(&t->open_files); e != list_end(&t->open_files); e = list_next(e)) {
    struct file_descriptor *file_desc = list_entry(e, struct file_descriptor, elem);
    if (file_desc->fd_num == fd) {
      return file_desc;
    }
  }

  return NULL;
}

void
close_extra_files(int fd_num) 
{
  struct thread *t = thread_current();
  struct list_elem *e;

  for (e = list_begin(&t->open_files); e != list_end(&t->open_files);) {
    struct file_descriptor *file_desc = list_entry(e, struct file_descriptor, elem);
    if (file_desc->fd_num == fd_num) {
      e = list_remove(e);
      file_close(file_desc->file_struct);
      free(file_desc);
      return;
    } else {
      e = list_next(e);
    }
  }
}

void
close_thread_files(tid_t tid) 
{
  struct thread *t = thread_current();
  struct list_elem *e;

  for (e = list_begin(&t->open_files); e != list_end(&t->open_files);) {
    struct file_descriptor *file_desc = list_entry(e, struct file_descriptor, elem);
    if (file_desc->owner == tid) {
      e = list_remove(e);
      file_close(file_desc->file_struct);
      free(file_desc);
    } else {
      e = list_next(e);
    }
  }
}