

#include <algorithm>

#include "core/tasks/ProcessManager.h"
#include "core/SystemClock.h"
#include "core/files/VirtualFileSystem.h"

#include "libs/string.h"
#include "libs/memory.h"

#include "palmyraOS/unistd.h" // _exit()

#include "core/tasks/WindowManager.h"    // for cleaning up windows upon terminating

///region Process


PalmyraOS::kernel::Process::Process(
	ProcessEntry entryPoint,
	uint32_t pid,
	Mode mode,
	Priority priority,
	uint32_t argc,
	char** argv
)
	: pid_(pid), age_(2), state_(State::Ready), mode_(mode), priority_(priority)
{
	// Assertions to ensure the entry point is within kernel space.
	// TODO: just temporarily check to prevent entry points outside the kernel.
	if (((uint32_t)entryPoint >> 12) > kernel::kernelLastPage)
	{
		// If the entry point is invalid, trigger a kernel panic with details.
		kernel::kernelPanic(
			"Entry point outside kernel pages!\n"
			"PID: %d\n"
			"Mode: %s\n"
			"Entry Point: 0x%X\n"
			"Kernel Upper: 0x%X",
			pid_,
			(mode_ == Mode::Kernel ? "kernel" : "user"),
			(uint32_t)entryPoint,
			kernel::kernelLastPage
		);
	}

	// 1.  Create and initialize the paging directory for the process.
	initializePagingDirectory(mode_);

	// 2.  Initialize the CPU state for the new process.
	initializeCPUState();

	// 3. Initialize the stack with the process arguments
	initializeArguments(entryPoint, argc, argv);

	// 4.  Initialize the process stack with CPU state
	{
		// Adjust the stack pointer to reserve space for the CPU registers.
		stack_.esp -= sizeof(interrupts::CPURegisters);
		auto* stack_ptr = reinterpret_cast<interrupts::CPURegisters*>(stack_.esp);

		// Write the CPU state onto the stack.
		*stack_ptr = stack_;

		// Adjust the stack pointer to point to the interrupt number location.
		stack_.esp += offsetof(interrupts::CPURegisters, intNo); // 52
	}
}

void PalmyraOS::kernel::Process::initializePagingDirectory(Process::Mode mode)
{
	PagingDirectory* pagingDirectory = nullptr;

	// 1. Create and map the paging directory to itself based on the process mode.
	if (mode == Process::Mode::Kernel)
	{
		// For kernel mode, use the kernel's paging directory.
		pagingDirectory_ = kernel::kernelPagingDirectory_ptr;
	}
	else
	{
		// For user mode, allocate a new paging directory.
		uint32_t PagingDirectoryFrames = (sizeof(PagingDirectory) >> PAGE_BITS) + 1;
		pagingDirectory_ = static_cast<PagingDirectory*>(
			kernelPagingDirectory_ptr->allocatePages(PagingDirectoryFrames)
		);
		registerPages(pagingDirectory_, PagingDirectoryFrames);
		pagingDirectory_->mapPages(
			pagingDirectory,
			pagingDirectory,
			PagingDirectoryFrames,
			PageFlags::Present | PageFlags::ReadWrite | PageFlags::UserSupervisor
		);
	}
	// Page directory is initialized

	// 2. Map the kernel stack for both kernel and user mode processes.
	kernelStack_ = kernelPagingDirectory_ptr->allocatePage();
	registerPages(kernelStack_, 1);
	pagingDirectory_->mapPage(
		kernelStack_,
		kernelStack_,
		PageFlags::Present | PageFlags::ReadWrite
	);

	// 3. If the process is in user mode, set up the user stack.
	if (mode == Process::Mode::User)
	{
		// Allocate and map the user stack.
		userStack_ = allocatePages(1);

		// TODO Temporarily map the kernel code space for debugging purposes.
		pagingDirectory_->mapPages(
			nullptr,
			nullptr,
			kernel::kernelLastPage,
			PageFlags::Present | PageFlags::ReadWrite | PageFlags::UserSupervisor
		);
	}
}

