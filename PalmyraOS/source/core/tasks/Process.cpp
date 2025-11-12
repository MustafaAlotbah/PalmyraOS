

#include <algorithm>
#include <elf.h>
#include <new>

#include "core/SystemClock.h"
#include "core/tasks/Process.h"

#include "libs/memory.h"
#include "libs/stdio.h"
#include "libs/stdlib.h"  // uitoa64
#include "libs/string.h"

#include "palmyraOS/unistd.h"  // _exit()

#include "core/tasks/WindowManager.h"  // for cleaning up windows upon terminating

#include "core/files/VirtualFileSystem.h"
#include "core/peripherals/Logger.h"
/// region Process


PalmyraOS::kernel::Process::Process(ProcessEntry entryPoint, uint32_t pid, Mode mode, Priority priority, uint32_t argc, char* const* argv, char* const* envp, bool isInternal)
    : pid_(pid), age_(2), state_(State::Ready), mode_(mode), priority_(priority) {

    LOG_DEBUG("Constructing Process [pid %d] (%s) (mode: %s)", pid_, argv[0], mode_ == Mode::Kernel ? "kernel" : "user");

    // Assertions to ensure the entry point is within kernel space.
    // TODO: just temporarily check to prevent entry points outside the kernel.
    if (((uint32_t) entryPoint >> 12) > kernel::kernelLastPage) {
        // If the entry point is invalid, trigger a kernel panic with details.
        kernel::kernelPanic("Entry point outside kernel pages!\n"
                            "PID: %d\n"
                            "Mode: %s\n"
                            "Entry Point: 0x%X\n"
                            "Kernel Upper: 0x%X",
                            pid_,
                            (mode_ == Mode::Kernel ? "kernel" : "user"),
                            (uint32_t) entryPoint,
                            kernel::kernelLastPage);
    }

    // 1.  Create and initialize the paging directory for the process.
    initializePagingDirectory(mode_, isInternal);

    // 2.  Initialize the CPU state for the new process.
    initializeCPUState();

    // 3. Capture environment variables (for /proc and process metadata)
    captureEnvironment(envp);

    // 4. Initialize the stack with the process arguments and environment
    // NOTE: For ELF processes, we DON'T initialize here! The auxiliary vector
    //       needs to be built first (in execv_elf), THEN we set up the stack once.
    if (isInternal) { initializeArguments(entryPoint, argc, argv, envp); }
    // For ELF: Stack initialization deferred to execv_elf() after auxv is built

    // 5.  Initialize the process stack with CPU state
    {
        // Adjust the stack pointer to reserve space for the CPU registers.
        stack_.esp -= sizeof(interrupts::CPURegisters);
        auto* stack_ptr = reinterpret_cast<interrupts::CPURegisters*>(stack_.esp);

        // Write the CPU state onto the stack.
        *stack_ptr      = stack_;

        // Adjust the stack pointer to point to the interrupt number location.
        stack_.esp += offsetof(interrupts::CPURegisters, intNo);  // 52
    }

    debug_.entryEip = reinterpret_cast<uint32_t>(entryPoint);

    // Record the time when this process was started (for /proc/pid/stat)
    startTime_      = SystemClock::getTicks();

    // 5. Capture command-line arguments (safe copy for later access via /proc/{pid}/cmdline)
    captureCommandlineArguments(argc, argv);

    // 6. Initialize Virtual File System Hooks
    initializeProcessInVFS();


    LOG_DEBUG("Constructing Process [pid %d] success", pid_);

    // Log complete memory layout for debugging
    LOG_DEBUG("Process Memory Layout [PID %d]:", pid_);
    LOG_DEBUG("  Kernel Stack: 0x%X - 0x%X (Size: %d pages)",
              reinterpret_cast<uint32_t>(kernelStack_),
              reinterpret_cast<uint32_t>(kernelStack_) + PAGE_SIZE * PROCESS_KERNEL_STACK_SIZE,
              PROCESS_KERNEL_STACK_SIZE);
    if (mode_ == Mode::User) {
        LOG_DEBUG("  User Stack:   0x%X - 0x%X (Size: %d pages)",
                  reinterpret_cast<uint32_t>(userStack_),
                  reinterpret_cast<uint32_t>(userStack_) + PAGE_SIZE * PROCESS_USER_STACK_SIZE,
                  PROCESS_USER_STACK_SIZE);
    }
    LOG_DEBUG("  Paging Directory: 0x%X", reinterpret_cast<uint32_t>(pagingDirectory_->getDirectory()));
    LOG_DEBUG("  Kernel Space: 0x%X - 0x%X (Size: %d pages)", nullptr, nullptr, kernel::kernelLastPage);
}

