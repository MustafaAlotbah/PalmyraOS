

#include "palmyraOS/unistd.h"
#include "palmyraOS/time.h"
#include <cstdarg>
#include <cstddef>


uint32_t get_pid() {
    uint32_t pid;
    register uint32_t syscall_no asm("eax") = POSIX_INT_GET_PID;
    asm volatile("int $0x80\n\t"  // Interrupt to trigger system call
                 "mov %%eax, %0"
                 : "=r"(pid)        // Output: store the result in pid
                 : "r"(syscall_no)  // Input: system call number
                 : "memory"         // Clobbered register
    );
    return pid;
}

void _exit(uint32_t exitCode) {
    register uint32_t syscall_no asm("eax") = POSIX_INT_EXIT;
    register uint32_t code asm("ebx")       = exitCode;
    asm volatile("int $0x80"                   // Interrupt to trigger system call
                 :                             // No outputs
                 : "r"(syscall_no), "r"(code)  // Inputs
                 : "memory"                    // Clobbered memory
    );
    // The function will not return as the process will be terminated.
}

int write(uint32_t fd, const void* buf, uint32_t count) {
    int result;

    register uint32_t syscall_no asm("eax") = POSIX_INT_WRITE;
    register uint32_t file_desc asm("ebx")  = fd;
    register const void* buffer asm("ecx")  = buf;
    register uint32_t byte_count asm("edx") = count;

    asm volatile("int $0x80"                                                      // Interrupt to trigger system call
                 : "=a"(result)                                                   // Output: store the result (number of bytes written or error code) in result
                 : "r"(syscall_no), "r"(file_desc), "r"(buffer), "r"(byte_count)  // Inputs
                 : "memory"                                                       // Clobbered memory
    );
    return result;
}

void* mmap(void* addr, uint32_t length, int prot, int flags, int fd, uint32_t offset) {
    void* result;

    register int syscall_no asm("eax")    = POSIX_INT_MMAP;
    register auto addr_reg asm("ebx")     = reinterpret_cast<uint32_t>(addr);
    register size_t length_reg asm("ecx") = length;
    register int prot_reg asm("edx")      = prot;
    register int flags_reg asm("esi")     = flags;
    register int fd_reg asm("edi")        = fd;

    asm volatile("int $0x80" : "=a"(result) : "r"(syscall_no), "r"(addr_reg), "r"(length_reg), "r"(prot_reg), "r"(flags_reg), "r"(fd_reg), "m"(offset) : "memory");

    return result;
}

void closeWindow(uint32_t windowID) {
    register uint32_t syscall_no asm("eax") = INT_CLOSE_WINDOW;
    register uint32_t windowId asm("ebx")   = windowID;

    // TODO
    asm volatile("int $0x80"                       // Software interrupt to trigger the syscall
                 :                                 // No output operands
                 : "r"(syscall_no), "r"(windowId)  // Input operands
                 : "memory"                        // Clobbered registers
    );
}

KeyboardEvent nextKeyboardEvent(uint32_t windowID) {
    register uint32_t syscall_no asm("eax") = INT_NEXT_KEY_EVENT;
    register uint32_t windowId asm("ebx")   = windowID;

    KeyboardEvent event;
    register uint32_t eventPtr asm("ecx") = reinterpret_cast<uint32_t>(&event);

    // TODO
    asm volatile("int $0x80"  // Software interrupt to trigger the syscall
                 :            // No output operands
                 : "a"(syscall_no),
                   "b"(windowId),
                   "c"(eventPtr)
                 : "memory"  // Clobbered registers
    );

    return event;
}

MouseEvent nextMouseEvent(uint32_t windowID) {
    register uint32_t syscall_no asm("eax") = INT_NEXT_MOUSE_EVENT;
    register uint32_t windowId asm("ebx")   = windowID;

    MouseEvent event;
    register uint32_t eventPtr asm("ecx") = reinterpret_cast<uint32_t>(&event);

    // TODO
    asm volatile("int $0x80"  // Software interrupt to trigger the syscall
                 :            // No output operands
                 : "a"(syscall_no),
                   "b"(windowId),
                   "c"(eventPtr)
                 : "memory"  // Clobbered registers
    );

    return event;
}

palmyra_window_status getStatus(uint32_t windowID) {
    register uint32_t syscall_no asm("eax") = INT_GET_WINDOW_STATUS;
    register uint32_t windowId asm("ebx")   = windowID;

    palmyra_window_status status;
    register uint32_t eventPtr asm("ecx") = reinterpret_cast<uint32_t>(&status);

    // TODO
    asm volatile("int $0x80"  // Software interrupt to trigger the syscall
                 :            // No output operands
                 : "a"(syscall_no),
                   "b"(windowId),
                   "c"(eventPtr)
                 : "memory"  // Clobbered registers
    );

    return status;
}

int sched_yield() {
    int result;
    register uint32_t syscall_no asm("eax") = POSIX_INT_YIELD;
    asm volatile("int $0x80\n\t"  // Interrupt to trigger system call
                 "mov %%eax, %0"
                 : "=r"(result)     // Output: store the result in result
                 : "r"(syscall_no)  // Input
                 : "memory"         // Clobbered register
    );
    return result;
}