void PalmyraOS::kernel::Process::initializeCPUState()
{
	// Determine the data and code segment selectors based on the mode.
	uint32_t dataSegment = mode_ == Mode::Kernel ?
						   gdt_ptr->getKernelDataSegmentSelector() :
						   gdt_ptr->getUserDataSegmentSelector();

	uint32_t codeSegment = mode_ == Mode::Kernel ?
						   gdt_ptr->getKernelCodeSegmentSelector() :
						   gdt_ptr->getUserCodeSegmentSelector();

	// Set the data segment selectors in the process's stack.
	// This sets the GS, FS, ES, DS, and SS segment registers.
	stack_.gs = dataSegment | static_cast<uint32_t>(mode_);
	stack_.fs = dataSegment | static_cast<uint32_t>(mode_);
	stack_.es = dataSegment | static_cast<uint32_t>(mode_);
	stack_.ds = dataSegment | static_cast<uint32_t>(mode_);
	stack_.ss = dataSegment | static_cast<uint32_t>(mode_);    // Only for user mode

	// Set the code segment selector in the process's stack.
	stack_.cs = codeSegment | static_cast<uint32_t>(mode_);

	// The general-purpose registers are initialized to 0 by default.

	// Initialize the stack pointer (ESP) and instruction pointer (EIP).
	stack_.esp = reinterpret_cast<uint32_t>(kernelStack_) + PAGE_SIZE;
	stack_.eip = reinterpret_cast<uint32_t>(dispatcher);

	// Set the EFLAGS register, enabling interrupts and setting reserved bits.
	stack_.eflags = (1 << 1) | (1 << static_cast<uint32_t>(EFlags::IF_Interrupt));

	// For user mode, initialize the user stack pointer (userEsp).
	if (mode_ == Mode::User) stack_.userEsp = reinterpret_cast<uint32_t>(userStack_) + PAGE_SIZE;

	// Set the CR3 register to point to the process's paging directory.
	stack_.cr3 = reinterpret_cast<uint32_t>(pagingDirectory_->getDirectory());
}

void PalmyraOS::kernel::Process::initializeArguments(ProcessEntry entry, uint32_t argc, char** argv)
{
	// Calculate the total size required for argv and the strings
	size_t        totalSize = (argc + 1) * sizeof(char*);
	for (uint32_t i         = 0; i < argc; ++i)
	{
		totalSize += strlen(argv[i]) + 1;
	}

	// Allocate a single block of memory for argv and the strings
	size_t numPages = (totalSize + PAGE_SIZE - 1) >> PAGE_BITS;
	void* argv_block = allocatePages(numPages);
	char** argv_copy = static_cast<char**>(argv_block);
	char* str_copy   = reinterpret_cast<char*>(argv_block) + (argc + 1) * sizeof(char*);

	// Copy each argument string into the allocated block.
	for (uint32_t i = 0; i < argc; ++i)
	{
		argv_copy[i] = str_copy;
		size_t len = strlen(argv[i]) + 1;
		memcpy((void*)str_copy, (void*)argv[i], len);
		str_copy += len;
	}
	argv_copy[argc] = nullptr; // Null-terminate the argv array.

	// Set up the process arguments in the stack based on the mode.
	if (mode_ == Mode::Kernel)
	{
		// Push the ProcessArguments structure onto the stack.
		stack_.esp -= sizeof(Arguments);
		auto* processArgs = reinterpret_cast<Arguments*>(stack_.esp);
		processArgs->entryPoint = entry;
		processArgs->argc       = argc;
		processArgs->argv       = argv_copy;

		// In kernel mode, ss becomes the effective argument to the dispatcher
		stack_.ss = stack_.esp;
	}
	else
	{
		// User mode, copy to user stack
		stack_.userEsp -= sizeof(Arguments);
		auto* processArgs = reinterpret_cast<Arguments*>(stack_.userEsp);
		processArgs->entryPoint = entry;
		processArgs->argc       = argc;
		processArgs->argv       = argv_copy;

		// Adjust the user stack pointer to include the arguments' pointer.
		stack_.userEsp -= sizeof(Arguments*);
		auto* processArgsPtr = reinterpret_cast<Arguments**>(stack_.userEsp);
		*processArgsPtr = processArgs;
		stack_.userEsp -= 4;    // first argument is esp + 4
	}

}

