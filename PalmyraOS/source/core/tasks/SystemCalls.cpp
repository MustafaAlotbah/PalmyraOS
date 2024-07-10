

#include "core/tasks/SystemCalls.h"
#include "libs/memory.h"

// API Headers
#include "palmyraOS/unistd.h"
#include "palmyraOS/time.h"
#include "palmyraOS/errono.h"

// System Objects
#include "core/tasks/ProcessManager.h"
#include "core/tasks/WindowManager.h"
#include "core/SystemClock.h"
#include "core/files/VirtualFileSystem.h"


// Define the static member systemCallHandlers_
PalmyraOS::kernel::KMap<uint32_t, PalmyraOS::kernel::SystemCallsManager::SystemCallHandler>
	PalmyraOS::kernel::SystemCallsManager::systemCallHandlers_;

void PalmyraOS::kernel::SystemCallsManager::initialize()
{
	// Setting the interrupt handler for system calls (interrupt 0x80)
	interrupts::InterruptController::setInterruptHandler(0x80, &handleInterrupt);

	// Map system call numbers to their respective handler functions
	// POSIX
	systemCallHandlers_[POSIX_INT_EXIT]    = &SystemCallsManager::handleExit;
	systemCallHandlers_[POSIX_INT_GET_PID] = &SystemCallsManager::handleGetPid;
	systemCallHandlers_[POSIX_INT_YIELD]   = &SystemCallsManager::handleYield;
	systemCallHandlers_[POSIX_INT_MMAP]    = &SystemCallsManager::handleMmap;
	systemCallHandlers_[POSIX_INT_GETTIME] = &SystemCallsManager::handleGetTime;
	systemCallHandlers_[POSIX_INT_OPEN]    = &SystemCallsManager::handleOpen;
	systemCallHandlers_[POSIX_INT_CLOSE]   = &SystemCallsManager::handleClose;
	systemCallHandlers_[POSIX_INT_WRITE]   = &SystemCallsManager::handleWrite;
	systemCallHandlers_[POSIX_INT_READ]    = &SystemCallsManager::handleRead;
	systemCallHandlers_[POSIX_INT_IOCTL]   = &SystemCallsManager::handleIoctl;

	// Custom
	systemCallHandlers_[INT_INIT_WINDOW]   = &SystemCallsManager::handleInitWindow;
	systemCallHandlers_[INT_CLOSE_WINDOW]  = &SystemCallsManager::handleCloseWindow;
	systemCallHandlers_[INT_NEXT_KEY_EVENT] = &SystemCallsManager::handleNextKeyboardEvent;

	// Adopted from Linux
	systemCallHandlers_[LINUX_INT_GETDENTS] = &SystemCallsManager::handleGetdents;

}

bool PalmyraOS::kernel::SystemCallsManager::isValidAddress(void* addr)
{
	// Validate if the given address is within the current process's valid address range

	// Retrieve the current process
	auto* proc = TaskManager::getCurrentProcess();

	// Check if the address is valid in the current process's paging directory
	if (!proc->pagingDirectory_->isAddressValid(addr))
	{
		// If the address is invalid, terminate the process with a BAD ADDRESS error code
		proc->terminate(-EFAULT);
		return false;
	}
	return true;
}

uint32_t* PalmyraOS::kernel::SystemCallsManager::handleInterrupt(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// Find the appropriate system call handler based on the syscall number in regs->eax
	auto it = systemCallHandlers_.find(regs->eax);
	if (it != systemCallHandlers_.end())
	{
		// Call the handler function
		it->second(regs);
	}

	// Retrieve the current process
	auto* proc = TaskManager::getCurrentProcess();
	bool condition_01 = proc->getState() == Process::State::Terminated;    // unexpected / exit
	bool condition_02 = proc->age_ == 0;                                   // yield

	// Return to the appropriate interrupt handler based on the process state
	if (condition_01 || condition_02)
		return TaskManager::interruptHandler(regs);
	else
		return (uint32_t*)(regs);
}

