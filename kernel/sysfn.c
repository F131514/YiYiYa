/*******************************************************************
 * Copyright 2021-2080 evilbinary
 * 作者: evilbinary on 01/01/20
 * 邮箱: rootdebug@163.com
 ********************************************************************/

#include "sysfn.h"

#include "fd.h"
#include "kernel.h"
#include "kernel/elf.h"
#include "loader.h"
#include "thread.h"
#include "vfs.h"

int sys_print(char* s) {
  thread_t* current = thread_current();
  kprintf("sys print %d %s\n", current->id, s);
  // kprintf("%s", s);
  return 0;
}

void sys_test() {
  thread_t* current = thread_current();
  kprintf("sys test %d\n", current->id);

  kprintf("-------dump thread %d-------------\n", current->id);
  thread_dump(current);
}

int sys_print_at(char* s, u32 x, u32 y) {
  set_cursor(x, y);
  kprintf("%s", s);
  return 0;
}

size_t sys_ioctl(u32 fd, u32 cmd, void* args) {
  u32 ret = 0;
  fd_t* f = thread_find_fd_id(thread_current(), fd);
  if (f == NULL) {
    // f = find_fd(fd);
  }
  if (f == NULL) {
    kprintf("ioctl not found fd %d\n", fd);
    return 0;
  }
  vnode_t* node = f->data;
  ret = vioctl(node, cmd, args);

#ifdef SYS_DEBUG
  kprintf("sys ioctl fd %d %s cmd %x ret %x\n", fd, f->name, cmd, ret);
#endif
  return ret;
}

u32 sys_open(char* name, int attr) {
  // mm_dump();
  // kprintf("open %s attr %x\n",name,attr&O_CREAT==O_CREAT);

  thread_t* current = thread_current();
  if (current == NULL) {
    kprintf(" cannot find current thread\n");
    return -1;
  }
  int f = thread_find_fd_name(current, name);
  if (f >= 0) {
    kprintf("sys open name return : %s fd: %d\n", name, f);
    return f;
  }
  vnode_t* file = vfs_open(NULL, name, attr);
  if (file == NULL) {
    kprintf("sys open file %s error\n", name);
    return -1;
  }
  fd_t* fd = fd_new(file, DEVICE_TYPE_FILE, name);
  if (fd == NULL) {
    kprintf(" new fd error\n");
    return -1;
  }

  f = thread_add_fd(current, fd);
  if (f < 0) {
    kprintf("sys open %s error\n", name);
    return -1;
  }
  kprintf("sys open new name: %s fd:%d fd->id:%d ptr:%x tid:%d\n", name, f,
          fd->id, fd, current->id);
  return f;
}

void sys_close(u32 fd) {}

size_t sys_write(u32 fd, void* buf, size_t nbytes) {
  thread_t* current = thread_current();
  fd_t* f = thread_find_fd_id(current, fd);
  if (f == NULL) {
    kprintf("write not found fd %d tid %d\n", fd, current->id);
    thread_dump_fd(current);
    return 0;
  }
  vnode_t* node = f->data;
  if (node == NULL) {
    kprintf("sys write node is null tid %d \n", current->id);
    return -1;
  }
  // kprintf("sys write %d %s fd:%s\n",current->id,buf,f->name);
  u32 ret = vwrite(node, f->offset, nbytes, buf);
  return ret;
}
size_t sys_read(u32 fd, void* buf, size_t nbytes) {
  thread_t* current = thread_current();
  fd_t* f = thread_find_fd_id(current, fd);
  if (f == NULL) {
    kprintf("read not found fd %d tid %d\n", fd, current->id);
    return 0;
  }
  vnode_t* node = f->data;
  if (node == NULL) {
    kprintf("sys read node is null\n");
    return -1;
  }
  u32 ret = vread(node, f->offset, nbytes, buf);
  return ret;
}

size_t sys_seek(u32 fd, ulong offset) {
  fd_t* f = thread_find_fd_id(thread_current(), fd);
  if (f == NULL) {
    kprintf("seek not found fd %d\n", fd);
    return 0;
  }
  f->offset = offset;
  return 1;
}