void PalmyraOS::kernel::Process::terminate(int exitCode)
{
	state_ = Process::State::Terminated;
	exitCode_ = exitCode;
}

void PalmyraOS::kernel::Process::kill()
{
	state_ = State::Killed;
	age_   = 0;
	// exitCode_ is set by _exit syscall

	// clean up memory
	for (auto& physicalPage : physicalPages_)
	{
		kernel::kernelPagingDirectory_ptr->freePage(physicalPage);
	}

	// clean up windows buffers
	for (auto windowID : windows_)
	{
		WindowManager::closeWindow(windowID);
	}

	// TODO free directory table arrays if user process
}

void PalmyraOS::kernel::Process::dispatcher(PalmyraOS::kernel::Process::Arguments* args)
{
	// This is executed in user mode (for user processes)

	// Call the entry point with the given arguments
	uint32_t exitCode = args->entryPoint(args->argc, args->argv);

	// should not arrive here, but just in cse
	_exit(exitCode);
}

void PalmyraOS::kernel::Process::registerPages(void* physicalAddress, size_t count)
{
	for (int i = 0; i < count; ++i)
	{
		uint32_t address = (uint32_t)physicalAddress + (i << PAGE_BITS);
		physicalPages_.push_back((void*)address);
	}
}

void PalmyraOS::kernel::Process::deregisterPages(void* physicalAddress, size_t count)
{
	for (size_t i = 0; i < count; ++i)
	{
		uint32_t address = (uint32_t)physicalAddress + (i << PAGE_BITS);
		auto     it      = std::find(physicalPages_.begin(), physicalPages_.end(), (void*)address);
		if (it != physicalPages_.end())
		{
			physicalPages_.erase(it);
			kernel::kernelPagingDirectory_ptr->freePage((void*)address);
		}
	}
}

void* PalmyraOS::kernel::Process::allocatePages(size_t count)
{
	// allocate the pages in kernel directory (so that they are accessible in syscalls)
	void* address = kernelPagingDirectory_ptr->allocatePages(count);

	// register them to keep track of them when we terminate
	registerPages(address, count);

	// Make them accessible to the process
	pagingDirectory_->mapPages(
		address,
		address,
		count,
		PageFlags::Present | PageFlags::ReadWrite | PageFlags::UserSupervisor
	);

	return address;
}

bool PalmyraOS::kernel::Process::checkStackOverflow() const
{
	if (stack_.esp < (reinterpret_cast<uint32_t>(kernelStack_)))
	{
		// Trigger a kernel panic or handle stack overflow appropriately
		kernel::kernelPanic("Kernel Stack overflow detected for PID: %d", pid_);
		return false;
	}
	else if (mode_ == Mode::User && stack_.userEsp < (reinterpret_cast<uint32_t>(userStack_)))
	{
		// Trigger a kernel panic or handle stack overflow appropriately
		kernel::kernelPanic("User Stack overflow detected for PID: %d", pid_);
		return false;
	}
	return true;
}
///endregion








///region Task Manager

// Globals
PalmyraOS::kernel::KVector<PalmyraOS::kernel::Process> PalmyraOS::kernel::TaskManager::processes_;
uint32_t PalmyraOS::kernel::TaskManager::currentProcessIndex_ = MAX_PROCESSES;
uint32_t PalmyraOS::kernel::TaskManager::atomicSectionLevel_  = 0;
uint32_t PalmyraOS::kernel::TaskManager::pid_count            = 0;

void PalmyraOS::kernel::TaskManager::initialize()
{
	// Attach the task switching interrupt handler to the system clock.
	SystemClock::attachHandler(interruptHandler);

	// Clear and reserve space in the processes vector.
	processes_.clear();
	processes_.reserve(MAX_PROCESSES);
}

