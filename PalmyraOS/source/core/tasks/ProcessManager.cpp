

#include <elf.h>
#include <algorithm>

#include "core/tasks/ProcessManager.h"
#include "core/SystemClock.h"

#include "libs/string.h"
#include "libs/memory.h"
#include "libs/stdio.h"
#include "libs/stdlib.h" // uitoa64

#include "palmyraOS/unistd.h" // _exit()

#include "core/tasks/WindowManager.h"    // for cleaning up windows upon terminating

#include "core/files/VirtualFileSystem.h"
#include "core/peripherals/Logger.h"
///region Process


PalmyraOS::kernel::Process::Process(
	ProcessEntry entryPoint,
	uint32_t pid,
	Mode mode,
	Priority priority,
	uint32_t argc,
	char* const* argv,
	bool isInternal
)
	: pid_(pid), age_(2), state_(State::Ready), mode_(mode), priority_(priority)
{

	LOG_DEBUG("Constructing Process [pid %d]", pid_);

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
	initializePagingDirectory(mode_, isInternal);

	// 2.  Initialize the CPU state for the new process.
	initializeCPUState();

	// 3. Initialize the stack with the process arguments
	if (isInternal)
	{
		initializeArguments(entryPoint, argc, argv);
	}
	else
	{
		initializeArgumentsForELF(argc, argv);
	}

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


	// 5. Initialize Virtual File System Hooks
	initializeProcessInVFS();


	LOG_DEBUG("Constructing Process [pid %d] success", pid_);
}

