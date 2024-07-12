# __Palmyra OS Reference__

## Coding Guideline

1. **Avoid Using Function Pointers**:

   Prefer virtual classes and polymorphism over pointers to functions.

2. **Adopt the Scandinavian Style (Meyers Style)**:

   n class definitions, declare member functions before member attributes. End attribute names with an underscore `_`.

    ```cpp
    class Example {
    public:
        Example(int value);
        void doSomething();
        int getValue() const;

    private:
        int value_;
    };
    ```

3. **Use Descriptive Naming (Self-Documenting Code)**:

   Use whole words instead of abbreviations for variable and function names.

   Example: Prefer `fileSystem_` over `fs_`.

4. **Separate Headers from Implementation**:

5. **Embrace Modern C++ Practices**:

   For example, use `std::vector`, `std::map` and `std::algorithm`

6. **Utilize Guard Clauses (Early Exit Pattern)**

   Check for edge cases early in the function to simplify the main logic and improve readability. This helps to handle
   special conditions or errors upfront, reducing nested conditions and clarifying the flow of the code.

   You may also use **Goto Cleanup**

7. **Use Raw Pointers**:

   Prefer using `Example*` to `std::shared_pointer<Example>`

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
16. [x] SysCalls + (System API)
17. [x] Virtual File System (VFS)
18. [x] Keyboard driver
19. [ ] Terminal
20. [ ] Interprocess Communication (IPC) pipes
21. [ ] FAT16
22. [ ] Elf
23. [ ] FAT32
24. [ ] Terminal Commands
25. [ ] Mouse driver

**Nice to haves**
- User Authentication
- Networking (TCP, HTTP, HTTPs)
- Real time Scheduling
- Multi-level Feedback Queue Scheduling (MLFQ)
- Shared Memory

## SysCalls Roadmap

**Processes**

- [x] `uint32_t get_pid()`
- [x] `_exit(uint32_t exitCode)`
- [x] `int sched_yield()`
- [x] `void* mmap(void* addr, uint32_t length, int prot, int flags, int fd, uint32_t offset)`

**Windows (palmyraOS-specific)**

- [x] `uint32_t initializeWindow(uint32_t** buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height)`
- [x] `void closeWindow(uint32_t windowID)`
- [x] `KeyboardEvent nextKeyboardEvent(uint32_t windowID)`
- [ ] `MouseEvent nextMouseEvent(uint32_t windowID)`

**VFS API**

- [x] `int open(const char* pathname, int flags)`
- [x] `int close(uint32_t fd);`
- [x] `int write(uint32_t fileDescriptor, const void* buffer, uint32_t count)`
- [x] `int read(uint32_t fileDescriptor, void* buffer, uint32_t count)`
- [x] `int ioctl(uint32_t fd, uint32_t request, ...)`
- [x] `int getdents(unsigned int fd, linux_dirent* dirp, unsigned int count)`

**Required For Execution**

- [ ] `int pipe(int pipefd[2])`
- [ ] `int fork()`
- [ ] `int dup(int oldfd)`
- [ ] `int dup2(int oldfd, int newfd)`
- [ ] `int execve(const char *filename, char *const argv[], char *const envp[])`
- [ ] `int waitpid (__pid_t __pid, int *__stat_loc, int __options)`
- [ ] ``

## VFS Roadmap

### Device

- [x] `/dev/`
- [x] `/dev/rtc`
- [ ] `/dev/null`
- [ ] `/dev/zero`
- [ ] `/dev/random`
- [ ] `/dev/full`
- [ ] `/dev/tty`
- [ ] `/dev/nullb0`
- [ ] `/dev/shm/` *Shared Memory*
- [ ] `/dev/stdin`, `/dev/fb/0`
- [ ] `/dev/stdout`, `/dev/fb/1`
- [ ] `/dev/stderr`, `/dev/fb/2`
- [ ] ``

## Terminal Roadmap

- [x] echo
- [x] clear
- [x] cat
- [x] ls
- [ ] pwd
- [ ] cd
- [ ] head
- [ ] tail
- [ ] uname
- [ ] ps aux
- [ ] free
- [ ] lsblk
- [ ] lscpu
- [ ] uptime
- [ ] exit
- [ ] chmod

**FAT32/...**

- [ ] mkdir
- [ ] rmdir
- [ ] touch
- [ ] cp

# Security Issues

- [x] if a process passes pointer to a syscall,
  the kernel might access an invalid pointer,
  or access a different process space.
  Hence, we must always check that the pointer is valid!
-

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
3. [x] `pid_t getppid(void)`
4. [ ] `int kill(pid_t pid, int sig)`

5. [x] `int open(const char *pathname, int flags, mode_t mode)`
6. [x] `int close(int fd)`
7. [x] `ssize_t read(int fd, void *buf, size_t count)`
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