PalmyraOS::kernel::Process* PalmyraOS::kernel::TaskManager::newProcess(
	Process::ProcessEntry entryPoint,
	Process::Mode mode,
	Process::Priority priority, uint32_t argc, char** argv
)
{
	// Check if the maximum number of processes has been reached.
	if (processes_.size() == MAX_PROCESSES - 1) return nullptr;

	// Create a new process and add it to the processes vector.
	processes_.emplace_back(entryPoint, pid_count++, mode, priority, argc, argv);

	// Return a pointer to the newly created process.
	return &processes_.back();
}

uint32_t* PalmyraOS::kernel::TaskManager::interruptHandler(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// kill terminated processes
	for (int i = 0; i < processes_.size(); ++i)
	{
		if (i == currentProcessIndex_) continue; // we cannot kill the process in its own stack (-> Page Fault)
		if (processes_[i].state_ == Process::State::Terminated)
		{
			processes_[i].kill();
		}
	}

	// If there are no processes, or we are in an atomic section, return the current registers.
	if (processes_.empty()) return reinterpret_cast<uint32_t*>(regs);
	if (atomicSectionLevel_ > 0) return reinterpret_cast<uint32_t*>(regs);

	// Save the current process state if a process is running.
	if (currentProcessIndex_ < MAX_PROCESSES)
	{
		// save current process state
		processes_[currentProcessIndex_].stack_ = *regs;

		// check stackOverflow
		if (!processes_[currentProcessIndex_].checkStackOverflow())
		{
			// TODO handle here e.g. .terminate(-3)
		}

		// if the process is not terminated or killed
		if (processes_[currentProcessIndex_].state_ != Process::State::Terminated
			&& processes_[currentProcessIndex_].state_ != Process::State::Killed)
		{
			// Decrease the age of the current process.
			if (processes_[currentProcessIndex_].age_ > 0) processes_[currentProcessIndex_].age_--;

			// If the age of the current process is still greater than 0, continue running it.
			if (processes_[currentProcessIndex_].age_ > 0) return reinterpret_cast<uint32_t*>(regs);

			// If the age reaches 0, set the current process state to Ready.
			processes_[currentProcessIndex_].state_ = Process::State::Ready;
			processes_[currentProcessIndex_].age_   = static_cast<uint32_t>(processes_[currentProcessIndex_].priority_);
		}
	}

	// Find the next process in the "Ready" state.
	{
		size_t nextProcessIndex = currentProcessIndex_;

		for (size_t i        = 0; i < processes_.size(); ++i)
		{
			nextProcessIndex = (currentProcessIndex_ + 1 + i) % processes_.size();
			if (processes_[nextProcessIndex].getState() == Process::State::Ready) break;
		}
		currentProcessIndex_ = nextProcessIndex;
	}

	// Set the new process state to running.
	processes_[currentProcessIndex_].state_ = Process::State::Running;

	// If the new process is in user mode, set the kernel stack.
	if (processes_[currentProcessIndex_].mode_ == Process::Mode::User)
		// set the kernel stack at the top of the kernel stack
		kernel::gdt_ptr->setKernelStack(
			reinterpret_cast<uint32_t>(processes_[currentProcessIndex_].kernelStack_) + STACK_SIZE - 1
		);

	// Return the new process's stack pointer.
	auto* result = reinterpret_cast<uint32_t*>(
		processes_[currentProcessIndex_].stack_.esp - offsetof(interrupts::CPURegisters, intNo)
	);


	return result;
}

PalmyraOS::kernel::Process* PalmyraOS::kernel::TaskManager::getCurrentProcess()
{
	return &processes_[currentProcessIndex_];
}

PalmyraOS::kernel::Process* PalmyraOS::kernel::TaskManager::getProcess(uint32_t pid)
{
	return &processes_[pid];
}

void PalmyraOS::kernel::TaskManager::startAtomicOperation()
{
	if (processes_[currentProcessIndex_].mode_ != Process::Mode::Kernel) return;
	atomicSectionLevel_++;
}

void PalmyraOS::kernel::TaskManager::endAtomicOperation()
{
	if (atomicSectionLevel_ > 0) atomicSectionLevel_--;
}






///endregion


