void PalmyraOS::kernel::SystemCallsManager::handleExit(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// void _exit(int)
	TaskManager::getCurrentProcess()->terminate((int)regs->ebx);
}

void PalmyraOS::kernel::SystemCallsManager::handleGetPid(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// uint32_t getpid()
	regs->eax = TaskManager::getCurrentProcess()->getPid();
}

void PalmyraOS::kernel::SystemCallsManager::handleYield(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// int sched_yield();

	regs->eax                              = 0;    // Return 0 to indicate success
	TaskManager::getCurrentProcess()->age_ = 0;    // Reset the age to yield the CPU
}

void PalmyraOS::kernel::SystemCallsManager::handleMmap(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// void* mmap(void* addr, uint32_t length, int prot, int flags, int fd, uint32_t offset)

	// Extract arguments from registers
	void* addr = (void*)regs->ebx;    // TODO: currently ignored
	uint32_t length          = regs->ecx;
	uint32_t protectionFlags = regs->edx;    // TODO, offset
	uint32_t flags           = regs->esi;
	uint32_t fd_reg          = regs->edi;

	// Check if addr is a valid pointer
//	if (!isValidAddress(addr)) return;

	// Allocate memory pages for the current process based on the requested length
	void* allocatedAddr = TaskManager::getCurrentProcess()->allocatePages((length >> 12) + 1);

	// Set eax to the allocated address or MAP_FAILED
	if (allocatedAddr != nullptr)
	{
		regs->eax = (uint32_t)allocatedAddr;
	}
	else
	{
		regs->eax = (uint32_t)MAP_FAILED;
	}
}

void PalmyraOS::kernel::SystemCallsManager::handleGetTime(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// int clock_gettime(uint32_t clk_id, struct timespec *tp)

	// Extract arguments from registers
	uint32_t clockId = regs->edi;
	auto* timeSpec = (timespec*)regs->esi;

	// Check if timeSpec is a valid pointer
	if (!isValidAddress(timeSpec)) return;

	// Get the current time in ticks and frequency
	uint64_t ticks     = SystemClock::getTicks();
	uint64_t frequency = SystemClockFrequency;

	// Convert ticks to seconds and nanoseconds
	timeSpec->tv_nsec = (ticks * 1'000'000) / frequency;
	timeSpec->tv_sec  = timeSpec->tv_nsec / 1'000'000;

	// Set eax to 0 to indicate success
	regs->eax = 0;
}

void PalmyraOS::kernel::SystemCallsManager::handleOpen(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// int open(const char *pathname, int flags)

	// Extract arguments from registers
	char* pathname = (char*)regs->ebx;
	int flags = static_cast<int>(regs->ecx);

	// Check if pathname is a valid pointer
	if (!isValidAddress(pathname)) return;

	// Get the inode associated with the given pathname
	auto inode = vfs::VirtualFileSystem::getInodeByPath(KString(pathname));
	if (!inode)
	{
		// If the inode does not exist, set eax to -1 to indicate failure
		regs->eax = -1;
		return;
	}

	// Allocate a file descriptor for the inode
	fd_t fileDescriptor = TaskManager::getCurrentProcess()->fileTableDescriptor_.allocate(inode, flags);

	// Set eax to the allocated file descriptor
	regs->eax = fileDescriptor;
}

void PalmyraOS::kernel::SystemCallsManager::handleClose(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// int close(int fd)

	// Extract arguments from registers
	uint32_t fd = regs->ebx;

	// Release the file descriptor
	TaskManager::getCurrentProcess()->fileTableDescriptor_.release(fd);

	// Set eax to 0 to indicate success
	regs->eax = 0;
}