void PalmyraOS::kernel::Process::initializePagingDirectory(Process::Mode mode, bool isInternal) {
    // 1. Create and map the paging directory to itself based on the process mode.
    LOG_DEBUG("Creating Paging Directory. Mode: %s, Is Internal: %d", mode == Process::Mode::Kernel ? "Kernel" : "User", isInternal);

    if (mode == Process::Mode::Kernel) {
        // For kernel mode, use the kernel's paging directory.
        pagingDirectory_ = kernel::kernelPagingDirectory_ptr;
    }
    else {
        // For user mode, allocate a new paging directory.
        uint32_t PagingDirectoryFrames = (sizeof(PagingDirectory) >> PAGE_BITS) + 1;
        pagingDirectory_               = static_cast<PagingDirectory*>(kernelPagingDirectory_ptr->allocatePages(PagingDirectoryFrames));
        new (pagingDirectory_) PagingDirectory();

        registerPages(pagingDirectory_, PagingDirectoryFrames);
        pagingDirectory_->mapPages(pagingDirectory_, pagingDirectory_, PagingDirectoryFrames, PageFlags::Present | PageFlags::ReadWrite | PageFlags::UserSupervisor);
    }
    // Page directory is initialized

    // 2. Map the kernel stack for both kernel and user mode processes.
    LOG_DEBUG("Mapping Kernel Stack. Size: %d pages", PROCESS_KERNEL_STACK_SIZE);
    kernelStack_ = kernelPagingDirectory_ptr->allocatePages(PROCESS_KERNEL_STACK_SIZE);
    registerPages(kernelStack_, PROCESS_KERNEL_STACK_SIZE);
    pagingDirectory_->mapPages(kernelStack_,
                               kernelStack_,
                               PROCESS_KERNEL_STACK_SIZE,
                               PageFlags::Present | PageFlags::ReadWrite  //| PageFlags::UserSupervisor // TODO investigation with ELF
    );
    uint32_t kernelStackStart = reinterpret_cast<uint32_t>(kernelStack_);
    uint32_t kernelStackEnd   = kernelStackStart + (PAGE_SIZE * PROCESS_KERNEL_STACK_SIZE);
    LOG_INFO("Kernel Stack [PID %d] at 0x%X - 0x%X (size %d pages / %d bytes)",
             pid_,
             kernelStackStart,
             kernelStackEnd,
             PROCESS_KERNEL_STACK_SIZE,
             PAGE_SIZE * PROCESS_KERNEL_STACK_SIZE);

    // 3. If the process is in user mode, set up the user stack.
    if (mode == Process::Mode::User) {
        // Allocate and map the user stack.
        LOG_DEBUG("Mapping User Stack. Size: %d pages", PROCESS_USER_STACK_SIZE);
        userStack_              = allocatePages(PROCESS_USER_STACK_SIZE);
        uint32_t userStackStart = reinterpret_cast<uint32_t>(userStack_);
        uint32_t userStackEnd   = userStackStart + (PAGE_SIZE * PROCESS_USER_STACK_SIZE);
        LOG_INFO("User Stack [PID %d] at 0x%X - 0x%X (size %d pages / %d bytes)", pid_, userStackStart, userStackEnd, PROCESS_USER_STACK_SIZE, PAGE_SIZE * PROCESS_USER_STACK_SIZE);

        // registers pages automatically
        PageFlags kernelSpaceFlags = PageFlags::Present | PageFlags::ReadWrite;
        if (isInternal) kernelSpaceFlags = kernelSpaceFlags | PageFlags::UserSupervisor;

        // The kernel is still mapped, but only accessed in user mode for internal applications.
        LOG_DEBUG("Mapping Kernel Space. Size: %d pages", kernel::kernelLastPage);
        pagingDirectory_->mapPages(nullptr, nullptr, kernel::kernelLastPage, kernelSpaceFlags);
    }
}

void PalmyraOS::kernel::Process::initializeCPUState() {
    // Determine the data and code segment selectors based on the mode.
    uint32_t dataSegment     = mode_ == Mode::Kernel ? gdt_ptr->getKernelDataSegmentSelector().withRPL(GDT::PrivilegeLevel::Ring0)
                                                     : gdt_ptr->getUserDataSegmentSelector().withRPL(GDT::PrivilegeLevel::Ring3);

    uint32_t codeSegment     = mode_ == Mode::Kernel ? gdt_ptr->getKernelCodeSegmentSelector().withRPL(GDT::PrivilegeLevel::Ring0)
                                                     : gdt_ptr->getUserCodeSegmentSelector().withRPL(GDT::PrivilegeLevel::Ring3);

    // Set the data segment selectors in the process's stack.
    // This sets the GS, FS, ES, DS, and SS segment registers.
    stack_.gs                = dataSegment;
    stack_.fs                = dataSegment;
    stack_.es                = dataSegment;
    stack_.ds                = dataSegment;
    stack_.ss                = dataSegment;  // Only for user mode

    // Set the code segment selector in the process's stack.
    stack_.cs                = codeSegment;

    // The general-purpose registers are initialized to 0 by default.

    // Initialize the stack pointer (ESP) and instruction pointer (EIP).
    stack_.esp               = reinterpret_cast<uint32_t>(kernelStack_) + PAGE_SIZE * PROCESS_KERNEL_STACK_SIZE;
    stack_.eip               = reinterpret_cast<uint32_t>(dispatcher);

    uint32_t kernelStackBase = reinterpret_cast<uint32_t>(kernelStack_);
    uint32_t kernelStackTop  = kernelStackBase + PAGE_SIZE * PROCESS_KERNEL_STACK_SIZE;
    LOG_DEBUG("[PID %d] Kernel ESP initialized: 0x%X (Stack base: 0x%X, Stack top: 0x%X)", pid_, stack_.esp, kernelStackBase, kernelStackTop);

    // Set the EFLAGS register, enabling interrupts and setting reserved bits.
    stack_.eflags = (1 << 1) | (1 << static_cast<uint32_t>(EFlags::IF_Interrupt));

    // For user mode, initialize the user stack pointer (userEsp).
    if (mode_ == Mode::User) {
        uint32_t userStackBase = reinterpret_cast<uint32_t>(userStack_);
        uint32_t userStackTop  = userStackBase + PAGE_SIZE * PROCESS_USER_STACK_SIZE;

        // Reserve a 512-byte red zone at the top of the user stack for two purposes:
        // 1. Overflow detection: If ESP grows into this region, it indicates critical stack shortage.
        // 2. Argument buffer: Guarantees sufficient space for argc/argv setup in initializeArgumentsForELF()
        //    without conflicting with the physical stack boundary.
        // The red zone is explicitly zeroed below to create a detectable boundary pattern.
        stack_.userEsp         = userStackTop - 512;
        LOG_DEBUG("[PID %d] User ESP initialized: 0x%X (Stack base: 0x%X, Stack top: 0x%X)", pid_, stack_.userEsp, userStackBase, userStackTop);

        // Fill the red zone memory range with 0 from `userEsp` to the top of the user stack
        // This creates a detectable guard region that should remain zero if the stack is healthy.
        auto* userStackStart = reinterpret_cast<uint32_t*>(userStackTop - 512);
        auto* userStackEnd   = reinterpret_cast<uint32_t*>(userStackTop);
        std::fill(userStackStart, userStackEnd, 0);
    }

    // Set the CR3 register to point to the process's paging directory.
    stack_.cr3 = reinterpret_cast<uint32_t>(pagingDirectory_->getDirectory());
}

