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

1. [ ] `int execve(const char *filename, char *const argv[], char *const envp[])`:
2. [ ] `void _exit(int status)`
3. [ ] `pid_t wait(int *status)`
4. [ ] `pid_t getpid(void)`
5. [ ] `pid_t getppid(void)`
6. [ ] `int kill(pid_t pid, int sig)`

7. [ ] `int open(const char *pathname, int flags, mode_t mode)`
8. [ ] `int close(int fd)`
9. [ ] `ssize_t read(int fd, void *buf, size_t count)`
10. [ ] `ssize_t write(int fd, const void *buf, size_t count)`
11. [ ] `off_t lseek(int fd, off_t offset, int whence)`
12. [ ] `int fstat(int fd, struct stat *statbuf)`

13. [ ] `int chdir(const char *path)`
14. [ ] `int mkdir(const char *pathname, mode_t mode)`
15. [ ] `int rmdir(const char *pathname)`
16. [ ] `int unlink(const char *pathname)`
17. [ ] `int rename(const char *oldpath, const char *newpath)`
18. [ ] `int chmod(const char *pathname, mode_t mode)`
19. [ ] `int chown(const char *pathname, uid_t owner, gid_t group)`

20. [ ] `int brk(void *addr)`
21. [ ] `void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)`
22. [ ] `int munmap(void *addr, size_t length)`
23. [ ] `int mprotect(void *addr, size_t len, int prot)`

24. [ ] `int pipe(int pipefd[2])`
25. [ ] `int dup(int oldfd)`
26. [ ] `int dup2(int oldfd, int newfd)`
27. [ ] `int socket(int domain, int type, int protocol)`
28. [ ] `int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)`
29. [ ] `int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)`
30. [ ] `int listen(int sockfd, int backlog)`
31. [ ] `int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)`
32. [ ] `ssize_t send(int sockfd, const void *buf, size_t len, int flags)`
33. [ ] `ssize_t recv(int sockfd, void *buf, size_t len, int flags)`

34. [ ] `time_t time(time_t *tloc)`
35. [ ] `int gettimeofday(struct timeval *tv, struct timezone *tz)`
36. [ ] `int nanosleep(const struct timespec *req, struct timespec *rem)`

37. [ ] `uid_t getuid(void)`
38. [ ] `uid_t geteuid(void)`
39. [ ] `gid_t getgid(void)`
40. [ ] `gid_t getegid(void)`
41. [ ] `int setuid(uid_t uid)`
42. [ ] `int setgid(gid_t gid)`
43. [ ] `char *getcwd(char *buf, size_t size)`
44. [ ] `int uname(struct utsname *buf)`
45. [ ] ``
46. [ ] ``
47. [ ] ``
48. [ ] ``