int clock_gettime(uint32_t clk_id, struct timespec* tp) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(POSIX_INT_GETTIME), "D"(clk_id), "S"(tp) : "memory");
    return ret;
}

int open(const char* pathname, int flags) {
    int fd;
    asm volatile("int $0x80" : "=a"(fd) : "a"(POSIX_INT_OPEN), "b"(pathname), "c"(flags) : "memory");
    return fd;
}

int close(uint32_t fd) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(POSIX_INT_CLOSE), "b"(fd) : "memory");
    return ret;
}

int ioctl(uint32_t fd, uint32_t request, ...) {
    va_list args;
    va_start(args, request);
    void* argp = va_arg(args, void*);  // extract just the first argument (for now)
    va_end(args);

    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(POSIX_INT_IOCTL), "b"(fd), "c"(request), "d"(argp) : "memory");
    return ret;
}

int read(uint32_t fileDescriptor, void* buffer, uint32_t count) {
    int result;

    register uint32_t syscall_no asm("eax") = POSIX_INT_READ;
    register uint32_t fd asm("ebx")         = fileDescriptor;
    register void* buf asm("ecx")           = buffer;
    register uint32_t n asm("edx")          = count;

    asm volatile("int $0x80"                                   // Interrupt to trigger system call
                 : "=a"(result)                                // Output: store the result (number of bytes read or error code) in result
                 : "r"(syscall_no), "r"(fd), "r"(buf), "r"(n)  // Inputs
                 : "memory"                                    // Clobbered memory
    );
    return result;
}

int getdents(unsigned int fileDescriptor, linux_dirent* dirp, unsigned int count) {
    int result;

    register uint32_t syscall_no asm("eax") = LINUX_INT_GETDENTS;
    register uint32_t fd asm("ebx")         = fileDescriptor;
    register void* buf asm("ecx")           = dirp;
    register uint32_t n asm("edx")          = count;

    asm volatile("int $0x80" : "=a"(result) : "r"(syscall_no), "r"(fd), "r"(buf), "r"(n) : "memory");
    return result;
}

int32_t lseek(uint32_t fd, int32_t offset, int whence) {
    int32_t result;

    // Prepare the registers for the system call
    register uint32_t syscall_no asm("eax") = POSIX_INT_LSEEK;
    register uint32_t file_desc asm("ebx")  = fd;
    register int32_t offset_val asm("ecx")  = offset;
    register int whence_val asm("edx")      = whence;

    // Perform the system call using inline assembly
    asm volatile("int $0x80"                                                          // Interrupt to trigger system call
                 : "=a"(result)                                                       // Output: store the result (new offset or error code) in result
                 : "r"(syscall_no), "r"(file_desc), "r"(offset_val), "r"(whence_val)  // Inputs
                 : "memory"                                                           // Clobbered memory
    );

    return result;  // Return the new file offset or -1 if an error occurred
}

int posix_spawn(uint32_t* pid, const char* path, void* file_actions, void* attrp, char* const* argv, char* const* envp) {
    int result;

    register uint32_t syscall_no asm("eax")   = POSIX_INT_POSIX_SPAWN;
    register uint32_t pid_reg asm("ebx")      = reinterpret_cast<uint32_t>(pid);
    register const char* path_reg asm("ecx")  = path;
    register char* const* argv_reg asm("edx") = argv;
    register char* const* envp_reg asm("esi") = envp;

    asm volatile("int $0x80" : "=a"(result) : "r"(syscall_no), "r"(pid_reg), "r"(path_reg), "r"(argv_reg), "r"(envp_reg) : "memory");

    return result;
}

uint32_t waitpid(uint32_t pid, int* status, int options) {
    uint32_t ret;

    register uint32_t syscall_no asm("eax") = POSIX_INT_WAITPID;
    register uint32_t pid_reg asm("ebx")    = pid;
    register int* status_reg asm("ecx")     = status;
    register int options_reg asm("edx")     = options;

    asm volatile("int $0x80" : "=a"(ret) : "r"(syscall_no), "r"(pid_reg), "r"(status_reg), "r"(options_reg) : "memory");

    return ret;
}

int clock_nanosleep(uint32_t clock_id, int flags, const struct timespec* req, struct timespec* rem) {
    int result;
    register uint32_t syscall_no asm("eax")            = POSIX_INT_CLOCK_NANOSLEEP_64;
    register uint32_t clk_id asm("ebx")                = clock_id;
    register int flg asm("ecx")                        = flags;
    register const struct timespec* req_ptr asm("edx") = req;
    register struct timespec* rem_ptr asm("esi")       = rem;

    asm volatile("int $0x80"                                                           // Software interrupt to trigger the syscall
                 : "=a"(result)                                                        // Output: store the result in result
                 : "r"(syscall_no), "r"(clk_id), "r"(flg), "r"(req_ptr), "r"(rem_ptr)  // Inputs
                 : "memory"                                                            // Clobbered memory
    );

    return result;
}