void PalmyraOS::kernel::SystemCallsManager::handleWrite(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// int write(uint32_t fd, const void *buf, uint32_t count)

	// Extract arguments from registers
	size_t fileDescriptor = regs->ebx;
	char* bufferPointer = (char*)regs->ecx;
	size_t size = regs->edx;

	// Check if bufferPointer is a valid pointer
	if (!isValidAddress(bufferPointer)) return;

	// Get the current process
	auto* proc = TaskManager::getCurrentProcess();

	// TODO move to actual
	// TODO 0

	// Handle writing to stdout (file descriptor 1) and stderr (file descriptor 2)
	if (fileDescriptor == 1)
	{
		// Initialize the number of bytes written to 0
		regs->eax = 0;
		for (size_t i = 0; i < size; ++i)
		{
			if (bufferPointer[i] == '\0') break;         // Stop at null terminator
			proc->stdout_.push_back(bufferPointer[i]);   // Write to stdout
			regs->eax++;                                 // Increment the byte count
		}
	}
	else if (fileDescriptor == 2)
	{
		// Initialize the number of bytes written to 0
		regs->eax = 0;
		for (size_t i = 0; i < size; ++i)
		{
			if (bufferPointer[i] == '\0') break;         // Stop at null terminator
			proc->stderr_.push_back(bufferPointer[i]);   // Write to stdout
			regs->eax++;                                 // Increment the byte count
		}
	}
	else
	{
		// Handle writing to regular files
		auto file = proc->fileTableDescriptor_.getOpenFile(fileDescriptor);
		if (!file)
		{
			// If the file is not open, set the number of bytes written to 0
			regs->eax = 0;    // we wrote 0 bytes
			return;
		}

		// Write data to the file and update the file offset
		auto bytesRead = file->getInode()->write(bufferPointer, size, file->getOffset());
		file->advanceOffset(bytesRead);

		// Set eax to the number of bytes written
		regs->eax = bytesRead;
	}
}

void PalmyraOS::kernel::SystemCallsManager::handleRead(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// int read(uint32_t fileDescriptor, void* buffer, uint32_t count);

	// Extract arguments from registers
	size_t fileDescriptor = regs->ebx;
	char* bufferPointer = (char*)regs->ecx;
	size_t size = regs->edx;

	// Check if bufferPointer is a valid pointer
	if (!isValidAddress(bufferPointer)) return;

	// Get the file associated with the file descriptor
	auto file = TaskManager::getCurrentProcess()->fileTableDescriptor_.getOpenFile(fileDescriptor);
	if (!file)
	{
		// If the file is not open, set the number of bytes read to 0
		regs->eax = 0; // we read 0 bytes
		return;
	}

	// Read data from the file and update the file offset
	auto bytesRead = file->getInode()->read(bufferPointer, size, file->getOffset());
	file->advanceOffset(bytesRead);

	// Set eax to the number of bytes read
	regs->eax = bytesRead;
}

void PalmyraOS::kernel::SystemCallsManager::handleIoctl(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// Extract arguments from registers
	auto fileDescriptor = regs->ebx;
	auto request        = static_cast<int>(regs->ecx);
	auto argp           = (void*)regs->edx;

	// Check if argp is a valid pointer
	if (!isValidAddress(argp)) return;

	// Get the current process
	auto* proc = TaskManager::getCurrentProcess();

	// Get the file associated with the file descriptor
	auto file = proc->fileTableDescriptor_.getOpenFile(fileDescriptor);
	if (!file)
	{
		// If the file is not open, set the number of bytes read to 0
		regs->eax = 0;
		return;
	}

	// Perform the IOCTL operation
	auto status = file->getInode()->ioctl(request, argp);
	regs->eax = status;
}