void PalmyraOS::kernel::Process::initializePagingDirectory(Process::Mode mode, bool isInternal)
{

	// 1. Create and map the paging directory to itself based on the process mode.
	LOG_DEBUG("Creating Paging Directory.");
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
			pagingDirectory_,
			pagingDirectory_,
			PagingDirectoryFrames,
			PageFlags::Present | PageFlags::ReadWrite | PageFlags::UserSupervisor
		);
	}
	// Page directory is initialized

	// 2. Map the kernel stack for both kernel and user mode processes.
	LOG_DEBUG("Mapping Kernel Stack");
	kernelStack_ = kernelPagingDirectory_ptr->allocatePages(PROCESS_KERNEL_STACK_SIZE);
	registerPages(kernelStack_, PROCESS_KERNEL_STACK_SIZE);
	pagingDirectory_->mapPages(
		kernelStack_,
		kernelStack_,
		PROCESS_KERNEL_STACK_SIZE,
		PageFlags::Present | PageFlags::ReadWrite //| PageFlags::UserSupervisor // TODO investigation with ELF
	);

	// 3. If the process is in user mode, set up the user stack.
	if (mode == Process::Mode::User)
	{
		// Allocate and map the user stack.
		LOG_DEBUG("Mapping User Stack");
		userStack_ = allocatePages(PROCESS_USER_STACK_SIZE);

		// registers pages automatically
		PageFlags kernelSpaceFlags = PageFlags::Present | PageFlags::ReadWrite;
		if (isInternal) kernelSpaceFlags = kernelSpaceFlags | PageFlags::UserSupervisor;

		// The kernel is still mapped, but only accessed in user mode for internal applications.
		LOG_DEBUG("Mapping Kernel Space");
		pagingDirectory_->mapPages(
			nullptr,
			nullptr,
			kernel::kernelLastPage,
			kernelSpaceFlags
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
	stack_.esp = reinterpret_cast<uint32_t>(kernelStack_) + PAGE_SIZE * PROCESS_KERNEL_STACK_SIZE;
	stack_.eip = reinterpret_cast<uint32_t>(dispatcher);

	// Set the EFLAGS register, enabling interrupts and setting reserved bits.
	stack_.eflags = (1 << 1) | (1 << static_cast<uint32_t>(EFlags::IF_Interrupt));

	// For user mode, initialize the user stack pointer (userEsp).
	if (mode_ == Mode::User)
		stack_.userEsp = reinterpret_cast<uint32_t>(userStack_) + PAGE_SIZE * PROCESS_USER_STACK_SIZE - 16;

	// Set the CR3 register to point to the process's paging directory.
	stack_.cr3 = reinterpret_cast<uint32_t>(pagingDirectory_->getDirectory());
}

void PalmyraOS::kernel::Process::initializeArguments(ProcessEntry entry, uint32_t argc, char* const* argv)
{
	// Calculate the total size required for argv and the strings

	// Size of argc and argv pointers (inc. nullptr termination)
	size_t totalSize = (argc + 1) * sizeof(char*);

	// Sizes of the arguments
	for (uint32_t i = 0; i < argc; ++i) totalSize += strlen(argv[i]) + 1;

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
	physicalPages_.clear();

	// clean up windows buffers
	for (auto windowID : windows_)
	{
		WindowManager::closeWindow(windowID);
	}
	windows_.clear();

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
	/*
	 * Check for kernel stack overflow.
	 * The kernel stack pointer (esp) should never be below the base of the kernel stack (kernelStack_).
	 */
	if (stack_.esp < (reinterpret_cast<uint32_t>(kernelStack_)))
	{
		kernel::kernelPanic(
			"Kernel Stack Overflow detected for PID: %d.\nESP: 0x%x is below Kernel Stack Base: 0x%x",
			pid_, stack_.esp, reinterpret_cast<uint32_t>(kernelStack_));
	}

	/*
	 * When in user mode and executing in user space, we need to ensure that the user stack pointer (userEsp)
	 * is above the base of the user stack (userStack_). This prevents user stack overflow.
	 */
	if (mode_ == Mode::User && (stack_.cs & 0x11) != 0)
	{
		if (stack_.userEsp < (reinterpret_cast<uint32_t>(userStack_)))
		{

			kernel::kernelPanic(
				"User Stack Overflow detected for PID: %d. User ESP: 0x%x is below User Stack Base: 0x%x",
				pid_, stack_.userEsp, reinterpret_cast<uint32_t>(userStack_));
			return false;
		}
	}

	return true;
}

void* PalmyraOS::kernel::Process::allocatePagesAt(void* virtual_address, size_t count)
{
	// allocate the pages in kernel directory (so that they are accessible in syscalls)
	void* physicalAddress = kernelPagingDirectory_ptr->allocatePages(count);

	// register them to keep track of them when we terminate
	registerPages(physicalAddress, count);

	// Make them accessible to the process
	pagingDirectory_->mapPages(
		physicalAddress,
		virtual_address,
		count,
		PageFlags::Present | PageFlags::ReadWrite | PageFlags::UserSupervisor
	);

	return physicalAddress;
}

void PalmyraOS::kernel::Process::initializeArgumentsForELF(uint32_t argc, char* const* argv)
{
	LOG_DEBUG("argc=%d", argc);

	// Calculate the total size required for argv pointers and the strings themselves
	size_t        totalSize = (argc + 1) * sizeof(char*); // Space for argv pointers + null termination
	for (uint32_t i         = 0; i < argc; ++i)
	{
		totalSize += strlen(argv[i]) + 1; // Space for each string (null-terminated)
	}

	// Allocate memory for argv pointers and the strings
	size_t numPages = (totalSize + PAGE_SIZE - 1) >> PAGE_BITS;
	void* argv_block = allocatePages(numPages);
	char** argv_copy = static_cast<char**>(argv_block);
	char* str_copy   = reinterpret_cast<char*>(argv_block) + (argc + 1) * sizeof(char*);

	// Copy each argument string into the allocated memory block
	for (uint32_t i = 0; i < argc; ++i)
	{
		argv_copy[i] = str_copy;
		size_t len = strlen(argv[i]) + 1;
		memcpy(str_copy, argv[i], len);
		str_copy += len;
	}
	argv_copy[argc] = nullptr; // Null-terminate the argv array

	// Set up the stack layout according to the expected Linux x86 ABI
	if (mode_ == Mode::User)
	{
		// 1. Copy the argv pointers to the stack
		stack_.userEsp -= (argc + 1) * sizeof(char*);
		memcpy(reinterpret_cast<void*>(stack_.userEsp), argv_copy, (argc + 1) * sizeof(char*));
		uint32_t argv_pointer = stack_.userEsp; // Save the location of argv

		// 2. Place argc 4 bytes before the current stack pointer
		stack_.userEsp -= sizeof(uint32_t);
		*reinterpret_cast<uint32_t*>(stack_.userEsp) = argc;

	}
	else
	{
		// Normally, this setup is done for user mode; kernel mode doesn't follow this convention.
		// But for the sake of completeness, a similar setup could be done for kernel mode.
		stack_.esp -= (argc + 1) * sizeof(char*);
		memcpy(reinterpret_cast<void*>(stack_.esp), argv_copy, (argc + 1) * sizeof(char*));
		uint32_t argv_pointer = stack_.esp; // Save the location of argv

		stack_.esp -= sizeof(uint32_t);
		*reinterpret_cast<uint32_t*>(stack_.esp) = argc;
	}
}

void PalmyraOS::kernel::Process::initializeProcessInVFS()
{

	LOG_DEBUG("Initializing VFS hooks");

	char buffer[50];
	snprintf(buffer, sizeof(buffer), "/proc/%d", pid_);

	KString directory = KString(buffer);

	vfs::VirtualFileSystem::createDirectory(directory, vfs::InodeBase::Mode::USER_READ);

	auto statusNode = kernel::heapManager.createInstance<vfs::FunctionInode>(
		// Read
		[this](char* buffer, size_t size, size_t offset) -> size_t
		{
		  char uptime[40];
		  uitoa64(upTime_, uptime, 10, false);

		  // Create a string with the desired format
		  char   output[512];
		  size_t written = snprintf(
			  output, sizeof(output),
			  ""
			  "Pid: %d\n"
			  "State: %s\n"
			  "Up Time: %s\n"
			  "Pages: %d\n"
			  "Windows: %d\n"
			  "exitCode: %d\n",
			  pid_,
			  stateToString(),
			  uptime,
			  physicalPages_.size(),
			  windows_.size(),
			  exitCode_
		  );

		  // Ensure the output fits in the buffer, considering the offset
		  if (offset >= written) return 0;

		  size_t available = written - offset;
		  size_t to_copy   = available < size ? available : size;
		  memcpy(buffer, output + offset, to_copy);

		  return to_copy;
		}, nullptr, nullptr
	);
	vfs::VirtualFileSystem::setInodeByPath(directory + KString("/status"), statusNode);

	auto stdoutNode = kernel::heapManager.createInstance<vfs::FunctionInode>(
		// Read
		[this](char* buffer, size_t size, size_t offset) -> size_t
		{

		  // Calculate the size of stdout_ vector
		  size_t stdout_size = stdout_.size();

		  // Ensure the offset is within the bounds of the stdout_ data
		  if (offset >= stdout_size) return 0;

		  // Calculate the number of bytes to copy to the buffer
		  size_t available = stdout_size - offset;
		  size_t to_copy   = available < size ? available : size;

		  // Copy the data from stdout_ to the provided buffer
		  memcpy(buffer, stdout_.data() + offset, to_copy);

		  return to_copy;
		}, nullptr, nullptr
	);
	vfs::VirtualFileSystem::setInodeByPath(directory + KString("/stdout"), stdoutNode);

	auto stderrNode = kernel::heapManager.createInstance<vfs::FunctionInode>(
		// Read
		[this](char* buffer, size_t size, size_t offset) -> size_t
		{

		  // Calculate the size of stdout_ vector
		  size_t stderrsize = stderr_.size();

		  // Ensure the offset is within the bounds of the stdout_ data
		  if (offset >= stderrsize) return 0;

		  // Calculate the number of bytes to copy to the buffer
		  size_t available = stderrsize - offset;
		  size_t to_copy   = available < size ? available : size;

		  // Copy the data from stdout_ to the provided buffer
		  memcpy(buffer, stderr_.data() + offset, to_copy);

		  return to_copy;
		}, nullptr, nullptr
	);
	vfs::VirtualFileSystem::setInodeByPath(directory + KString("/stderr"), stderrNode);
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
	Process::Priority priority, uint32_t argc, char* const* argv,
	bool isInternal
)
{
	// Check if the maximum number of processes has been reached.
	if (processes_.size() == MAX_PROCESSES - 1) return nullptr;

	// Create a new process and add it to the processes vector.
	processes_.emplace_back(entryPoint, pid_count++, mode, priority, argc, argv, isInternal);

	// Return a pointer to the newly created process.
	return &processes_.back();
}

uint32_t* PalmyraOS::kernel::TaskManager::interruptHandler(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{

	size_t nextProcessIndex;
	uint32_t* result;

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
		nextProcessIndex = currentProcessIndex_;

		for (size_t i        = 0; i < processes_.size(); ++i)
		{
			nextProcessIndex = (currentProcessIndex_ + 1 + i) % processes_.size();
			if (processes_[nextProcessIndex].getState() == Process::State::Ready) break;
		}
		currentProcessIndex_ = nextProcessIndex;
	}

	// Set the new process state to running.
	processes_[currentProcessIndex_].state_ = Process::State::Running;
	processes_[currentProcessIndex_].upTime_++;

	// If the new process is in user mode, set the kernel stack.
	if (processes_[currentProcessIndex_].mode_ == Process::Mode::User)
	{
		// set the kernel stack at the top of the kernel stack
		kernel::gdt_ptr->setKernelStack(
			reinterpret_cast<uint32_t>(processes_[currentProcessIndex_].kernelStack_)
				+ PAGE_SIZE * PROCESS_KERNEL_STACK_SIZE - 1
		);
	}

	// Return the new process's stack pointer.
	result = reinterpret_cast<uint32_t*>(
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
	if (pid >= processes_.size()) return nullptr;
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

PalmyraOS::kernel::Process* PalmyraOS::kernel::TaskManager::execv_elf(
	KVector<uint8_t>& elfFileContent,
	PalmyraOS::kernel::Process::Mode mode,
	PalmyraOS::kernel::Process::Priority priority,
	uint32_t argc,
	char* const* argv
)
{
	// Ensure the ELF file is large enough to contain the header
	if (elfFileContent.size() < EI_NIDENT) return nullptr;

	// Read the ELF identification bytes
	unsigned char e_ident[EI_NIDENT];
	memcpy(e_ident, elfFileContent.data(), EI_NIDENT);

	// Verify the ELF magic number
	if (e_ident[EI_MAG0] != ELFMAG0 || e_ident[EI_MAG1] != ELFMAG1) return nullptr;
	if (e_ident[EI_MAG2] != ELFMAG2 || e_ident[EI_MAG3] != ELFMAG3) return nullptr;

	// Check the ELF class (32-bit)
	if (e_ident[EI_CLASS] != ELFCLASS32) return nullptr;

	// Check the data encoding (little-endian or big-endian)
	if (e_ident[EI_DATA] != ELFDATA2LSB) return nullptr;

	// Check the ELF version (1)
	if (e_ident[EI_VERSION] != EV_CURRENT) return nullptr;

	// Now cast the elfFileContent data to an Elf32_Ehdr structure for easier access to the fields
	const auto* elfHeader = reinterpret_cast<const Elf32_Ehdr*>(elfFileContent.data());

	// Check if the ELF file is an executable
	if (elfHeader->e_type != ET_EXEC) return nullptr;

	// Check if the ELF file is for the Intel 80386 architecture
	if (elfHeader->e_machine != EM_386) return nullptr;

	// Validations are successful.
	LOG_DEBUG("Elf Validations successful. Loading headers..");

	// Create a new process
	Process* process = newProcess(nullptr, mode, priority, argc, argv, false);
	if (!process) return nullptr;

	// Temporarily set process as killed, in case of invalid initialization
	process->setState(Process::State::Killed);

	uint32_t highest_vaddr = 0;  // To track the highest loaded segment's address for initializing current_brk

	// Load program headers
	const auto* programHeaders = reinterpret_cast<const Elf32_Phdr*>(elfFileContent.data() + elfHeader->e_phoff);
	for (int i = 0; i < elfHeader->e_phnum; ++i)
	{
		const Elf32_Phdr& ph = programHeaders[i];

		// Only load PT_LOAD segments
		if (ph.p_type != PT_LOAD) continue;

		// Align the virtual address down to the nearest page boundary
		uint32_t aligned_vaddr = ph.p_vaddr & ~(PAGE_SIZE - 1);

		// Calculate the page offset
		uint32_t page_offset = ph.p_vaddr - aligned_vaddr;

		// Adjust the size to include the page offset
		uint32_t segment_size = ph.p_memsz + page_offset;

		// Calculate the number of pages needed
		size_t num_pages = (segment_size + PAGE_SIZE - 1) >> PAGE_BITS;

		LOG_DEBUG("Loading Section %d: at 0x%X for %d pages", i, aligned_vaddr, num_pages);
		void* segmentAddress = process->allocatePagesAt(reinterpret_cast<void*>(aligned_vaddr), num_pages);
		if (!segmentAddress) return nullptr;

		// Copy the segment into memory
		memcpy(segmentAddress, elfFileContent.data() + ph.p_offset, ph.p_filesz);

		// Zero the remaining memory if p_memsz > p_filesz
		if (ph.p_memsz > ph.p_filesz)
		{
			memset(reinterpret_cast<uint8_t*>(segmentAddress) + ph.p_filesz, 0, ph.p_memsz - ph.p_filesz);
		}


		// Update the highest virtual address to track the end of the loaded segments
		uint32_t segment_end = ph.p_vaddr + ph.p_memsz;
		if (segment_end > highest_vaddr)
		{
			highest_vaddr = segment_end;
		}

	}
	LOG_DEBUG("Loading headers completed.");

	// Initialize the program break to the end of the last loaded segment
	// Set initial_brk and current_brk to just after the highest loaded segment
	process->initial_brk = (highest_vaddr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);  // Align to next page boundary
	process->current_brk = process->initial_brk;  // Initial and current brk set to the same value
//	process->max_brk = process->initial_brk + MAX_HEAP_SIZE;  // Set an arbitrary limit for heap size (you can define MAX_HEAP_SIZE)

	LOG_DEBUG("Program break (brk) initialized at: 0x%X", process->initial_brk);

	// Set up the initial CPU state (already initialized in newProcess)
	// TODO make stuff here more logical PLEASE (intNo + 2 * sizeof(uint32_t) for eip)
	*(uint32_t*)(process->stack_.esp + 8) = elfHeader->e_entry;
	process->stack_.eip = elfHeader->e_entry;

	// Set process state to Ready
	process->setState(Process::State::Ready);

	return process;

}






///endregion


