/**
 * @brief Initializes arguments for builtin (internal) executables
 *
 * Builtin executables are kernel functions exposed as user programs (e.g., terminal.elf).
 * They use a dispatcher wrapper with a special Arguments struct, NOT the standard
 * Linux stack layout. The environment is captured but NOT pushed to the stack
 * (builtins don't use envp directly; it's only for /proc metadata).
 */
void PalmyraOS::kernel::Process::initializeArguments(ProcessEntry entry, uint32_t argc, char* const* argv, char* const* envp) {
    LOG_DEBUG("[Process %d] Initializing builtin executable arguments (argc=%d)", pid_, argc);

    /**
     * Calculate total memory needed for argv.
     * We DON'T push envp for builtins (they use dispatcher, not standard main()).
     */

    // 1. Space for argv pointers + NULL terminator
    size_t totalSize = (argc + 1) * sizeof(char*);

    // 2. Space for argument strings
    for (uint32_t i = 0; i < argc; ++i) { totalSize += strlen(argv[i]) + 1; }

    /**
     * Allocate a single contiguous block for all argv data.
     * Layout: [argv pointers array] [argument strings]
     */
    size_t numPages  = (totalSize + PAGE_SIZE - 1) >> PAGE_BITS;
    void* argv_block = allocatePages(numPages);
    debug_.argvBlock = reinterpret_cast<uint32_t>(argv_block);

    // Set up pointers within the allocated block
    char** argv_copy = static_cast<char**>(argv_block);
    char* str_copy   = reinterpret_cast<char*>(argv_block) + (argc + 1) * sizeof(char*);

    // Copy argument strings
    for (uint32_t i = 0; i < argc; ++i) {
        argv_copy[i] = str_copy;             // Point argv[i] to the string location
        size_t len   = strlen(argv[i]) + 1;  // Include null terminator
        memcpy(str_copy, argv[i], len);      // Copy string
        str_copy += len;                     // Advance to next string
    }
    argv_copy[argc] = nullptr;  // NULL terminator for argv array

    /**
     * Set up the Arguments struct based on execution mode.
     * Kernel mode: Use kernel stack (esp)
     * User mode: Use user stack (userEsp)
     */
    if (mode_ == Mode::Kernel) {
        // Push Arguments struct onto kernel stack
        stack_.esp -= sizeof(Arguments);
        auto* processArgs       = reinterpret_cast<Arguments*>(stack_.esp);
        processArgs->entryPoint = entry;
        processArgs->argc       = argc;
        processArgs->argv       = argv_copy;

        // In kernel mode, ss register holds the stack pointer
        stack_.ss               = stack_.esp;

        LOG_DEBUG("[Process %d] Kernel builtin initialized. ESP: 0x%X", pid_, stack_.esp);
    }
    else {
        // Push Arguments struct onto user stack
        stack_.userEsp -= sizeof(Arguments);
        auto* processArgs       = reinterpret_cast<Arguments*>(stack_.userEsp);
        processArgs->entryPoint = entry;
        processArgs->argc       = argc;
        processArgs->argv       = argv_copy;

        // Push pointer to Arguments struct
        stack_.userEsp -= sizeof(Arguments*);
        auto* processArgsPtr = reinterpret_cast<Arguments**>(stack_.userEsp);
        *processArgsPtr      = processArgs;

        // Adjust for calling convention (first argument at esp + 4)
        stack_.userEsp -= 4;

        LOG_DEBUG("[Process %d] User builtin initialized. ESP: 0x%X", pid_, stack_.userEsp);
    }
}

void PalmyraOS::kernel::Process::terminate(int exitCode) {
    state_    = Process::State::Terminated;
    exitCode_ = exitCode;
}

void PalmyraOS::kernel::Process::kill() {
    state_ = State::Killed;
    age_   = 0;
    // exitCode_ is set by _exit syscall

    // clean up memory
    for (auto& physicalPage: physicalPages_) { kernel::kernelPagingDirectory_ptr->freePage(physicalPage); }
    physicalPages_.clear();

    // clean up windows buffers
    for (auto windowID: windows_) { WindowManager::closeWindow(windowID); }
    windows_.clear();

    // TODO free directory table arrays if user process
}

