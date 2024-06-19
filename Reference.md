# __Palmyra OS Reference__

## Initial Setup

### Project Structure
```
PalmyraOS/
├── CMakeLists.txt
├── Makefile
├── include/
│   └── boot/
│       └── multiboot.h
├── linker.ld
└── source/
    ├── boot/
    │   └── grub.cfg
    ├── bootloader.asm
    └── kernel.cpp
```

## Basic Concepts

1. CPU Registers

2. cdecl Conventions
    - Returning a value
    - Passing arguments


## Roadmap

1. [x] MultiBooting
2. [x] Ports
3. [x] Logger + (TSC/delay)
4. [x] Allocator + Simple KHeap Allocator
5. [x] VBE VESA (Graphics)
6. [x] Panic
7. [x] CPU-ID (extra)
8. [x] GDT
9. [x] PIC Manager (Hardware Interrupts)
10. [x] Interrupts
11. [x] Physical Memory
12. [x] Paging
13. [x] Heap Manager
14. [x] Multitasking
15. [x] User Space
16. [ ] SysCalls + (System API)
17. [ ] Interprocess Communication (IPC) pipes
18. [ ] Virtual File System (VFS)
19. [ ] Elf
20. [ ] FAT16
21.

- Keyboard driver
- Mouse driver
- FAT32
- NTFS
- User Authentication
- Userland
- System Calls
- Elf
- stdlib
- shell
- Networking (TCP, HTTP, HTTPs)
- Priority Scheduling
- Real time Scheduling
- GUI: Window Manager
- Shared Memory

# System Calls Roadmap

## (Partially) Done

1. [x] `pid_t getpid(void)`
2. [x] `void _exit(int status)`
3. [x] `ssize_t write(int fd, const void *buf, size_t count)`
4. [x] `void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)`
5. [x] `int sched_yield()`

## TODO

1. [ ] `int execve(const char *filename, char *const argv[], char *const envp[])`:

2. [ ] `pid_t wait(int *status)`
3. [ ] `pid_t getppid(void)`
4. [ ] `int kill(pid_t pid, int sig)`

5. [ ] `int open(const char *pathname, int flags, mode_t mode)`
6. [ ] `int close(int fd)`
7. [ ] `ssize_t read(int fd, void *buf, size_t count)`
8. [ ] `off_t lseek(int fd, off_t offset, int whence)`
9. [ ] `int fstat(int fd, struct stat *statbuf)`

10. [ ] `int chdir(const char *path)`
11. [ ] `int mkdir(const char *pathname, mode_t mode)`
12. [ ] `int rmdir(const char *pathname)`
13. [ ] `int unlink(const char *pathname)`
14. [ ] `int rename(const char *oldpath, const char *newpath)`
15. [ ] `int chmod(const char *pathname, mode_t mode)`
16. [ ] `int chown(const char *pathname, uid_t owner, gid_t group)`

17. [ ] `int brk(void *addr)`
18. [ ] `int munmap(void *addr, size_t length)`
19. [ ] `int mprotect(void *addr, size_t len, int prot)`

20. [ ] `int pipe(int pipefd[2])`
21. [ ] `int dup(int oldfd)`
22. [ ] `int dup2(int oldfd, int newfd)`
23. [ ] `int socket(int domain, int type, int protocol)`
24. [ ] `int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)`
25. [ ] `int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)`
26. [ ] `int listen(int sockfd, int backlog)`
27. [ ] `int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)`
28. [ ] `ssize_t send(int sockfd, const void *buf, size_t len, int flags)`
29. [ ] `ssize_t recv(int sockfd, void *buf, size_t len, int flags)`

30. [ ] `time_t time(time_t *tloc)`
31. [ ] `int gettimeofday(struct timeval *tv, struct timezone *tz)`
32. [ ] `int nanosleep(const struct timespec *req, struct timespec *rem)`

33. [ ] `uid_t getuid(void)`
34. [ ] `uid_t geteuid(void)`
35. [ ] `gid_t getgid(void)`
36. [ ] `gid_t getegid(void)`
37. [ ] `int setuid(uid_t uid)`
38. [ ] `int setgid(gid_t gid)`
39. [ ] `char *getcwd(char *buf, size_t size)`
40. [ ] `int uname(struct utsname *buf)`
41. [ ] ``
42. [ ] ``
43. [ ] ``
44. [ ] ``













