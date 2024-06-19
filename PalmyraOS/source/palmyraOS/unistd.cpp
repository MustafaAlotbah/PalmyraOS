

#include "palmyraOS/unistd.h"
#include "palmyraOS/time.h"
#include <cstddef>


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
		:
		"a" (4),               // Input: system call number (4) in eax
		"b" (fd),              // Input: file descriptor in ebx
		"c" (buf),             // Input: buffer pointer in ecx
		"d" (count)            // Input: number of bytes to write in edx
		: "memory"             // Clobbered memory,
		// Tell the compiler, the assembly instruction will modify memory contents

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