void PalmyraOS::kernel::Process::dispatcher(PalmyraOS::kernel::Process::Arguments* args) {
    // This is executed in user mode (for user processes)

    // Call the entry point with the given arguments
    uint32_t exitCode = args->entryPoint(args->argc, args->argv);

    // should not arrive here, but just in cse
    _exit(exitCode);
}

void PalmyraOS::kernel::Process::registerPages(void* physicalAddress, size_t count) {
    for (int i = 0; i < count; ++i) {
        uint32_t address = (uint32_t) physicalAddress + (i << PAGE_BITS);
        physicalPages_.push_back((void*) address);
    }
}

void PalmyraOS::kernel::Process::deregisterPages(void* physicalAddress, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        uint32_t address = (uint32_t) physicalAddress + (i << PAGE_BITS);
        auto it          = std::find(physicalPages_.begin(), physicalPages_.end(), (void*) address);
        if (it != physicalPages_.end()) {
            physicalPages_.erase(it);
            kernel::kernelPagingDirectory_ptr->freePage((void*) address);
        }
    }
}

void* PalmyraOS::kernel::Process::allocatePages(size_t count) {
    // allocate the pages in kernel directory (so that they are accessible in syscalls)
    void* address = kernelPagingDirectory_ptr->allocatePages(count);

    // register them to keep track of them when we terminate
    registerPages(address, count);

    // Make them accessible to the process
    pagingDirectory_->mapPages(address, address, count, PageFlags::Present | PageFlags::ReadWrite | PageFlags::UserSupervisor);

    return address;
}

bool PalmyraOS::kernel::Process::checkStackOverflow() const {
    /*
     * Check for kernel stack overflow.
     * The kernel stack pointer (esp) should never be below the base of the kernel stack (kernelStack_).
     */
    if (stack_.esp < (reinterpret_cast<uint32_t>(kernelStack_))) {
        kernel::kernelPanic("Kernel Stack Overflow detected for PID: %d.\nESP: 0x%x is below Kernel Stack Base: 0x%x", pid_, stack_.esp, reinterpret_cast<uint32_t>(kernelStack_));
    }

    /*
     * When in user mode and executing in user space, we need to ensure that the user stack pointer (userEsp)
     * is above the base of the user stack (userStack_). This prevents user stack overflow.
     */
    if (mode_ == Mode::User && (stack_.cs & 0x11) != 0) {
        if (stack_.userEsp < (reinterpret_cast<uint32_t>(userStack_))) {

            kernel::kernelPanic("User Stack Overflow detected for PID: %d. User ESP: 0x%x is below User Stack Base: 0x%x",
                                pid_,
                                stack_.userEsp,
                                reinterpret_cast<uint32_t>(userStack_));
            return false;
        }
    }

    return true;
}

void* PalmyraOS::kernel::Process::allocatePagesAt(void* virtual_address, size_t count) {
    // allocate the pages in kernel directory (so that they are accessible in syscalls)
    void* physicalAddress = kernelPagingDirectory_ptr->allocatePages(count);

    // register them to keep track of them when we terminate
    registerPages(physicalAddress, count);

    // Make them accessible to the process
    pagingDirectory_->mapPages(physicalAddress, virtual_address, count, PageFlags::Present | PageFlags::ReadWrite | PageFlags::UserSupervisor);

    return physicalAddress;
}

/**
 * @brief Initializes arguments for ELF executables with Linux-compatible stack layout
 *
 * This function creates the standard Linux i386 process stack layout at process startup.
 * The stack structure follows the System V ABI specification and matches what Linux
 * kernels provide, ensuring compatibility with standard toolchains and C runtime libraries.
 *
 * Stack layout (from low to high address):
 *   argc (4 bytes)
 *   argv[0..argc] + NULL
 *   envp[0..envc] + NULL
 *   auxv[0..n] + AT_NULL
 *   [argument strings]
 *   [environment strings]
 */
