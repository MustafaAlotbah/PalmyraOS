

#include "palmyraOS/unistd.h"


uint32_t get_pid()
{
	uint32_t pid;
	asm("mov $20, %%eax\n\t"  // System call number for getpid (20)
		"int $0x80\n\t"       // Interrupt to trigger system call
		"mov %%eax, %0"
		: "=r" (pid)          // Output: store the result in pid
		:                     // No inputs
		: "%eax"              // Clobbered register
		);
	return pid;
}

void _exit(uint32_t exitCode)
{
	asm("mov $1, %%eax\n\t"   // System call number for exit (1)
		"mov %0, %%ebx\n\t"   // Load exitCode into ebx
		"int $0x80"           // Interrupt to trigger system call
		:                     // No outputs
		: "r" (exitCode)      // Input: exitCode
		: "%eax", "%ebx"      // Clobbered registers
		);
	// The function will not return as the process will be terminated.
}

int write(uint32_t fd, const void* buf, uint32_t count)
{
	int result;
	asm volatile(
		"int $0x80"            // Interrupt to trigger system call
		: "=a" (result)        // Output: store the result (number of bytes written or error code) in result
		:
		"a" (4),               // Input: system call number (4) in eax
		"b" (fd),              // Input: file descriptor in ebx
		"c" (buf),             // Input: buffer pointer in ecx
		"d" (count)            // Input: number of bytes to write in edx
		: "memory"             // Clobbered memory,
		// Tell the compiler, the assembly instruction will modify memory contents
		);
	return result;
}