size_t sys_yeild() { thread_yield(); }

void sys_exit(int status) {
  thread_t* current = thread_current();
  thread_exit(current, status);
  thread_dumps();
  kprintf("sys exit tid %d status %d\n", current->id, status);
}

void* sys_vmap(void* addr, size_t size) {
  thread_t* current = thread_current();
  vmemory_area_t* area = vmemory_area_create(
      current->vmm->vaddr + current->vmm->size, size, MEMORY_HEAP);
  vmemory_area_add(current->vmm, area);
  return area->vaddr;
}

void sys_vumap(void* ptr, size_t size) {
  thread_t* current = thread_current();
  vmemory_area_t* area = vmemory_area_find(current->vmm, ptr, size);
  if (area == NULL) return;
  vmemory_area_free(area);
}

void* sys_valloc(void* addr, size_t size) { return valloc(addr, size); }

void* sys_vheap() {
  thread_t* current = thread_current();
  return current->vmm->vaddr;
}

void sys_vfree(void* addr) {
  // todo
  vfree(addr);
}

#define PAGE_CLONE 1

u32 sys_exec(char* filename, char* const argv[], char* const envp[]) {
  int fd = sys_open(filename, 0);
  if (fd < 0) {
    sys_close(fd);
    kprintf("sys exec file not found %s\n", filename);
    return -1;
  }
  sys_close(fd);
  thread_t* current = thread_current();
  u8* stack0 = kmalloc(KERNEL_THREAD_STACK_SIZE);
  u8* stack3 = kmalloc_alignment(THREAD_STACK_SIZE,PAGE_SIZE);
  u8* vstack3 = STACK_ADDR;
  thread_t* t = thread_create_ex((u32*)&run_elf_thread, stack0, vstack3,
                                 THREAD_STACK_SIZE, NULL);

  t->context.page_dir = current->context.page_dir;
  #ifdef PAGE_CLONE
  t->context.page_dir = page_alloc_clone(current->context.page_dir);
  #endif
  t->context.kernel_page_dir = current->context.kernel_page_dir;

  // init vmm
  t->vmm = vmemory_area_create(HEAP_ADDR, MEMORY_CREATE_SIZE, MEMORY_HEAP);
  vmemory_area_t* vmexec =
      vmemory_area_create(EXEC_ADDR, MEMORY_EXEC_SIZE, MEMORY_EXEC);
  vmemory_area_add(t->vmm, vmexec);
  vmemory_area_t* stack =
      vmemory_area_create(vstack3, THREAD_STACK_SIZE, MEMORY_STACK);
  vmemory_area_add(t->vmm, stack);

  // init data
  exec_t* data = kmalloc(sizeof(exec_t) + 4);
  kstrcpy(data->filename, filename);
  data->argv = argv;
  data->argc = 1;
  t->data = data;

  // init fds
  for (int i = 0; i < 3; i++) {
    t->fds[i] = current->fds[i];
  }
  // thread_dump(t);
  thread_run(t);
  return 0;
}

int sys_fork() {
  thread_t* current = thread_current();
  if (current == NULL) {
    kprintf("current is null\n");
    return -1;
  }
  thread_t* copy_thread = thread_clone(current, STACK_ADDR, THREAD_STACK_SIZE);

  kprintf("-------dump current thread %d-------------\n", current->id);
  thread_dump(current);
  kprintf("-------dump clone thread %d-------------\n", copy_thread->id);
  thread_dump(copy_thread);

  interrupt_context_t* context = copy_thread->context.esp0;
  context_ret(context) = 0;

  thread_run(copy_thread);
  return current->id;
}

int sys_pipe(int fds[2]) {
  thread_t* current = thread_current();
  vnode_t* node = pipe_make(PAGE_SIZE);
  fd_t* fd0 = fd_new(node, DEVICE_TYPE_VIRTUAL, "pipe0");
  fd_t* fd1 = fd_new(node, DEVICE_TYPE_VIRTUAL, "pipe1");
  fds[0] = thread_add_fd(current, fd0);
  ;
  fds[1] = thread_add_fd(current, fd1);
  ;
  return 0;
}

