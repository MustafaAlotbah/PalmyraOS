

#include "core/tasks/ProcessManager.h"
#include "core/SystemClock.h"

///region Process


PalmyraOS::kernel::Process::Process(int (* entryPoint)(), uint32_t pid, Mode mode)
	: pid_(pid), age_(2), state_(State::Ready), mode_(mode)
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
	{
		// Determine the data and code segment selectors based on the mode.
		uint32_t dataSegment = mode_ == Mode::Kernel ?
							   gdt_ptr->getKernelDataSegmentSelector() :
							   gdt_ptr->getUserDataSegmentSelector();

		uint32_t codeSegment = mode_ == Mode::Kernel ?
							   gdt_ptr->getKernelCodeSegmentSelector() :
							   gdt_ptr->getUserCodeSegmentSelector();

		// Set the data segment selectors in the process's stack.
		stack_.gs = dataSegment | static_cast<uint32_t>(mode_);
		stack_.fs = dataSegment | static_cast<uint32_t>(mode_);
		stack_.es = dataSegment | static_cast<uint32_t>(mode_);
		stack_.ds = dataSegment | static_cast<uint32_t>(mode_);
		stack_.ss = dataSegment | static_cast<uint32_t>(mode_);

		// Set the code segment selector in the process's stack.
		stack_.cs = codeSegment | static_cast<uint32_t>(mode_);

		// The general-purpose registers are initialized to 0 by default.

		// Initialize the stack pointer (ESP) and instruction pointer (EIP).
		stack_.esp = reinterpret_cast<uint32_t>(kernelStack_) + PAGE_SIZE - sizeof(interrupts::CPURegisters) - 4;
		stack_.eip = reinterpret_cast<uint32_t>(entryPoint);

		// Set the EFLAGS register, enabling interrupts and setting reserved bits.
		stack_.eflags = (1 << 1) | (1 << static_cast<uint32_t>(EFlags::IF_Interrupt));

		// For user mode, initialize the user stack pointer (userEsp).
		if (mode_ == Mode::User)
		{
			stack_.userEsp = reinterpret_cast<uint32_t>(userStack_) + PAGE_SIZE - sizeof(interrupts::CPURegisters) - 4;
		}

		// Set the CR3 register to point to the process's paging directory.
		stack_.cr3 = reinterpret_cast<uint32_t>(pagingDirectory_->getDirectory());
	}

	// 5.  Initialize the process stack with CPU state
	{
		// Write the CPU state onto the stack.
		*((interrupts::CPURegisters*)(stack_.esp)) = stack_;

		// Adjust the stack pointer to point to the interrupt number location.
		stack_.esp += offsetof(interrupts::CPURegisters, intNo); // 52
		stack_.userEsp                             = stack_.esp;
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
		pagingDirectory_->mapPages(
			pagingDirectory,
			pagingDirectory,
			PagingDirectoryFrames,
			PageFlags::Present | PageFlags::ReadWrite | PageFlags::UserSupervisor
		);
	}

	// 2. Map the kernel stack for both kernel and user mode processes.
	kernelStack_ = kernelPagingDirectory_ptr->allocatePage();
	pagingDirectory_->mapPage(
		kernelStack_,
		kernelStack_,
		PageFlags::Present | PageFlags::ReadWrite | PageFlags::UserSupervisor
	);

	// 3. If the process is in user mode, set up the user stack.
	if (mode == Process::Mode::User)
	{
		// Allocate and map the user stack.
		userStack_ = kernelPagingDirectory_ptr->allocatePage();
		pagingDirectory_->mapPage(
			userStack_,
			userStack_,
			PageFlags::Present | PageFlags::ReadWrite | PageFlags::UserSupervisor
		);

		// TODO Temporarily map the kernel code space for debugging purposes.
		pagingDirectory_->mapPages(
			nullptr,
			nullptr,
			kernel::kernelLastPage,
			PageFlags::Present | PageFlags::ReadWrite | PageFlags::UserSupervisor
		);
	}
}


///endregion








///region Task Manager

// Globals
PalmyraOS::kernel::ProcessVector PalmyraOS::kernel::TaskManager::processes_;
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

PalmyraOS::kernel::Process* PalmyraOS::kernel::TaskManager::newProcess(int (* entryPoint)(), Process::Mode mode)
{
	// Check if the maximum number of processes has been reached.
	if (processes_.size() == MAX_PROCESSES - 1) return nullptr;

	// Create a new process and add it to the processes vector.
	processes_.emplace_back(entryPoint, pid_count++, mode);

	// Return a pointer to the newly created process.
	return &processes_.back();
}

uint32_t* PalmyraOS::kernel::TaskManager::interruptHandler(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// If there are no processes, or we are in an atomic section, return the current registers.
	if (processes_.empty()) return reinterpret_cast<uint32_t*>(regs);
	if (atomicSectionLevel_ > 0) return reinterpret_cast<uint32_t*>(regs);

	// Save the current process state if a process is running.
	if (currentProcessIndex_ < MAX_PROCESSES)
	{
		// save current process state
		processes_[currentProcessIndex_].stack_ = *regs;
		processes_[currentProcessIndex_].state_ = Process::State::Ready;
	}

	// Move to the next process in the queue.
	currentProcessIndex_++;
	if (currentProcessIndex_ >= processes_.size())
		currentProcessIndex_ %= processes_.size();

	// Set the new process state to running.
	processes_[currentProcessIndex_].state_ = Process::State::Running;

	// If the new process is in user mode, set the kernel stack.
	if (processes_[currentProcessIndex_].mode_ == Process::Mode::User)
		kernel::gdt_ptr->setKernelStack(
			reinterpret_cast<uint32_t>(processes_[currentProcessIndex_].kernelStack_)
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






///endregion


