void PalmyraOS::kernel::Process::initializeArgumentsForELF(uint32_t argc, char* const* argv, char* const* envp) {
    LOG_DEBUG("[Process %d] Initializing ELF executable with Linux stack layout (argc=%d)", pid_, argc);

    /**
     * Step 1: Get environment count from our map
     *
     * We use environmentMap_ as the single source of truth, not the envp parameter.
     * This ensures that default environment variables (added in captureEnvironment) are
     * always available to the process, even if nullptr was passed.
     */
    uint32_t envc = environmentMap_.size();
    LOG_DEBUG("[Process %d] Environment count: %d", pid_, envc);

    /**
     * Step 2: Calculate total memory needed for all data
     *
     * We allocate a single contiguous block to hold:
     * - argv pointer array
     * - envp pointer array
     * - auxiliary vector
     * - all string data
     */
    size_t totalSize = 0;

    // 1. Argv pointers + NULL terminator
    totalSize += (argc + 1) * sizeof(char*);

    // 2. Envp pointers + NULL terminator
    totalSize += (envc + 1) * sizeof(char*);

    // 3. Auxiliary vector (type-value pairs + AT_NULL)
    totalSize += (auxiliaryVector_.size() + 1) * 2 * sizeof(uint32_t);

    // 4. Argument strings
    for (uint32_t i = 0; i < argc; ++i) { totalSize += strlen(argv[i]) + 1; }

    // 5. Environment strings (serialize from map: "KEY=VALUE" format)
    for (const auto& [key, value]: environmentMap_) {
        totalSize += key.size() + 1 + value.size() + 1;  // "KEY" + "=" + "VALUE" + '\0'
    }

    // 6. Platform string for AT_PLATFORM auxiliary vector entry
    const char* platform_string = "i386";
    totalSize += strlen(platform_string) + 1;

    /**
     * Step 3: Allocate memory block and set up layout pointers
     */
    size_t numPages = (totalSize + PAGE_SIZE - 1) >> PAGE_BITS;
    void* block     = allocatePages(numPages);

    LOG_DEBUG("[Process %d] Allocated %d pages (%d bytes) for ELF stack data", pid_, numPages, totalSize);

    // Layout memory: [argv] [envp] [auxv] [strings]
    char** argv_copy = static_cast<char**>(block);
    char** envp_copy = argv_copy + argc + 1;
    uint32_t* auxv   = reinterpret_cast<uint32_t*>(envp_copy + envc + 1);
    char* str_copy   = reinterpret_cast<char*>(auxv + (auxiliaryVector_.size() + 1) * 2);

    /**
     * Step 4: Copy argv strings and build argv pointer array
     */
    for (uint32_t i = 0; i < argc; ++i) {
        argv_copy[i] = str_copy;             // Point to string
        size_t len   = strlen(argv[i]) + 1;  // Include null terminator
        memcpy(str_copy, argv[i], len);      // Copy string
        str_copy += len;                     // Advance pointer
    }
    argv_copy[argc]    = nullptr;  // NULL terminator

    /**
     * Step 5: Serialize environment from map and build envp pointer array
     *
     * We serialize from environmentMap_ (our single source of truth) into "KEY=VALUE" format.
     * This ensures default environment variables are always available and prevents duplicates.
     */
    uint32_t env_index = 0;
    for (const auto& [key, value]: environmentMap_) {
        envp_copy[env_index] = str_copy;  // Point to string

        // Build "KEY=VALUE" string
        // 1. Copy key
        size_t key_len       = key.size();
        memcpy(str_copy, key.c_str(), key_len);
        str_copy += key_len;

        // 2. Copy '='
        *str_copy = '=';
        str_copy++;

        // 3. Copy value
        size_t value_len = value.size();
        memcpy(str_copy, value.c_str(), value_len);
        str_copy += value_len;

        // 4. Null terminator
        *str_copy = '\0';
        str_copy++;

        env_index++;
    }
    envp_copy[envc]     = nullptr;  // NULL terminator

    /**
     * Step 5.5: Copy platform string and update AT_PLATFORM entry
     *
     * The platform string identifies the CPU architecture (e.g., "i386", "i686").
     * Programs and libraries use this to select optimized code paths at runtime.
     */
    char* platform_ptr  = str_copy;
    size_t platform_len = strlen(platform_string) + 1;
    memcpy(platform_ptr, platform_string, platform_len);
    str_copy += platform_len;

    // Find and update the AT_PLATFORM entry in our auxiliary vector
    for (size_t i = 0; i < auxiliaryVector_.size(); ++i) {
        if (auxiliaryVector_[i].type == AT_PLATFORM) {
            auxiliaryVector_[i].value = reinterpret_cast<uint32_t>(platform_ptr);
            LOG_DEBUG("[Process %d] Set AT_PLATFORM to '%s' at 0x%X", pid_, platform_string, platform_ptr);
            break;
        }
    }

    /**
     * Step 6: Build auxiliary vector (type-value pairs)
     *
     * The auxiliary vector is an array of (type, value) pairs terminated by AT_NULL.
     * Programs iterate through it sequentially until they find AT_NULL.
     */
    for (size_t i = 0; i < auxiliaryVector_.size(); ++i) {
        auxv[i * 2]     = auxiliaryVector_[i].type;   // AT_* constant
        auxv[i * 2 + 1] = auxiliaryVector_[i].value;  // Corresponding value
    }
    // Add AT_NULL terminator
    auxv[auxiliaryVector_.size() * 2]     = AT_NULL;
    auxv[auxiliaryVector_.size() * 2 + 1] = 0;

    /**
     * Step 7: Push data to user stack in Linux ABI order
     *
     * Stack grows downward (toward lower addresses), so we push in reverse:
     * - Auxiliary vector (highest address)
     * - Environment pointers
     * - Argument pointers
     * - Argument count (lowest address, ESP points here)
     */

    // Push auxiliary vector
    size_t auxv_size                      = (auxiliaryVector_.size() + 1) * 2 * sizeof(uint32_t);
    stack_.userEsp -= auxv_size;
    memcpy(reinterpret_cast<void*>(stack_.userEsp), auxv, auxv_size);
    LOG_DEBUG("[Process %d] Pushed auxv at 0x%X (size: %d bytes, %d entries)", pid_, stack_.userEsp, auxv_size, auxiliaryVector_.size());

    // Push envp array
    stack_.userEsp -= (envc + 1) * sizeof(char*);
    memcpy(reinterpret_cast<void*>(stack_.userEsp), envp_copy, (envc + 1) * sizeof(char*));
    LOG_DEBUG("[Process %d] Pushed envp at 0x%X (%d entries)", pid_, stack_.userEsp, envc);

    // Push argv array
    stack_.userEsp -= (argc + 1) * sizeof(char*);
    memcpy(reinterpret_cast<void*>(stack_.userEsp), argv_copy, (argc + 1) * sizeof(char*));
    LOG_DEBUG("[Process %d] Pushed argv at 0x%X (%d entries)", pid_, stack_.userEsp, argc);

    // Push argc
    stack_.userEsp -= sizeof(uint32_t);
    *reinterpret_cast<uint32_t*>(stack_.userEsp) = argc;
    LOG_DEBUG("[Process %d] Pushed argc at 0x%X (value: %d)", pid_, stack_.userEsp, argc);

    LOG_DEBUG("[Process %d] ELF stack initialization complete. Final ESP: 0x%X", pid_, stack_.userEsp);
}