int sys_getpid() {
  thread_t* current = thread_current();
#ifdef SYS_DEBUG
  kprintf("sys get pid %d\n", current->id);
#endif
  return current->id;
}

int sys_getppid() {
  thread_t* current = thread_current();
  return current->pid;
}

int sys_dup(int oldfd) {
  thread_t* current = thread_current();
  fd_t* fd = thread_find_fd_id(current, oldfd);
  if (fd == NULL) {
    kprintf("dup not found fd %d\n", oldfd);
    return 0;
  }
  int newfd = thread_add_fd(current, fd);
#ifdef DEBUG_SYS_FN
  kprintf("sys dup %d %s\n", newfd, fd->name);
#endif
  return newfd;
}

int sys_dup2(int oldfd, int newfd) {
  thread_t* current = thread_current();
  fd_t* fd = thread_find_fd_id(current, oldfd);
  if (fd == NULL) {
    kprintf("dup not found fd %d\n", fd);
    return 0;
  }
  fd_t* nfd = thread_find_fd_id(current, newfd);
  if (nfd == NULL) {
    kprintf("dup not found nfd %d\n", nfd);
    return 0;
  }
  fd_close(fd);
  thread_set_fd(current, newfd, fd);
  return newfd;
}

int sys_readdir(int fd, int index, void* dirent) {
  thread_t* current = thread_current();
  fd_t* findfd = thread_find_fd_id(current, fd);
  if (fd == NULL) {
    kprintf("readdir not found fd %d\n", fd);
    return 0;
  }
  vdirent_t* d = vreaddir(findfd->data, index);
  if (d == NULL) {
    return 0;
  }
  kmemmove(dirent, d, sizeof(vdirent_t));
  kfree(d);
  return 1;
}

int sys_brk(int addr) {
  thread_t* current = thread_current();
  kprintf("sys sbrk tid:%x addr:%x\n", current->id, addr);

  vmemory_area_t* vm = vmemory_area_find_flag(current->vmm, MEMORY_HEAP);
  if (vm == NULL) {
    kprintf("sys brk not found vm\n");
    return 0;
  }
  if (addr == 0) {
    if (vm->vend == vm->vaddr) {
      vm->vend = vm->vaddr + addr;
    }
    addr = vm->vend;
    kprintf("sys sbrk return first addr:%x\n", addr);
    return addr;
  }
  vm->vend = addr;
  kprintf("sys sbrk return addr:%x\n", vm->vend);
  return vm->vend;
}

int sys_readv(int fd, iovec_t* vector, int count) {
  int ret = 0;
  int n;
  int i;
  for (i = 0; i < count; i++, vector++) {
    n = sys_read(fd, vector->iov_base, vector->iov_len);
    if (n < 0) return n;
    ret += n;
    if (n != vector->iov_len) break;
  }
  return ret;
}

int sys_writev(int fd, iovec_t* vector, int count) {
  int ret = 0;
  int n;
  int i;
  for (i = 0; i < count; i++, vector++) {
    n = sys_write(fd, vector->iov_base, vector->iov_len);
    if (n < 0) return n;
    ret += n;
    if (n != vector->iov_len) break;
  }
  return ret;
}

int sys_chdir(const char* path) {
  int ret = 0;
  kprintf("sys chdir not impl %s\n", path);
  return ret;
}

void* sys_mmap2(void* addr, int length, int prot, int flags, int fd,
                int pgoffset) {
  int ret = 0;
  ret=addr;
  if (fd >= 0 || pgoffset > 0) {
    kprintf("mmap2 system call : fd = %d, prot = %x, pgoffset = %d\n", fd, prot,
            pgoffset);
    return -1;
  }

  return ret;
}

int sys_mprotect(const void* start, size_t len, int prot) {
  int ret = 0;
  kprintf("sys mprotect not impl\n");

  return ret;
}

int sys_rt_sigprocmask(int h, void *set, void* old_set){
  kprintf("sys sigprocmask not impl\n");
  return 0;
}