void PalmyraOS::kernel::SystemCallsManager::handleInitWindow(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// Extract arguments from registers
	auto** userBuffer = (uint32_t**)regs->ebx;
	uint32_t x             = regs->ecx;
	uint32_t y             = regs->edx;
	uint32_t width         = regs->esi;
	uint32_t height        = regs->edi;
	uint32_t requiredSize  = width * height * sizeof(uint32_t); // Calculate required size
	uint32_t requiredPages = CEIL_DIV_PAGE_SIZE(requiredSize);  // Calculate required pages

	// Check if userBuffer is a valid pointer
	if (!isValidAddress(userBuffer)) return;

	// Allocate the required pages for the window buffer
	auto* proc          = TaskManager::getCurrentProcess();
	auto* allocatedAddr = (uint32_t*)proc->allocatePages(requiredPages);
	*userBuffer = allocatedAddr;     // Set the user buffer to the allocated address

	// Request a window with the given parameters
	auto* window = WindowManager::requestWindow(allocatedAddr, x, y, width, height);
	if (!window)
	{
		// If the window cannot be created, set eax to -1
		regs->eax = -1;
		return;
	}

	// Add the window ID to the process's list of windows and set eax to the window ID
	proc->windows_.push_back(window->getID());
	regs->eax = window->getID();
}

void PalmyraOS::kernel::SystemCallsManager::handleCloseWindow(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// Extract arguments from registers
	uint32_t windowId = regs->ebx;

	// Get the current process and remove the window ID from its list of windows
	auto* proc = TaskManager::getCurrentProcess();
	for (auto it = proc->windows_.begin(); it != proc->windows_.end(); ++it)
	{
		if (*it == windowId)
		{
			proc->windows_.erase(it);
			break;
		}
	}

	// Close the window with the given ID
	WindowManager::closeWindow(windowId);
}

void PalmyraOS::kernel::SystemCallsManager::handleNextKeyboardEvent(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// Extract arguments from registers
	uint32_t windowId = regs->ebx;
	auto     event    = (KeyboardEvent*)regs->ecx;

	// Check if userBuffer is a valid pointer
	if (!isValidAddress(event)) return;

	*event = WindowManager::popKeyboardEvent(windowId);

}

void PalmyraOS::kernel::SystemCallsManager::handleGetdents(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// int write(uint32_t fd, const void *buf, uint32_t count)

	// Extract arguments from registers
	size_t fileDescriptor = regs->ebx;
	auto   bufferPointer  = (linux_dirent*)regs->ecx;
	size_t count          = regs->edx;

	// Check if bufferPointer is a valid pointer
	if (!isValidAddress(bufferPointer)) return;
	if (!isValidAddress(bufferPointer + count)) return;

	// TODO: uncap and allow for iterative
	count = count > 100 ? 100 : count;


	// Get the file associated with the file descriptor
	auto file = TaskManager::getCurrentProcess()->fileTableDescriptor_.getOpenFile(fileDescriptor);
	if (!file || file->getInode()->getType() != vfs::InodeBase::Type::Directory)
	{
		// If the file is not open, set the number of bytes read to 0
		regs->eax = -1; // invalid operation
		return;
	}

	// Read directory entries by offset
	auto dentries = file->getInode()->getDentries(file->getOffset());
	file->advanceOffset(dentries.size());

	char   * buffer  = (char*)bufferPointer;
	size_t bytesRead = 0;

	for (size_t index = 0; index < dentries.size() && bytesRead < count; ++index)
	{
		auto dentry = dentries[index];
		auto name   = dentry.first;
		auto type   = (uint8_t)dentry.second->getType();
		auto d_ino  = (uint32_t)dentry.second->getInodeNumber();

		size_t nameLen = name.size();

		// +1 for null-terminator, +1 for d_type
		size_t reclen = sizeof(linux_dirent) + nameLen + 1 + 1;

		if (bytesRead + reclen > count)
		{
			break; // Not enough space in the buffer
		}


		auto* dirent = (linux_dirent*)(buffer + bytesRead);
		dirent->d_ino    = d_ino;
		dirent->d_off    = bytesRead + reclen;
		dirent->d_reclen = reclen;

		memcpy((void*)dirent->d_name, (void*)name.c_str(), nameLen);
		dirent->d_name[nameLen] = '\0'; // Null-terminate the name

		*(char*)(buffer + bytesRead + reclen - 1) = (char)type; // Append the file type

		bytesRead += reclen;
	}

	regs->eax = bytesRead;
}