void PalmyraOS::kernel::Process::initializeProcessInVFS() {

    LOG_DEBUG("Initializing VFS hooks");

    char buffer[50];
    snprintf(buffer, sizeof(buffer), "/proc/%d", pid_);

    KString directory = KString(buffer);

    vfs::VirtualFileSystem::createDirectory(directory, vfs::InodeBase::Mode::USER_READ);

    auto statusNode = kernel::heapManager.createInstance<vfs::FunctionInode>(
            // Read
            [this](char* buffer, size_t size, size_t offset) -> size_t {
                char uptime[40];
                uitoa64(upTime_, uptime, 10, false);

                // Create a string with the desired format
                char output[512];
                size_t written = snprintf(output,
                                          sizeof(output),
                                          ""
                                          "Pid: %d\n"
                                          "Name: %s\n"
                                          "State: %s\n"
                                          "Up Time: %s\n"
                                          "Pages: %d\n"
                                          "Windows: %d\n"
                                          "exitCode: %d\n",
                                          pid_,
                                          commandName_.c_str(),
                                          stateToString(),
                                          uptime,
                                          physicalPages_.size(),
                                          windows_.size(),
                                          exitCode_);

                // Ensure the output fits in the buffer, considering the offset
                if (offset >= written) return 0;

                size_t available = written - offset;
                size_t to_copy   = available < size ? available : size;
                memcpy(buffer, output + offset, to_copy);

                return to_copy;
            },
            nullptr,
            nullptr);
    vfs::VirtualFileSystem::setInodeByPath(directory + KString("/status"), statusNode);

    auto stdoutNode = kernel::heapManager.createInstance<vfs::FunctionInode>(
            // Read
            [this](char* buffer, size_t size, size_t offset) -> size_t {
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
            },
            nullptr,
            nullptr);
    vfs::VirtualFileSystem::setInodeByPath(directory + KString("/stdout"), stdoutNode);

    auto stderrNode = kernel::heapManager.createInstance<vfs::FunctionInode>(
            // Read
            [this](char* buffer, size_t size, size_t offset) -> size_t {
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
            },
            nullptr,
            nullptr);
    vfs::VirtualFileSystem::setInodeByPath(directory + KString("/stderr"), stderrNode);

    /// Command-line arguments file - Linux compatible format (null-terminated strings)
    auto cmdlineNode = kernel::heapManager.createInstance<vfs::FunctionInode>(
            // Read function: serializes cmdline to null-terminated format
            [this](char* buffer, size_t size, size_t offset) -> size_t {
                // Serialize command-line arguments to buffer (Linux /proc/pid/cmdline format)
                char serialized[512];
                size_t serialized_len = this->serializeCmdline(serialized, sizeof(serialized));

                // Handle offset into the serialized data
                if (offset >= serialized_len) return 0;

                // Calculate bytes to copy
                size_t available = serialized_len - offset;
                size_t to_copy   = available < size ? available : size;

                // Copy to output buffer
                memcpy(buffer, serialized + offset, to_copy);

                return to_copy;
            },
            nullptr,
            nullptr);
    vfs::VirtualFileSystem::setInodeByPath(directory + KString("/cmdline"), cmdlineNode);

    /// Environment variables file - Linux compatible format (null-terminated KEY=VALUE strings)
    auto environNode = kernel::heapManager.createInstance<vfs::FunctionInode>(
            // Read function: serializes environment to null-terminated format
            [this](char* buffer, size_t size, size_t offset) -> size_t {
                /**
                 * Serialize environment from map to "KEY1=VALUE1\0KEY2=VALUE2\0" format.
                 * This matches Linux /proc/pid/environ format.
                 */
                char serialized[2048];  // Larger buffer for environment
                size_t written = 0;

                // Serialize each environment variable
                for (const auto& [key, value]: environmentMap_) {
                    // Check space for "KEY=VALUE\0"
                    size_t needed = key.size() + 1 + value.size() + 1;
                    if (written + needed > sizeof(serialized)) break;

                    // Copy key
                    memcpy(serialized + written, key.c_str(), key.size());
                    written += key.size();

                    // Copy '='
                    serialized[written++] = '=';

                    // Copy value
                    memcpy(serialized + written, value.c_str(), value.size());
                    written += value.size();

                    // Null terminator
                    serialized[written++] = '\0';
                }

                // Handle offset into the serialized data
                if (offset >= written) return 0;

                // Calculate bytes to copy
                size_t available = written - offset;
                size_t to_copy   = available < size ? available : size;

                // Copy to output buffer
                memcpy(buffer, serialized + offset, to_copy);

                return to_copy;
            },
            nullptr,
            nullptr);
    vfs::VirtualFileSystem::setInodeByPath(directory + KString("/environ"), environNode);

    /// Linux-compatible process statistics file
    auto statNode = kernel::heapManager.createInstance<vfs::FunctionInode>(
            // Read function: serializes process stats in Linux /proc/pid/stat format
            [this](char* buffer, size_t size, size_t offset) -> size_t {
                // Serialize process statistics in Linux-compatible format
                char serialized[512];
                size_t serialized_len = this->serializeStat(serialized, sizeof(serialized));

                // Handle offset into the serialized data
                if (offset >= serialized_len) return 0;

                // Calculate bytes to copy
                size_t available = serialized_len - offset;
                size_t to_copy   = available < size ? available : size;

                // Copy to output buffer
                memcpy(buffer, serialized + offset, to_copy);

                return to_copy;
            },
            nullptr,
            nullptr);
    vfs::VirtualFileSystem::setInodeByPath(directory + KString("/stat"), statNode);
}

