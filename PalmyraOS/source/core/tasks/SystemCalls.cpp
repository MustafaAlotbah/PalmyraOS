

#include "core/tasks/SystemCalls.h"

// API Headers
#include "palmyraOS/unistd.h"
#include "palmyraOS/time.h"

// System Objects
#include "core/tasks/ProcessManager.h"
#include "core/tasks/WindowManager.h"
#include "core/SystemClock.h"
#include "core/files/VirtualFileSystem.h"


void PalmyraOS::kernel::SystemCallsManager::initialize()
{
	interrupts::InterruptController::setInterruptHandler(0x80, &handleInterrupt);
}

uint32_t* PalmyraOS::kernel::SystemCallsManager::handleInterrupt(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	bool handled = false;

	// TODO help functions to get rid of goto
	// Arguments: eax, ebx, ecx, edx, esi, edi

	/***** POSIX System Calls *****/

	// void _exit(int)
	if (regs->eax == POSIX_INT_EXIT)
	{
		handled = true;

		// terminate
		TaskManager::getCurrentProcess()->terminate((int)regs->ebx);

		// scheduler calls kill() later.
		return TaskManager::interruptHandler(regs);
	}


	// uint32_t getpid()
	if (!handled && regs->eax == POSIX_INT_GET_PID)
	{
		handled = true;
		auto* proc = TaskManager::getCurrentProcess();
		regs->eax = proc->getPid();
	}


	// int sched_yield();
	if (!handled && regs->eax == POSIX_INT_YIELD)
	{
		handled = true;

		// return true anyway TODO
		regs->eax = 0;

		auto* proc = TaskManager::getCurrentProcess();
		proc->age_ = 0;

		// find another.
		return TaskManager::interruptHandler(regs);
	}


	// void* mmap(void* addr, uint32_t length, int prot, int flags, int fd, uint32_t offset)
	if (!handled && regs->eax == POSIX_INT_MMAP)
	{
		handled = true;
		void* addr = (void*)regs->ebx;
		uint32_t length = regs->ecx;

		// TODO, offset
		uint32_t protectionFlags = regs->edx;
		uint32_t flags           = regs->esi;
		uint32_t fd_reg          = regs->edi;

		auto* proc          = TaskManager::getCurrentProcess();
		void* allocatedAddr = proc->allocatePages((length >> 12) + 1);
		if (allocatedAddr != nullptr) regs->eax = (uint32_t)allocatedAddr;
		else regs->eax = (uint32_t)MAP_FAILED;
	}


	// int clock_gettime(uint32_t clk_id, struct timespec *tp)
	if (!handled && regs->eax == POSIX_INT_GETTIME)
	{
		handled = true;
		// arguments
		uint32_t clockId = regs->edi;
		auto* tp = (timespec*)regs->esi;

		// get ticks and frequency
		uint64_t ticks     = SystemClock::getTicks();
		uint64_t frequency = SystemClockFrequency;

		// results
		tp->tv_nsec = (ticks * 1'000'000) / frequency;
		tp->tv_sec  = tp->tv_nsec / 1'000'000;

		regs->eax = 0;
	}

	// int open(const char *pathname, int flags)
	if (!handled && regs->eax == POSIX_INT_OPEN)
	{
		handled = true;

		const char* pathname = (const char*)regs->ebx;
		int flags = static_cast<int>(regs->ecx);

		// TODO: Implement the actual file opening logic
		auto* proc = TaskManager::getCurrentProcess();
		auto inode = vfs::VirtualFileSystem::getInodeByPath(KString(pathname));

		if (!inode)
		{
			// File does not exit/not found
			regs->eax = -1;
			goto return_clause;
		}

		// inode found -> allocate the node
		fd_t fileDescriptor = proc->fileTableDescriptor_.allocate(inode, flags);
		regs->eax = fileDescriptor;
	}

	// int close(int fd)
	if (!handled && regs->eax == POSIX_INT_CLOSE)
	{
		handled = true;

		uint32_t fd = regs->ebx;

		// TODO: Improvement (other status, release -> bool?)
		auto* proc = TaskManager::getCurrentProcess();
		proc->fileTableDescriptor_.release(fd);
		regs->eax = 0; // Success
	}

	// int write(uint32_t fd, const void *buf, uint32_t count)
	if (!handled && regs->eax == POSIX_INT_WRITE)
	{
		handled = true;
		size_t fileDescriptor = regs->ebx;
		const char* bufferPointer = (const char*)regs->ecx;
		size_t size = regs->edx;
		auto* proc = TaskManager::getCurrentProcess();
		regs->eax = 0;

		// TODO move to actual
		// TODO 0
		if (fileDescriptor == 1)
		{
			for (size_t i = 0; i < size; ++i)
			{
				if (bufferPointer[i] == '\0') break;
				proc->stdout_.push_back(bufferPointer[i]);
				regs->eax++;
			}
		}
		else if (fileDescriptor == 2)
		{
			for (size_t i = 0; i < size; ++i)
			{
				if (bufferPointer[i] == '\0') break;
				proc->stderr_.push_back(bufferPointer[i]);
				regs->eax++;
			}
		}
		else
		{
			auto file = proc->fileTableDescriptor_.getOpenFile(fileDescriptor);

			// no such open file
			if (!file)
			{
				regs->eax = 0;    // we wrote 0 bytes
				goto return_clause;
			}

			// TODO: this must be moved to OpenFile
			auto bytesRead = file->getInode()->write(bufferPointer, size, file->getOffset());
			file->advanceOffset(bytesRead);
			regs->eax = bytesRead;
		}


	}

	// int read(uint32_t fileDescriptor, void* buffer, uint32_t count);
	if (!handled && regs->eax == POSIX_INT_READ)
	{

		handled = true;
		size_t fileDescriptor = regs->ebx;
		char* bufferPointer = (char*)regs->ecx;
		size_t size = regs->edx;
		auto   proc = TaskManager::getCurrentProcess();
		auto   file = proc->fileTableDescriptor_.getOpenFile(fileDescriptor);

		// no such open file
		if (!file)
		{
			regs->eax = 0;    // we read 0 bytes
			goto return_clause;
		}

		// TODO: this must be moved to OpenFile
		auto bytesRead = file->getInode()->read(bufferPointer, size, file->getOffset());
		file->advanceOffset(bytesRead);
		regs->eax = bytesRead;
	}

	// int ioctl(int fd, uint32_t request, ...)
	if (!handled && regs->eax == POSIX_INT_IOCTL)
	{
		handled = true;

		uint32_t fileDescriptor = regs->ebx;
		int      request        = static_cast<int>(regs->ecx);
		void* argp = (void*)regs->edx;

		// TODO: Implement the actual ioctl logic
		auto* proc = TaskManager::getCurrentProcess();
		auto file = proc->fileTableDescriptor_.getOpenFile(fileDescriptor);

		// no such open file
		if (!file)
		{
			regs->eax = 0;    // we read 0 bytes
			goto return_clause;
		}

		auto bytesRead = file->getInode()->ioctl(request, argp);
		regs->eax = bytesRead; // return status 0, 1
	}


	/***** Custom System Calls *****/

	// uint32_t initializeWindow(uint32_t** buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	if (!handled && regs->eax == INT_INIT_WINDOW)
	{
		handled = true;

		auto** userBuffer = (uint32_t**)regs->ebx;
		uint32_t x      = regs->ecx;
		uint32_t y      = regs->edx;
		uint32_t width  = regs->esi;
		uint32_t height = regs->edi;

		uint32_t requiredSize  = width * height * sizeof(uint32_t);
		uint32_t requiredPages = CEIL_DIV_PAGE_SIZE(requiredSize);    // ceil (requiredSize / 4096)

		auto* proc          = TaskManager::getCurrentProcess();
		auto* allocatedAddr = (uint32_t*)proc->allocatePages(requiredPages);    // TODO

		*userBuffer = allocatedAddr;
		auto* window = WindowManager::requestWindow(allocatedAddr, x, y, width, height);
		regs->eax = window->getID();
	}

	// void closeWindow(uint32_t windowID)
	if (!handled && regs->eax == INT_CLOSE_WINDOW)
	{
		handled = true;
		uint32_t windowId = regs->ebx;

		WindowManager::closeWindow(windowId);
	}

return_clause:
	return (uint32_t*)(regs);
}
