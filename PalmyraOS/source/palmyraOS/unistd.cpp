

#include "palmyraOS/unistd.h"
#include "palmyraOS/time.h"
#include <cstddef>
#include <cstdarg>


uint32_t get_pid()
{
	uint32_t          pid;
	register uint32_t syscall_no asm("eax") = POSIX_INT_GET_PID;
	asm volatile(
		"int $0x80\n\t"       // Interrupt to trigger system call
		"mov %%eax, %0"
		: "=r" (pid)          // Output: store the result in pid
		: "r" (syscall_no)    // Input: system call number
		: "memory"              // Clobbered register
		);
	return pid;
}

void _exit(uint32_t exitCode)
{
	register uint32_t syscall_no asm("eax") = POSIX_INT_EXIT;
	register uint32_t code asm("ebx")       = exitCode;
	asm volatile(
		"int $0x80"           // Interrupt to trigger system call
		:                     // No outputs
		: "r" (syscall_no), "r" (code) // Inputs
		: "memory"            // Clobbered memory
		);
	// The function will not return as the process will be terminated.
}

int write(uint32_t fd, const void* buf, uint32_t count)
{
	int result;

	register uint32_t syscall_no asm("eax") = POSIX_INT_WRITE;
	register uint32_t file_desc asm("ebx")  = fd;
	register const void* buffer asm("ecx") = buf;
	register uint32_t byte_count asm("edx") = count;

	asm volatile(
		"int $0x80"            // Interrupt to trigger system call
		: "=a" (result)        // Output: store the result (number of bytes written or error code) in result
		: "r" (syscall_no), "r" (file_desc), "r" (buffer), "r" (byte_count) // Inputs
		: "memory"             // Clobbered memory
		);
	return result;
}

void* mmap(void* addr, uint32_t length, int prot, int flags, int fd, uint32_t offset)
{
	void* result;

	register int    syscall_no asm("eax") = POSIX_INT_MMAP;
	register auto   addr_reg asm("ebx")   = reinterpret_cast<uint32_t>(addr);
	register size_t length_reg asm("ecx") = length;
	register int    prot_reg asm("edx")   = prot;
	register int    flags_reg asm("esi")  = flags;
	register int    fd_reg asm("edi")     = fd;

	asm volatile(
		"int $0x80"
		: "=a" (result)
		: "r" (syscall_no),
	"r" (addr_reg),
	"r" (length_reg),
	"r" (prot_reg),
	"r" (flags_reg),
	"r" (fd_reg),
	"m" (offset)
		: "memory"
		);

	return result;
}

uint32_t initializeWindow(uint32_t** buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	uint32_t result;
	// Prepare the registers for the system call
	uint32_t eax = INT_INIT_WINDOW; // System call number
	auto     ebx = reinterpret_cast<uint32_t>(buffer);
	uint32_t ecx = x;
	uint32_t edx = y;
	uint32_t esi = width;
	uint32_t edi = height;

	// Perform the system call using inline assembly
	__asm__ volatile (
		"int $0x80"         // Trigger interrupt 0x80
		: "=a"(result)      // Output: store result in 'result' from 'eax'
		:
		"a"(eax),         // Input: system call number
		"b"(ebx),         // Input: first parameter (buffer)
		"c"(ecx),         // Input: second parameter (x)
		"d"(edx),         // Input: third parameter (y)
		"S"(esi),         // Input: fourth parameter (width)
		"D"(edi)          // Input: fifth parameter (height)
		: "memory"          // Clobber: memory might be affected
		);

	return result;
}

void closeWindow(uint32_t windowID)
{
	register uint32_t syscall_no asm("eax") = INT_CLOSE_WINDOW;
	register uint32_t windowId asm("ebx")   = windowID;

	// TODO
	asm volatile(
		"int $0x80"  // Software interrupt to trigger the syscall
		:            // No output operands
		: "r"(syscall_no), "r"(windowId)  // Input operands
		: "memory"   // Clobbered registers
		);

}

int sched_yield()
{
	int               result;
	register uint32_t syscall_no asm("eax") = POSIX_INT_YIELD;
	asm volatile(
		"int $0x80\n\t"        // Interrupt to trigger system call
		"mov %%eax, %0"
		: "=r" (result)        // Output: store the result in result
		: "r" (syscall_no)     // Input
		: "memory"               // Clobbered register
		);
	return result;
}

int clock_gettime(uint32_t clk_id, struct timespec* tp)
{
	int ret;
	asm volatile (
		"int $0x80"
		: "=a" (ret)
		: "a" (POSIX_INT_GETTIME), "D" (clk_id), "S" (tp)
		: "memory"
		);
	return ret;
}

uint32_t open(const char* pathname, int flags)
{
	int fd;
	asm volatile (
		"int $0x80"
		: "=a" (fd)
		: "a" (POSIX_INT_OPEN), "b" (pathname), "c" (flags)
		: "memory"
		);
	return fd;
}

int close(uint32_t fd)
{
	int ret;
	asm volatile (
		"int $0x80"
		: "=a" (ret)
		: "a" (POSIX_INT_CLOSE), "b" (fd)
		: "memory"
		);
	return ret;
}

int ioctl(uint32_t fd, uint32_t request, ...)
{
	va_list args;
	va_start(args, request);
	void* argp = va_arg(args, void*); // extract just the first argument (for now)
	va_end(args);

	int ret;
	asm volatile (
		"int $0x80"
		: "=a" (ret)
		: "a" (POSIX_INT_IOCTL), "b" (fd), "c" (request), "d" (argp)
		: "memory"
		);
	return ret;
}

int read(uint32_t fileDescriptor, void* buffer, uint32_t count)
{
	int result;

	register uint32_t syscall_no asm("eax") = POSIX_INT_READ;
	register uint32_t fd asm("ebx")         = fileDescriptor;
	register void* buf asm("ecx") = buffer;
	register uint32_t n asm("edx") = count;

	asm volatile(
		"int $0x80"            // Interrupt to trigger system call
		: "=a" (result)        // Output: store the result (number of bytes read or error code) in result
		: "r" (syscall_no), "r" (fd), "r" (buf), "r" (n) // Inputs
		: "memory"             // Clobbered memory
		);
	return result;
}