/// Helper Methods for Command-line Metadata

/**
 * @brief Converts process state to Linux-compatible single character representation
 *
 * Maps PalmyraOS process states to Linux standard state characters:
 * - 'R' : Running (State::Running)
 * - 'S' : Sleeping/Ready (State::Ready)
 * - 'Z' : Zombie (State::Terminated, awaiting cleanup)
 * - 'X' : Dead (State::Killed, cleaned up)
 * - '?' : Unknown state (fallback)
 *
 * @return Single character representing the process state (Linux compatible)
 */
char PalmyraOS::kernel::Process::stateToChar() const {
    switch (state_) {
        case State::Running: return 'R';     // Currently executing
        case State::Ready: return 'S';       // Ready/sleeping (could be either)
        case State::Terminated: return 'Z';  // Terminated, awaiting cleanup (zombie)
        case State::Killed: return 'X';      // Killed, fully cleaned up
        case State::Waiting: return 'D';     // Waiting on I/O
        default: return '?';                 // Unknown state
    }
}

/**
 * @brief Captures command-line arguments at process creation time (safe for later access)
 *
 * This elegant solution stores the argv array safely at process construction,
 * enabling safe access later via /proc/{pid}/cmdline without worrying about
 * whether the original argv pointers are still valid (especially important for
 * user-mode processes where argv lives on the user stack).
 */
void PalmyraOS::kernel::Process::captureCommandlineArguments(uint32_t argc, char* const* argv) {
    // Guard: Validate inputs
    if (!argv || argc == 0) return;

    // Extract and store the command name (argv[0])
    if (argv[0]) { commandName_ = KString(argv[0]); }

    // Capture all arguments as safe copies
    commandlineArgs_.clear();
    commandlineArgs_.reserve(argc);  // Pre-allocate for efficiency

    for (uint32_t i = 0; i < argc; ++i) {
        if (argv[i]) { commandlineArgs_.push_back(KString(argv[i])); }
    }

    LOG_DEBUG("[Process %d] Captured %d command-line arguments: %s", pid_, argc, commandName_.c_str());
}

/**
 * @brief Captures environment variables for process metadata and /proc filesystem access
 *
 * Environment variables are parsed from "KEY=VALUE" format and stored as a key-value map.
 * This provides O(log n) lookup performance and prevents duplicate keys automatically.
 * If no environment is provided, sensible defaults are used.
 */
void PalmyraOS::kernel::Process::captureEnvironment(char* const* envp) {
    environmentMap_.clear();

    /**
     * If no environment is provided, use minimal POSIX-compatible defaults.
     * This ensures programs have at least basic environment variables available.
     */
    if (!envp) {
        LOG_DEBUG("[Process %d] No environment provided, using minimal defaults", pid_);

        // Minimal POSIX environment
        environmentMap_[KString("PATH")]  = KString("/bin");
        environmentMap_[KString("HOME")]  = KString("/");
        environmentMap_[KString("USER")]  = KString("root");
        environmentMap_[KString("SHELL")] = KString("/bin/terminal.elf");

        return;
    }

    /**
     * Parse environment variables from the provided array.
     * Each entry must be in KEY=VALUE format (e.g., "PATH=/bin:/usr/bin").
     * Invalid entries (without '=') are silently ignored.
     */
    uint32_t count = 0;
    while (envp[count] != nullptr) {
        const char* entry  = envp[count];
        const char* equals = strchr(entry, '=');

        if (equals) {
            // Split into key and value
            KString key(entry, equals - entry);  // Everything before '='
            KString value(equals + 1);           // Everything after '='

            // Store in map (automatically handles duplicates - last one wins)
            environmentMap_[key] = value;
        }
        else { LOG_WARN("[Process %d] Invalid environment entry (no '='): %s", pid_, entry); }

        count++;
    }

    LOG_DEBUG("[Process %d] Captured %d environment variables into map", pid_, environmentMap_.size());
}

/**
 * @brief Builds auxiliary vector for ELF process initialization
 *
 * The auxiliary vector provides essential metadata from the kernel to userspace
 * programs. Dynamic linkers (ld.so) and C runtime libraries rely on this information
 * to properly initialize the process environment.
 *
 * This implementation follows the Linux i386 ABI for compatibility with standard
 * toolchains and dynamically-linked executables.
 */