int arch_prctl(int code, unsigned long addr) {
    int result;

    register int syscall_no asm("eax")     = LINUX_INT_PRCTL;
    register int arg1 asm("ebx")           = code;  // First argument (operation code)
    register unsigned long arg2 asm("ecx") = addr;  // Second argument (address or value)

    asm volatile("int $0x80"                              // Trigger syscall interrupt
                 : "=a"(result)                           // Output the result (return value)
                 : "r"(syscall_no), "r"(arg1), "r"(arg2)  // Inputs
                 : "memory");

    return result;
}

int brk(void* end_data_segment) {
    int result;

    // Register the system call number for brk()
    register uint32_t syscall_no asm("eax") = POSIX_INT_BRK;

    // Pass the new program break (end of the data segment) as an argument
    register void* new_end asm("ebx")       = end_data_segment;

    // Perform the system call using inline assembly
    asm volatile("int $0x80"                      // Trigger interrupt 0x80 (system call entry point)
                 : "=a"(result)                   // Output: store the result of the system call
                 : "r"(syscall_no), "r"(new_end)  // Input: system call number and new program break
                 : "memory"                       // Clobber: memory might be affected
    );

    // Return the result: 0 on success, -1 on failure
    return result;
}

uint32_t getuid() {
    uint32_t uid;
    register uint32_t syscall_no asm("eax") = POSIX_INT_GETUID;
    asm volatile("int $0x80\n\t"  // Trigger the system call
                 "mov %%eax, %0"
                 : "=r"(uid)        // Output: store the result in uid
                 : "r"(syscall_no)  // Input: system call number
                 : "memory"         // Clobber: memory might be affected
    );
    return uid;
}

uint32_t getgid() {
    uint32_t gid;
    register uint32_t syscall_no asm("eax") = POSIX_INT_GETGID;
    asm volatile("int $0x80\n\t"  // Trigger the system call
                 "mov %%eax, %0"
                 : "=r"(gid)        // Output: store the result in gid
                 : "r"(syscall_no)  // Input: system call number
                 : "memory"         // Clobber: memory might be affected
    );
    return gid;
}

uint32_t geteuid32() {
    uint32_t euid;
    register uint32_t syscall_no asm("eax") = POSIX_INT_GETEUID32;
    asm volatile("int $0x80\n\t"  // Trigger the system call
                 "mov %%eax, %0"
                 : "=r"(euid)       // Output: store the result in euid
                 : "r"(syscall_no)  // Input: system call number
                 : "memory"         // Clobber: memory might be affected
    );
    return euid;
}

uint32_t getegid32() {
    uint32_t egid;
    register uint32_t syscall_no asm("eax") = POSIX_INT_GETEGID32;
    asm volatile("int $0x80\n\t"  // Trigger the system call
                 "mov %%eax, %0"
                 : "=r"(egid)       // Output: store the result in egid
                 : "r"(syscall_no)  // Input: system call number
                 : "memory"         // Clobber: memory might be affected
    );
    return egid;
}

uint32_t initializeWindow(uint32_t** buffer, palmyra_window* palmyraWindow) {
    uint32_t result;

    // Prepare the registers for the system call
    uint32_t eax = INT_INIT_WINDOW;                            // System call number for initializeWindow2
    auto ebx     = reinterpret_cast<uint32_t>(buffer);         // Address of buffer
    uint32_t ecx = reinterpret_cast<uint32_t>(palmyraWindow);  // Address of palmyra_window structure

    // Perform the system call using inline assembly
    asm volatile("int $0x80"     // Trigger interrupt 0x80
                 : "=a"(result)  // Output: store result in 'result' from 'eax'
                 : "a"(eax),     // Input: system call number
                   "b"(ebx),     // Input: buffer
                   "c"(ecx)      // Input: palmyra_window structure
                 : "memory"      // Clobber: memory might be affected
    );

    return result;  // Return the result of the system call (window ID)
}

int mkdir(const char* pathname, uint16_t mode) {
    int result;
    register uint32_t syscall_no asm("eax")  = POSIX_INT_MKDIR;
    register const char* path_reg asm("ebx") = pathname;
    register uint16_t mode_reg asm("ecx")    = mode;

    asm volatile("int $0x80" : "=a"(result) : "r"(syscall_no), "r"(path_reg), "r"(mode_reg) : "memory");
    return result;
}

int unlink(const char* pathname) {
    int result;
    register uint32_t syscall_no asm("eax")  = POSIX_INT_UNLINK;
    register const char* path_reg asm("ebx") = pathname;

    asm volatile("int $0x80" : "=a"(result) : "r"(syscall_no), "r"(path_reg) : "memory");
    return result;
}

int rmdir(const char* pathname) {
    int result;
    register uint32_t syscall_no asm("eax")  = POSIX_INT_RMDIR;
    register const char* path_reg asm("ebx") = pathname;

    asm volatile("int $0x80" : "=a"(result) : "r"(syscall_no), "r"(path_reg) : "memory");
    return result;
}