void PalmyraOS::kernel::Process::buildAuxiliaryVectorForELF(const Elf32_Ehdr* elfHeader, const Elf32_Phdr* programHeaders) {
    auxiliaryVector_.clear();

    /**
     * Build the auxiliary vector with essential entries.
     * The order here doesn't matter (programs iterate until AT_NULL),
     * but we follow a logical grouping for maintainability.
     */

    // 1. System information
    auxiliaryVector_.push_back({AT_PAGESZ, PAGE_SIZE});  // Page size (typically 4096)
    auxiliaryVector_.push_back({AT_CLKTCK, 100});        // Clock ticks per second (PIT frequency)

    // 2. ELF program header information (critical for dynamic linking)
    auxiliaryVector_.push_back({AT_PHDR, reinterpret_cast<uint32_t>(programHeaders)});  // Program headers address
    auxiliaryVector_.push_back({AT_PHENT, sizeof(Elf32_Phdr)});                         // Size of one program header
    auxiliaryVector_.push_back({AT_PHNUM, elfHeader->e_phnum});                         // Number of program headers

    // 3. Entry point (useful for debuggers and profilers)
    auxiliaryVector_.push_back({AT_ENTRY, elfHeader->e_entry});

    // 4. Platform string (CPU architecture identifier)
    // Value will be updated in initializeArgumentsForELF() to point to allocated string
    auxiliaryVector_.push_back({AT_PLATFORM, 0});  // Placeholder, updated during stack setup

    // 5. User/Group IDs (security context)
    // TODO: Implement proper UID/GID system; hardcoded to root (0) for now
    auxiliaryVector_.push_back({AT_UID, 0});   // Real user ID
    auxiliaryVector_.push_back({AT_EUID, 0});  // Effective user ID
    auxiliaryVector_.push_back({AT_GID, 0});   // Real group ID
    auxiliaryVector_.push_back({AT_EGID, 0});  // Effective group ID

    // 6. Security flag (not setuid/setgid)
    auxiliaryVector_.push_back({AT_SECURE, 0});

    /**
     * Future enhancements (not yet implemented):
     * - AT_BASE: Dynamic linker base address (for ld.so support)
     * - AT_RANDOM: 16 random bytes for stack canaries and ASLR
     * - AT_HWCAP: Hardware capability flags (SSE, SSE2, etc.)
     * - AT_EXECFN: Full path to executable
     */

    LOG_DEBUG("[Process %d] Built auxiliary vector with %d entries for ELF process", pid_, auxiliaryVector_.size());

    // Note: AT_NULL terminator and AT_PLATFORM string pointer are set during stack setup
}

/**
 * @brief Serializes command-line arguments to null-terminated format (Linux /proc/pid/cmdline style)
 *
 * This method generates a serialized representation of the command-line arguments
 * in the standard POSIX format used by Linux: "arg1\0arg2\0arg3\0"
 *
 * @param buffer Output buffer to write the serialized cmdline
 * @param bufferSize Maximum size of the output buffer
 * @return Number of bytes written to the buffer
 */
size_t PalmyraOS::kernel::Process::serializeCmdline(char* buffer, size_t bufferSize) const {
    if (!buffer || bufferSize == 0) return 0;

    size_t written = 0;

    // Serialize each argument, separated by null terminators
    for (const auto& arg: commandlineArgs_) {
        const char* str = arg.c_str();
        size_t argLen   = arg.size();

        // Check if we have enough space for this argument + null terminator
        if (written + argLen + 1 > bufferSize) break;

        // Copy argument to buffer
        memcpy(buffer + written, str, argLen);
        written += argLen;

        // Add null terminator
        buffer[written++] = '\0';
    }

    return written;
}


/**
 * @brief Serializes process statistics in Linux /proc/pid/stat format
 *
 * Generates a space-separated line compatible with Linux stat format (minimal fields).
 * This enables standard Linux tools to read PalmyraOS /proc files.
 *
 * Format (24 fields):
 *   pid (comm) state ppid pgrp session tty_nr tpgid flags minflt cminflt
 *   majflt cmajflt utime stime cutime cstime priority nice num_threads
 *   itrealvalue starttime vsize rss
 *
 * @param buffer Output buffer to write the serialized stat
 * @param bufferSize Maximum size of the output buffer
 * @param totalSystemTicks Total system CPU ticks (unused for now, for future expansion)
 * @return Number of bytes written to the buffer
 */
size_t PalmyraOS::kernel::Process::serializeStat(char* buffer, size_t bufferSize, uint64_t totalSystemTicks) const {
    if (!buffer || bufferSize == 0) return 0;

    // Use snprintf to safely format the stat line
    size_t written = snprintf(buffer,
                              bufferSize,
                              "%d (%s) %c %d %d %d %d %d %u %u %u %u %lu %lu %u %u %d %d %u %u %lu %lu %lu\n",
                              pid_,                         // 1: pid
                              commandName_.c_str(),         // 2: (comm)
                              stateToChar(),                // 3: state
                              0,                            // 4: ppid
                              0,                            // 5: pgrp
                              0,                            // 6: session
                              0,                            // 7: tty_nr
                              0,                            // 8: tpgid
                              0,                            // 9: flags
                              0,                            // 10: minflt
                              0,                            // 11: cminflt
                              0,                            // 12: majflt
                              0,                            // 13: cmajflt
                              cpuTimeTicks_,                // 14: utime (user CPU time in ticks)
                              0,                            // 15: stime (system CPU time - not separated yet)
                              0,                            // 16: cutime
                              0,                            // 17: cstime
                              static_cast<int>(priority_),  // 18: priority
                              0,                            // 19: nice
                              1,                            // 20: num_threads
                              0,                            // 21: itrealvalue (obsolete)
                              startTime_,                   // 22: starttime (ticks since boot)
                              0,                            // 23: vsize (virtual memory size)
                              physicalPages_.size());       // 24: rss (resident set size in pages)

    // snprintf returns the number of characters that would have been written
    // Return actual bytes written (capped by bufferSize)
    return (written < bufferSize) ? written : bufferSize - 1;
}


/// endregion
