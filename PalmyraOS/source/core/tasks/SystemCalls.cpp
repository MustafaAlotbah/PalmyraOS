

#include "core/tasks/SystemCalls.h"
#include "core/Interrupts.h"
#include "core/cpu.h"
#include "libs/memory.h"

// API Headers
#include "palmyraOS/errono.h"
#include "palmyraOS/time.h"
#include "palmyraOS/unistd.h"

// System Objects
#include "core/SystemClock.h"
#include "core/files/BuiltinExecutableInode.h"
#include "core/files/VirtualFileSystem.h"
#include "core/tasks/FileDescriptor.h"
#include "core/tasks/ProcessManager.h"
#include "core/tasks/SocketDescriptor.h"
#include "core/tasks/WindowManager.h"

#include "core/peripherals/Logger.h"
#include "userland/userland.h"


// Define the static member systemCallHandlers_
PalmyraOS::kernel::KMap<uint32_t, PalmyraOS::kernel::SystemCallsManager::SystemCallHandler> PalmyraOS::kernel::SystemCallsManager::systemCallHandlers_;

void PalmyraOS::kernel::SystemCallsManager::initialize() {
    // Setting the interrupt handler for system calls (interrupt 0x80)
    interrupts::InterruptController::setInterruptHandler(0x80, &handleInterrupt);

    // Map system call numbers to their respective handler functions
    // POSIX
    systemCallHandlers_[POSIX_INT_EXIT]               = &SystemCallsManager::handleExit;
    systemCallHandlers_[POSIX_INT_GET_PID]            = &SystemCallsManager::handleGetPid;
    systemCallHandlers_[POSIX_INT_YIELD]              = &SystemCallsManager::handleYield;
    systemCallHandlers_[POSIX_INT_MMAP]               = &SystemCallsManager::handleMmap;
    systemCallHandlers_[POSIX_INT_GETTIME]            = &SystemCallsManager::handleGetTime;
    systemCallHandlers_[POSIX_INT_CLOCK_NANOSLEEP_64] = &SystemCallsManager::handleClockNanoSleep64;
    systemCallHandlers_[POSIX_INT_BRK]                = &SystemCallsManager::handleBrk;
    systemCallHandlers_[POSIX_INT_SETTHREADAREA]      = &SystemCallsManager::handleSetThreadArea;
    systemCallHandlers_[POSIX_INT_GETUID]             = &SystemCallsManager::handleGetUID;
    systemCallHandlers_[POSIX_INT_GETGID]             = &SystemCallsManager::handleGetGID;
    systemCallHandlers_[POSIX_INT_GETEUID32]          = &SystemCallsManager::handleGetEUID;
    systemCallHandlers_[POSIX_INT_GETEGID32]          = &SystemCallsManager::handleGetEUID;
    systemCallHandlers_[POSIX_INT_REBOOT]             = &SystemCallsManager::handleReboot;

    // VFS
    systemCallHandlers_[POSIX_INT_OPEN]               = &SystemCallsManager::handleOpen;
    systemCallHandlers_[POSIX_INT_CLOSE]              = &SystemCallsManager::handleClose;
    systemCallHandlers_[POSIX_INT_WRITE]              = &SystemCallsManager::handleWrite;
    systemCallHandlers_[POSIX_INT_READ]               = &SystemCallsManager::handleRead;
    systemCallHandlers_[POSIX_INT_IOCTL]              = &SystemCallsManager::handleIoctl;
    systemCallHandlers_[POSIX_INT_LSEEK]              = &SystemCallsManager::handleLongSeek;
    systemCallHandlers_[POSIX_INT_MKDIR]              = &SystemCallsManager::handleMkdir;
    systemCallHandlers_[POSIX_INT_RMDIR]              = &SystemCallsManager::handleRmdir;
    systemCallHandlers_[POSIX_INT_UNLINK]             = &SystemCallsManager::handleUnlink;

    // Interprocess
    systemCallHandlers_[POSIX_INT_WAITPID]            = &SystemCallsManager::handleWaitPID;
    systemCallHandlers_[POSIX_INT_POSIX_SPAWN]        = &SystemCallsManager::handleSpawn;

    // Custom
    systemCallHandlers_[INT_INIT_WINDOW]              = &SystemCallsManager::handleInitWindow;
    systemCallHandlers_[INT_CLOSE_WINDOW]             = &SystemCallsManager::handleCloseWindow;
    systemCallHandlers_[INT_NEXT_KEY_EVENT]           = &SystemCallsManager::handleNextKeyboardEvent;
    systemCallHandlers_[INT_NEXT_MOUSE_EVENT]         = &SystemCallsManager::handleNextMouseEvent;
    systemCallHandlers_[INT_GET_WINDOW_STATUS]        = &SystemCallsManager::handleGetWindowStatus;

    // Adopted from Linux
    systemCallHandlers_[LINUX_INT_GETDENTS]           = &SystemCallsManager::handleGetdents;
    systemCallHandlers_[LINUX_INT_PRCTL]              = &SystemCallsManager::handleArchPrctl;

    // Socket syscalls
    systemCallHandlers_[POSIX_INT_SOCKET]             = &SystemCallsManager::handleSocket;
    systemCallHandlers_[POSIX_INT_BIND]               = &SystemCallsManager::handleBind;
    systemCallHandlers_[POSIX_INT_CONNECT]            = &SystemCallsManager::handleConnect;
    systemCallHandlers_[POSIX_INT_LISTEN]             = &SystemCallsManager::handleListen;
    systemCallHandlers_[POSIX_INT_ACCEPT]             = &SystemCallsManager::handleAccept;
    systemCallHandlers_[POSIX_INT_SENDTO]             = &SystemCallsManager::handleSendto;
    systemCallHandlers_[POSIX_INT_RECVFROM]           = &SystemCallsManager::handleRecvfrom;
    systemCallHandlers_[POSIX_INT_SETSOCKOPT]         = &SystemCallsManager::handleSetsockopt;
    systemCallHandlers_[POSIX_INT_GETSOCKOPT]         = &SystemCallsManager::handleGetsockopt;
    systemCallHandlers_[POSIX_INT_GETSOCKNAME]        = &SystemCallsManager::handleGetsockname;
    systemCallHandlers_[POSIX_INT_GETPEERNAME]        = &SystemCallsManager::handleGetpeername;
    systemCallHandlers_[POSIX_INT_SHUTDOWN]           = &SystemCallsManager::handleShutdown;
}

// TODO isValidAddress(void* addr, size_t size) this way we are sure for more than one byte!!
bool PalmyraOS::kernel::SystemCallsManager::isValidAddress(void* addr) {
    // Validate if the given address is within the current process's valid address range

    // Retrieve the current process
    auto* proc = TaskManager::getCurrentProcess();

    // Check if the address is valid in the current process's paging directory
    if (!proc->pagingDirectory_->isAddressValid(addr)) {
        // If the address is invalid, terminate the process with a BAD ADDRESS error code
        proc->terminate(-EFAULT);
        return false;
    }
    return true;
}

uint32_t* PalmyraOS::kernel::SystemCallsManager::handleInterrupt(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // Find the appropriate system call handler based on the syscall number in regs->eax
    auto it = systemCallHandlers_.find(regs->eax);
    if (it != systemCallHandlers_.end()) {
        // Call the handler function
        it->second(regs);
    }
    else {
        // unsupported syscall!!
        LOG_WARN("Unknown SYSCALL (%d) at 0x%X", regs->eax, regs->eip);
        regs->eax = -EINVAL;
    }

    // Retrieve the current process
    auto* proc        = TaskManager::getCurrentProcess();
    bool condition_01 = proc->getState() == Process::State::Terminated;  // unexpected / exit
    bool condition_02 = proc->age_ == 0;                                 // yield

    // Return to the appropriate interrupt handler based on the process state
    if (condition_01 || condition_02) return TaskManager::interruptHandler(regs);
    else return (uint32_t*) (regs);
}

void PalmyraOS::kernel::SystemCallsManager::handleExit(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // void _exit(int)
    TaskManager::getCurrentProcess()->terminate((int) regs->ebx);
}

void PalmyraOS::kernel::SystemCallsManager::handleGetPid(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // uint32_t getpid()
    regs->eax = TaskManager::getCurrentProcess()->getPid();
}

void PalmyraOS::kernel::SystemCallsManager::handleYield(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int sched_yield();

    regs->eax                              = 0;  // Return 0 to indicate success
    TaskManager::getCurrentProcess()->age_ = 0;  // Reset the age to yield the CPU
}

void PalmyraOS::kernel::SystemCallsManager::handleMmap(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // void* mmap(void* addr, uint32_t length, int prot, int flags, int fd, uint32_t offset)

    // Extract arguments from registers
    void* addr               = (void*) regs->ebx;  // TODO: currently ignored
    uint32_t length          = regs->ecx;
    uint32_t protectionFlags = regs->edx;  // TODO, offset
    uint32_t flags           = regs->esi;
    uint32_t fd_reg          = regs->edi;

    // Check if addr is a valid pointer
    //	if (!isValidAddress(addr)) return;

    // Allocate memory pages for the current process based on the requested length
    void* allocatedAddr      = TaskManager::getCurrentProcess()->allocatePages((length >> 12) + 1);

    // Set eax to the allocated address or MAP_FAILED
    if (allocatedAddr != nullptr) { regs->eax = (uint32_t) allocatedAddr; }
    else { regs->eax = (uint32_t) MAP_FAILED; }
}

void PalmyraOS::kernel::SystemCallsManager::handleGetTime(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int clock_gettime(uint32_t clk_id, struct timespec *tp)

    // Extract arguments from registers
    uint32_t clockId = regs->edi;
    auto* timeSpec   = (timespec*) regs->esi;

    // Check if timeSpec is a valid pointer
    if (!isValidAddress(timeSpec)) return;

    // Get the current time in ticks and frequency
    uint64_t ticks     = SystemClock::getTicks();
    uint64_t frequency = SystemClockFrequency;

    // Convert ticks to seconds and nanoseconds
    timeSpec->tv_nsec  = (ticks * 1'000'000) / frequency;
    timeSpec->tv_sec   = timeSpec->tv_nsec / 1'000'000;

    // Set eax to 0 to indicate success
    regs->eax          = 0;
}

void PalmyraOS::kernel::SystemCallsManager::handleOpen(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int open(const char *pathname, int flags)

    // Extract arguments from registers
    char* pathname = (char*) regs->ebx;
    int flags      = static_cast<int>(regs->ecx);

    // Check if pathname is a valid pointer
    if (!isValidAddress(pathname)) {
        regs->eax = -EFAULT;
        return;
    }

    KString path(pathname);
    auto inode = vfs::VirtualFileSystem::getInodeByPath(path);

    // TODO: enhance code readability
    if (inode) {
        // O_CREAT | O_EXCL on existing file -> EEXIST
        if ((flags & O_CREAT) && (flags & O_EXCL)) {
            regs->eax = -EEXIST;
            return;
        }

        // O_TRUNC requires file type and write access
        if (flags & O_TRUNC) {
            if (inode->getType() != vfs::InodeBase::Type::File) {
                regs->eax = -EISDIR;
                return;
            }
            if (!((flags & O_WRONLY) || (flags & O_RDWR))) {
                regs->eax = -EINVAL;
                return;
            }
            if (inode->truncate(0) != 0) {
                regs->eax = -1;
                return;
            }
        }
    }
    else {
        // Not found: create only if O_CREAT
        if (!(flags & O_CREAT)) {
            regs->eax = -ENOENT;
            return;
        }

        // Resolve parent directory and final component
        auto components = path.split('/', true);
        if (components.empty()) {
            regs->eax = -ENOENT;
            return;
        }

        auto* parent = vfs::VirtualFileSystem::getParentDirectory(vfs::VirtualFileSystem::getRootInode(), components);
        if (!parent || parent->getType() != vfs::InodeBase::Type::Directory) {
            regs->eax = -ENOENT;
            return;
        }

        KString finalName = components.back();
        inode = parent->createFile(finalName, vfs::InodeBase::Mode::USER_READ | vfs::InodeBase::Mode::USER_WRITE, vfs::InodeBase::UserID::ROOT, vfs::InodeBase::GroupID::ROOT);
        if (!inode) {
            regs->eax = -1;
            return;
        }
    }

    // Create a new FileDescriptor and allocate a file descriptor number
    auto* fileDesc      = heapManager.createInstance<FileDescriptor>(inode, flags);
    fd_t fileDescriptor = TaskManager::getCurrentProcess()->descriptorTable_.allocate(fileDesc);
    regs->eax           = fileDescriptor;
}

void PalmyraOS::kernel::SystemCallsManager::handleMkdir(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int mkdir(const char *pathname, mode_t mode)

    // Extract arguments from registers
    char* pathname = (char*) regs->ebx;
    uint32_t mode  = regs->ecx;

    // Check if pathname is a valid pointer
    if (!isValidAddress(pathname)) {
        regs->eax = -EFAULT;
        return;
    }

    KString path(pathname);

    // Resolve parent directory and final component
    auto components = path.split('/', true);
    if (components.empty()) {
        regs->eax = -ENOENT;
        return;
    }

    // Get parent directory
    auto* parent = vfs::VirtualFileSystem::getParentDirectory(vfs::VirtualFileSystem::getRootInode(), components);
    if (!parent || parent->getType() != vfs::InodeBase::Type::Directory) {
        regs->eax = -ENOENT;
        return;
    }

    // Get the final component (directory name)
    KString dirName = components.back();

    // Create the directory
    interrupts::InterruptController::enableInterrupts();
    auto* newDir = parent->createDirectory(dirName,
                                           vfs::InodeBase::Mode::USER_READ | vfs::InodeBase::Mode::USER_WRITE | vfs::InodeBase::Mode::USER_EXECUTE,
                                           vfs::InodeBase::UserID::ROOT,
                                           vfs::InodeBase::GroupID::ROOT);

    interrupts::InterruptController::disableInterrupts();

    if (!newDir) {
        regs->eax = -EEXIST;  // Directory already exists or creation failed
        return;
    }

    regs->eax = 0;  // Success
}

void PalmyraOS::kernel::SystemCallsManager::handleRmdir(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int rmdir(const char *pathname)

    // Extract arguments from registers
    char* pathname = (char*) regs->ebx;

    // Check if pathname is a valid pointer
    if (!isValidAddress(pathname)) {
        regs->eax = -EFAULT;
        return;
    }

    KString path(pathname);

    // POSIX semantics: rmdir() must fail with ENOTDIR if target is not a directory.
    // First, resolve the full path to the inode. If not found -> ENOENT.
    {
        auto target = vfs::VirtualFileSystem::getInodeByPath(path);
        if (!target) {
            LOG_ERROR("[SYSCALL] RMDIR - Target path '%s' not found", path.c_str());
            regs->eax = -ENOENT;
            return;
        }

        if (target->getType() != vfs::InodeBase::Type::Directory) {
            regs->eax = -ENOTDIR;
            return;
        }
    }

    auto components = path.split('/', true);
    if (components.empty()) {
        regs->eax = -ENOENT;
        return;
    }

    auto* parent = vfs::VirtualFileSystem::getParentDirectory(vfs::VirtualFileSystem::getRootInode(), components);
    if (!parent || parent->getType() != vfs::InodeBase::Type::Directory) {
        regs->eax = -ENOENT;
        return;
    }

    KString dirName = components.back();
    // Enable interrupts to allow the system to handle other tasks during the operation
    interrupts::InterruptController::enableInterrupts();
    bool success = parent->deleteDirectory(dirName);
    interrupts::InterruptController::disableInterrupts();

    if (!success) {
        // deleteDirectory() returns false for several reasons:
        // - Directory not found (ENOENT) - already checked above
        // - Not empty (ENOTEMPTY)
        // - Not a directory (ENOTDIR) - already checked above
        // Assuming the most common failure is non-empty directory
        regs->eax = -ENOTEMPTY;
        return;
    }

    regs->eax = 0;  // Success
}

void PalmyraOS::kernel::SystemCallsManager::handleUnlink(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int unlink(const char *pathname)  (remove file)
    char* pathname = (char*) regs->ebx;
    if (!isValidAddress(pathname)) {
        regs->eax = -EFAULT;
        return;
    }

    KString path(pathname);

    // POSIX semantics: unlink() must fail with EISDIR if target is a directory.
    // First, resolve the full path to the inode. If not found -> ENOENT.
    {
        auto target = vfs::VirtualFileSystem::getInodeByPath(path);
        if (!target) {
            regs->eax = -ENOENT;
            return;
        }
        if (target->getType() == vfs::InodeBase::Type::Directory) {
            regs->eax = -EISDIR;
            return;
        }
    }

    auto components = path.split('/', true);
    if (components.empty()) {
        regs->eax = -ENOENT;
        return;
    }

    auto* parent = vfs::VirtualFileSystem::getParentDirectory(vfs::VirtualFileSystem::getRootInode(), components);
    if (!parent || parent->getType() != vfs::InodeBase::Type::Directory) {
        regs->eax = -ENOENT;
        return;
    }

    KString fileName = components.back();
    interrupts::InterruptController::enableInterrupts();
    bool success = parent->deleteFile(fileName);
    interrupts::InterruptController::disableInterrupts();

    if (!success) {
        regs->eax = -ENOENT;
        return;
    }
    regs->eax = 0;
}

void PalmyraOS::kernel::SystemCallsManager::handleClose(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int close(int fd)

    // Extract arguments from registers
    uint32_t fd = regs->ebx;

    // Release the descriptor (works for all descriptor types)
    TaskManager::getCurrentProcess()->descriptorTable_.release(fd);

    // Set eax to 0 to indicate success
    regs->eax = 0;
}

void PalmyraOS::kernel::SystemCallsManager::handleWrite(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int write(uint32_t fd, const void *buf, uint32_t count)

    // Extract arguments from registers
    size_t fileDescriptor = regs->ebx;
    char* bufferPointer   = (char*) regs->ecx;
    size_t size           = regs->edx;

    // Check if bufferPointer is a valid pointer
    if (!isValidAddress(bufferPointer)) return;

    // Get the current process
    auto* proc    = TaskManager::getCurrentProcess();
    bufferPointer = (char*) proc->pagingDirectory_->getPhysicalAddress(bufferPointer);

    // TODO move to actual
    // TODO 0

    // Handle writing to stdout (file descriptor 1) and stderr (file descriptor 2)
    if (fileDescriptor == 1) {
        // Initialize the number of bytes written to 0
        regs->eax = 0;
        for (size_t i = 0; i < size; ++i) {
            if (bufferPointer[i] == '\0') break;        // Stop at null terminator
            proc->stdout_.push_back(bufferPointer[i]);  // Write to stdout
            regs->eax++;                                // Increment the byte count
        }
    }
    else if (fileDescriptor == 2) {
        // Initialize the number of bytes written to 0
        regs->eax = 0;
        for (size_t i = 0; i < size; ++i) {
            if (bufferPointer[i] == '\0') break;        // Stop at null terminator
            proc->stderr_.push_back(bufferPointer[i]);  // Write to stdout
            regs->eax++;                                // Increment the byte count
        }
    }
    else {
        // Handle writing to regular files
        Descriptor* desc = proc->descriptorTable_.get(fileDescriptor);
        if (!desc) {
            // If the descriptor is not open, set the number of bytes written to 0
            regs->eax = 0;  // we wrote 0 bytes
            return;
        }

        // Check if it's a file descriptor (pipes/sockets would have different logic)
        if (desc->kind() != Descriptor::Kind::File) {
            regs->eax = -EINVAL;  // Invalid descriptor type for this operation
            return;
        }

        // Safe to cast since we checked the kind
        auto* file     = static_cast<FileDescriptor*>(desc);

        // Write data to the file and update the file offset
        auto bytesRead = file->getInode()->write(bufferPointer, size, file->getOffset());
        file->advanceOffset(bytesRead);

        // Set eax to the number of bytes written
        regs->eax = bytesRead;
    }
}

void PalmyraOS::kernel::SystemCallsManager::handleRead(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int read(uint32_t fileDescriptor, void* buffer, uint32_t count);

    // Extract arguments from registers
    size_t fileDescriptor = regs->ebx;
    char* bufferPointer   = (char*) regs->ecx;
    size_t size           = regs->edx;

    // Check if bufferPointer is a valid pointer
    if (!isValidAddress(bufferPointer)) return;

    // Get the descriptor associated with the file descriptor
    Descriptor* desc = TaskManager::getCurrentProcess()->descriptorTable_.get(fileDescriptor);
    if (!desc) {
        // If the descriptor is not open, set the number of bytes read to 0
        regs->eax = 0;  // we read 0 bytes
        return;
    }

    // Check if it's a file descriptor (pipes/sockets would have different logic)
    if (desc->kind() != Descriptor::Kind::File) {
        regs->eax = -EINVAL;  // Invalid descriptor type for this operation
        return;
    }

    // Safe to cast since we checked the kind
    auto* file = static_cast<FileDescriptor*>(desc);

    // Enable interrupts to allow the system to handle other tasks while sleeping // TODO way to block only FAT32
    interrupts::InterruptController::enableInterrupts();

    // Read data from the file and update the file offset
    auto bytesRead = file->getInode()->read(bufferPointer, size, file->getOffset());
    file->advanceOffset(bytesRead);

    interrupts::InterruptController::disableInterrupts();

    // Set eax to the number of bytes read
    regs->eax = bytesRead;
}

void PalmyraOS::kernel::SystemCallsManager::handleIoctl(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // Extract arguments from registers
    auto fileDescriptor = regs->ebx;
    auto request        = static_cast<int>(regs->ecx);
    auto argp           = (void*) regs->edx;

    // Check if argp is a valid pointer
    if (!isValidAddress(argp)) return;

    // Get the current process
    auto* proc       = TaskManager::getCurrentProcess();

    // Get the descriptor associated with the file descriptor
    Descriptor* desc = proc->descriptorTable_.get(fileDescriptor);
    if (!desc) {
        regs->eax = -EBADF;
        return;
    }

    // Check if it's a file descriptor
    if (desc->kind() != Descriptor::Kind::File) {
        regs->eax = -EINVAL;
        return;
    }

    auto* file  = static_cast<FileDescriptor*>(desc);

    // Perform the IOCTL operation
    auto status = file->getInode()->ioctl(request, argp);
    regs->eax   = status;
}

void PalmyraOS::kernel::SystemCallsManager::handleInitWindow(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // Extract the arguments from the CPU registers
    auto** userBuffer = reinterpret_cast<uint32_t**>(regs->ebx);       // Buffer pointer
    auto* windowInfo  = reinterpret_cast<palmyra_window*>(regs->ecx);  // Window information structure

    // Check if the windowInfo pointer is a valid memory address
    if (!isValidAddress(windowInfo)) {
        regs->eax = -EINVAL;  // Invalid address error
        return;
    }

    // Extract window parameters from the palmyra_window structure
    uint32_t x             = windowInfo->x;
    uint32_t y             = windowInfo->y;
    uint32_t width         = windowInfo->width;
    uint32_t height        = windowInfo->height;

    // Calculate the size required for the window buffer
    uint32_t requiredSize  = width * height * sizeof(uint32_t);  // Each pixel is 32 bits (4 bytes)
    uint32_t requiredPages = CEIL_DIV_PAGE_SIZE(requiredSize);   // Calculate required pages

    // Check if userBuffer is a valid pointer
    if (!isValidAddress(userBuffer)) {
        regs->eax = -EINVAL;  // Invalid address error
        return;
    }

    // Allocate memory pages for the window buffer
    auto* proc          = TaskManager::getCurrentProcess();
    auto* allocatedAddr = reinterpret_cast<uint32_t*>(proc->allocatePages(requiredPages));

    // Set the user buffer to the allocated address
    *userBuffer         = allocatedAddr;

    // Request a window with the extracted parameters and title
    auto* window        = WindowManager::requestWindow(allocatedAddr, x, y, width, height);
    if (!window) {
        regs->eax = -ENOMEM;  // Error: Could not create the window
        return;
    }
    window->setMovable(windowInfo->movable);

    // Add the window ID to the process's list of windows and return the window ID in eax
    proc->windows_.push_back(window->getID());
    regs->eax = window->getID();  // Return the window ID
}

void PalmyraOS::kernel::SystemCallsManager::handleCloseWindow(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // Extract arguments from registers
    uint32_t windowId = regs->ebx;

    // Get the current process and remove the window ID from its list of windows
    auto* proc        = TaskManager::getCurrentProcess();
    for (auto it = proc->windows_.begin(); it != proc->windows_.end(); ++it) {
        if (*it == windowId) {
            proc->windows_.erase(it);
            break;
        }
    }

    // Close the window with the given ID
    WindowManager::closeWindow(windowId);
}

void PalmyraOS::kernel::SystemCallsManager::handleNextKeyboardEvent(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // Extract arguments from registers
    uint32_t windowId = regs->ebx;
    auto event        = (KeyboardEvent*) regs->ecx;

    // Check if event is a valid pointer
    if (!isValidAddress(event)) return;

    *event = WindowManager::popKeyboardEvent(windowId);
}

void PalmyraOS::kernel::SystemCallsManager::handleNextMouseEvent(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // Extract arguments from registers
    uint32_t windowId = regs->ebx;
    auto event        = (MouseEvent*) regs->ecx;

    // Check if event is a valid pointer
    if (!isValidAddress(event)) return;

    *event = WindowManager::popMouseEvent(windowId);
}

void PalmyraOS::kernel::SystemCallsManager::handleGetWindowStatus(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    uint32_t windowId = regs->ebx;
    auto status       = (palmyra_window_status*) regs->ecx;

    // Check if status is a valid pointer
    if (!isValidAddress(status)) return;

    // TODO check if the window belongs actually to current process
    auto window = WindowManager::getWindowById(windowId);
    if (!window) return;

    auto [x, y]          = window->getPosition();
    auto [width, height] = window->getSize();
    bool isActive        = WindowManager::getActiveWindowId() == windowId;

    *status              = {.x = x, .y = y, .width = width, .height = height, .isActive = isActive};
}

void PalmyraOS::kernel::SystemCallsManager::handleGetdents(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int getdents(unsigned int fd, linux_dirent* dirp, unsigned int count)

    // Extract arguments from registers
    size_t fileDescriptor = regs->ebx;
    auto bufferPointer    = (linux_dirent*) regs->ecx;
    size_t count          = regs->edx;

    // Check if bufferPointer is a valid pointer
    if (!isValidAddress(bufferPointer)) return;
    if (!isValidAddress((char*) bufferPointer + count)) return;

    // TODO: uncap and allow for iterative
    count            = count > 4096 ? 4096 : count;

    // Get the descriptor associated with the file descriptor
    Descriptor* desc = TaskManager::getCurrentProcess()->descriptorTable_.get(fileDescriptor);
    if (!desc) {
        regs->eax = -EBADF;
        return;
    }

    // Check if it's a file descriptor
    if (desc->kind() != Descriptor::Kind::File) {
        regs->eax = -EINVAL;
        return;
    }

    auto* file = static_cast<FileDescriptor*>(desc);

    // Check if it's a directory
    if (file->getInode()->getType() != vfs::InodeBase::Type::Directory) {
        regs->eax = -ENOTDIR;  // Not a directory
        return;
    }

    // Enable interrupts to allow the system to handle other tasks while sleeping
    interrupts::InterruptController::enableInterrupts();

    // Read directory entries by offset
    auto dentries = file->getInode()->getDentries(file->getOffset(), count);

    interrupts::InterruptController::disableInterrupts();

    file->advanceOffset(dentries.size());

    char* buffer     = (char*) bufferPointer;
    size_t bytesRead = 0;

    for (size_t index = 0; index < dentries.size() && bytesRead < count; ++index) {
        auto [name, inode] = dentries[index];

        // TODO: Check second inode (nullptr? etc..)

        auto type          = (uint8_t) inode->getType();
        auto d_ino         = (uint32_t) inode->getInodeNumber();

        size_t nameLen     = name.size();

        // +1 for null-terminator, +1 for d_type
        size_t reclen      = sizeof(linux_dirent) + nameLen + 1 + 1;

        // Not enough space in the buffer
        if (bytesRead + reclen > count) break;

        auto* dirent     = (linux_dirent*) (buffer + bytesRead);
        dirent->d_ino    = d_ino;
        dirent->d_off    = bytesRead + reclen;
        dirent->d_reclen = reclen;

        memcpy((void*) dirent->d_name, (void*) name.c_str(), nameLen);
        dirent->d_name[nameLen]                    = '\0';  // Null-terminate the name

        *(char*) (buffer + bytesRead + reclen - 1) = (char) type;  // Append the file type

        bytesRead += reclen;
    }

    // Disable interrupts

    regs->eax = bytesRead;
}

void PalmyraOS::kernel::SystemCallsManager::handleClockNanoSleep64(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // Extract arguments from registers
    uint32_t clock_id = regs->ebx;  // TODO
    uint32_t flags    = regs->ecx;  // TODO
    const auto* req   = reinterpret_cast<const timespec*>(regs->edx);
    auto* rem         = reinterpret_cast<timespec*>(regs->esi);

    // Validate the pointers
    if (!isValidAddress(const_cast<timespec*>(req)) || (rem && !isValidAddress(rem))) {
        regs->eax = -EFAULT;
        return;
    }

    // Calculate the target time in ticks
    uint64_t targetTicks = SystemClock::getTicks() + (req->tv_sec * SystemClockFrequency) + (req->tv_nsec * SystemClockFrequency / 1'000'000'000);

    // Enable interrupts to allow the system to handle other tasks while sleeping
    interrupts::InterruptController::enableInterrupts();

    // Busy-wait loop to simulate sleep
    while (SystemClock::getTicks() < targetTicks) sched_yield();

    // Disable interrupts
    interrupts::InterruptController::disableInterrupts();

    // Set the result to 0 to indicate success
    regs->eax = 0;
}

void PalmyraOS::kernel::SystemCallsManager::handleLongSeek(PalmyraOS::kernel::interrupts::CPURegisters* regs) {

    // int32_t lseek(uint32_t fd, int32_t offset, int whence)

    // Extract arguments from registers
    uint32_t fd      = regs->ebx;  // File descriptor
    int32_t offset   = regs->ecx;  // Offset
    int whence       = regs->edx;  // Reference point (SEEK_SET, SEEK_CUR, SEEK_END)

    // Get the current process
    auto* proc       = TaskManager::getCurrentProcess();

    // Get the descriptor associated with the file descriptor
    Descriptor* desc = proc->descriptorTable_.get(fd);
    if (!desc) {
        regs->eax = -EBADF;
        return;
    }

    // Check if it's a file descriptor - pipes and sockets are not seekable
    if (desc->kind() != Descriptor::Kind::File) {
        regs->eax = -ESPIPE;  // Illegal seek (POSIX standard for non-seekable descriptors)
        return;
    }

    // Safe to cast since we checked the kind
    auto* file = static_cast<FileDescriptor*>(desc);

    // Determine the new offset based on the whence value
    int32_t newOffset;
    switch (whence) {
        case SEEK_SET: newOffset = offset; break;
        case SEEK_CUR: newOffset = file->getOffset() + offset; break;
        case SEEK_END: newOffset = file->getInode()->getSize() + offset; break;
        default:
            // Invalid whence value, return an error
            regs->eax = -EINVAL;
            return;
    }

    // Check if the new offset is within valid bounds
    // Note: POSIX allows seeking beyond EOF - writing will extend the file
    if (newOffset < 0) {
        // If the new offset is negative, return an error
        regs->eax = -EINVAL;
        return;
    }

    // Set the file's offset to the new offset
    file->setOffset(newOffset);

    // Return the new offset in eax
    regs->eax = newOffset;
}

void PalmyraOS::kernel::SystemCallsManager::handleWaitPID(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // uint32_t waitpid(uint32_t pid, int* status, int options);

    // Extract arguments from registers
    uint32_t pid = regs->ebx;         // The process ID to wait for
    int* status  = (int*) regs->ecx;  // A pointer to store the status
    int options  = regs->edx;         // Options for waitpid (not used in this implementation)

    // Validate the status pointer
    if (status && !isValidAddress(status)) {
        regs->eax = -EFAULT;  // Invalid memory address
        return;
    }

    // Get the current process TODO needed?
    // auto* currentProcess = TaskManager::getCurrentProcess();

    // Find the child process with the given PID
    auto* childProcess = TaskManager::getProcess(pid);
    if (!childProcess) {
        // If no process with the given PID exists, return an error
        regs->eax = -ECHILD;  // No such child process
        return;
    }

    // Enable interrupts to allow the system to handle other tasks while sleeping
    interrupts::InterruptController::enableInterrupts();

    // Busy-wait loop until the child process is terminated
    while (childProcess->getState() != Process::State::Killed) {
        // Yield the CPU to allow other processes to run
        sched_yield();
    }

    // Disable interrupts
    interrupts::InterruptController::disableInterrupts();

    // If a status pointer is provided, write the child's exit status to it
    if (status) *status = childProcess->getExitCode();

    // Return the PID of the terminated child process
    regs->eax = pid;
}

void PalmyraOS::kernel::SystemCallsManager::handleSpawn(PalmyraOS::kernel::interrupts::CPURegisters* regs) {

    // uint32_t posix_spawn(uint32_t* pid, const char* path, void* file_actions, void* attrp, char* const argv[], char* const envp[]);

    // Extract arguments from registers
    auto* pid         = reinterpret_cast<uint32_t*>(regs->ebx);
    const char* path  = reinterpret_cast<const char*>(regs->ecx);
    char* const* argv = reinterpret_cast<char* const*>(regs->edx);
    char* const* envp = reinterpret_cast<char* const*>(regs->esi);

    // file_actions and attrp are ignored in this implementation
    // void* file_actions = reinterpret_cast<void*>(regs->esi); // Ignored
    // void* attrp = reinterpret_cast<void*>(regs->edi);         // Ignored

    // Check if path is a valid pointer
    if (!isValidAddress(const_cast<char*>(path))) {
        regs->eax = -EFAULT;  // Bad address
        return;
    }

    // Count the number of arguments in argv
    int argc = 0;
    while (argv[argc] != nullptr) { argc++; }

    // Look up the file in VFS
    auto inode = vfs::VirtualFileSystem::getInodeByPath(KString(path));
    if (!inode) {
        regs->eax = -ENOENT;  // No such file or directory
        return;
    }

    Process* proc = nullptr;

    // Check if it's a built-in executable (virtual method, no RTTI needed!)
    if (inode->isBuiltinExecutable()) {
        // Cast is safe because isBuiltinExecutable() returned true
        auto* builtinInode = static_cast<vfs::BuiltinExecutableInode*>(inode);
        auto entryPoint    = builtinInode->getEntryPoint();

        LOG_INFO("EXEC BUILTIN: %s at entry point 0x%X", path, (uint32_t) entryPoint);

        // Create process with the built-in entry point
        proc = kernel::TaskManager::newProcess(entryPoint, kernel::Process::Mode::User, kernel::Process::Priority::Low, argc, argv, true);
    }
    else if (inode->getType() == vfs::InodeBase::Type::File) {
        // Load as ELF from disk
        auto fileSize = inode->getSize();
        KVector<uint8_t> fileContent(fileSize);

        if (inode->read((char*) fileContent.data(), fileSize, 0) != fileSize) {
            regs->eax = -EIO;  // I/O error
            return;
        }

        LOG_INFO("EXEC ELF: %s", path);

        // Execute the ELF file as a new process
        proc = kernel::TaskManager::execv_elf(fileContent, kernel::Process::Mode::User, kernel::Process::Priority::Low, argc, argv);
    }
    else {
        // Not a regular file or built-in executable
        regs->eax = -EACCES;  // Permission denied (not executable)
        return;
    }

    // Check if process creation succeeded
    if (!proc) {
        regs->eax = -ENOMEM;  // Out of memory or failed to create the process
        return;
    }

    // If pid is a valid pointer, store the process ID of the new process
    if (isValidAddress(pid)) { *pid = proc->getPid(); }

    // Return success
    regs->eax = 0;
}

void PalmyraOS::kernel::SystemCallsManager::handleArchPrctl(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int arch_prctl(int code, unsigned long addr)

    uint32_t code = regs->ebx;  // The operation code (ARCH_SET_FS or ARCH_GET_FS)
    uint32_t addr = regs->ecx;  // The address to set or the value to get

    LOG_WARN("SYSCALL archprctl (0x%X, 0x%X)", code, addr);

    // Determine what operation is being requested
    switch (code) {
        case ARCH_SET_FS:
            // Set the FS base to addr
            TaskManager::getCurrentProcess()->stack_.fs = addr;  // Store the FS base for this process
            // Update the actual FS segment register if needed
            //			set_fs_base(addr);   // TODO This would be a function that writes to the MSR (Model Specific Register) for FS
            regs->eax                                   = 0;  // Success
            break;

        case ARCH_GET_FS:
            // Get the current FS base
            regs->eax = TaskManager::getCurrentProcess()->stack_.fs;  // Retrieve the FS base for this process
            break;

        case ARCH_SET_GS:
            // Set the GS base to addr
            TaskManager::getCurrentProcess()->stack_.gs = addr;  // Store the GS base for this process
            // Update the actual GS segment register if needed
            //			set_gs_base(addr);   // TODO This would be a function that writes to the MSR for GS
            regs->eax                                   = 0;  // Success
            break;

        case ARCH_GET_GS:
            // Get the current GS base
            regs->eax = TaskManager::getCurrentProcess()->stack_.gs;  // Retrieve the GS base for this process
            break;

        default:
            // Unsupported arch_prctl code
            LOG_WARN("Unknown arch_prctl code: 0x%X", code);
            regs->eax = -EINVAL;  // Set an error code (Invalid argument)
            break;
    }
}

void PalmyraOS::kernel::SystemCallsManager::handleBrk(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int brk(void *end_data_segment);

    // Get the requested new break address from the register
    uint32_t requested_brk = regs->ebx;

    LOG_WARN("SYSCALL brk(0x%X)", requested_brk);

    // Get the current process
    Process* currentProcess = TaskManager::getCurrentProcess();

    // If the requested break is 0, return the current break
    if (requested_brk == 0) {
        regs->eax = currentProcess->current_brk;
        return;
    }

    // Ensure requested_brk does not exceed the maximum addressable virtual memory TODO
    //	if (requested_brk > MAX_VIRTUAL_ADDRESS)
    //	{
    //		LOG_ERROR("SYSCALL brk(0x%X) -> invalid request (exceeds addressable memory)", requested_brk);
    //		regs->eax = (uint32_t)-1;
    //		return;
    //	}

    // Ensure the requested break is within the allowed range (between initial_brk and max_brk)
    if (requested_brk >= currentProcess->initial_brk && requested_brk <= currentProcess->max_brk) {
        // Adjust the current break to the requested break
        currentProcess->current_brk = requested_brk;
        regs->eax                   = currentProcess->current_brk;  // Success, return the new break
    }
    else if (requested_brk > currentProcess->max_brk) {
        // Calculate the additional pages required
        size_t additional_pages = (requested_brk - currentProcess->max_brk + PAGE_SIZE - 1) / PAGE_SIZE;

        // Use allocatePagesAt to allocate memory starting at max_brk
        void* allocated_memory  = currentProcess->allocatePagesAt(reinterpret_cast<void*>(currentProcess->max_brk), additional_pages);

        if (allocated_memory != nullptr) {
            LOG_WARN("SYSCALL brk(0x%X) -> pages: %d", requested_brk, additional_pages);

            // Successfully allocated new pages
            currentProcess->max_brk += additional_pages * PAGE_SIZE;
            currentProcess->current_brk = requested_brk;
            regs->eax                   = currentProcess->current_brk;  // Return the new break
        }
        else {
            // Memory allocation failed, return failure (-1)
            LOG_ERROR("SYSCALL brk(0x%X) -> memory allocation failed", requested_brk);
            regs->eax = (uint32_t) -1;
        }
    }
    else {
        // Requested break is below the initial break or invalid, return failure (-1)
        LOG_ERROR("SYSCALL brk(0x%X) -> invalid request (below initial_brk)", requested_brk);
        regs->eax = (uint32_t) -1;
    }
}

void PalmyraOS::kernel::SystemCallsManager::handleGetUID(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    LOG_WARN("SYSCALL handleGetUID");
    regs->eax = 1000;
}

void PalmyraOS::kernel::SystemCallsManager::handleGetGID(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    LOG_WARN("SYSCALL handleGetGID");
    regs->eax = 1000;
}

void PalmyraOS::kernel::SystemCallsManager::handleGetEUID(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    LOG_WARN("SYSCALL handleGetEUID");
    regs->eax = 1000;
}

void PalmyraOS::kernel::SystemCallsManager::handleGetEGID(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    LOG_WARN("SYSCALL handleGetEGID");
    regs->eax = 1000;
}

void PalmyraOS::kernel::SystemCallsManager::handleSetThreadArea(PalmyraOS::kernel::interrupts::CPURegisters* regs) {

    // Extract the pointer to the user descriptor (TLS descriptor) from the registers
    //	auto* userDescriptor = reinterpret_cast<UserDescriptor*>(regs->ebx);

    LOG_WARN("SYSCALL set_thread_area(0x%X) ", regs->ebx);

    // Make the process think it was successful TODO far-future implement TLS and threading inside processes
    regs->eax = (uint32_t) 0;
}

void PalmyraOS::kernel::SystemCallsManager::handleReboot(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int reboot(int magic, int magic2, int cmd, void *arg)

    // Extract arguments from registers
    uint32_t magic  = regs->ebx;          // Must be LINUX_REBOOT_MAGIC1
    uint32_t magic2 = regs->ecx;          // Must be one of LINUX_REBOOT_MAGIC2*
    uint32_t cmd    = regs->edx;          // Command (RESTART, POWER_OFF, etc.)
    void* arg       = (void*) regs->esi;  // Optional argument (unused for now)

    LOG_INFO("SYSCALL reboot(magic=0x%X, magic2=0x%X, cmd=0x%X)", magic, magic2, cmd);

    // Validate magic numbers (Linux compatibility)
    if (magic != LINUX_REBOOT_MAGIC1) {
        LOG_ERROR("reboot: Invalid magic1 (0x%X != 0x%X)", magic, LINUX_REBOOT_MAGIC1);
        regs->eax = -EINVAL;
        return;
    }

    // Check if magic2 is one of the valid values
    bool validMagic2 = (magic2 == LINUX_REBOOT_MAGIC2) || (magic2 == LINUX_REBOOT_MAGIC2A) || (magic2 == LINUX_REBOOT_MAGIC2B) || (magic2 == LINUX_REBOOT_MAGIC2C);

    if (!validMagic2) {
        LOG_ERROR("reboot: Invalid magic2 (0x%X)", magic2);
        regs->eax = -EINVAL;
        return;
    }

    // Execute the appropriate command
    switch (cmd) {
        case LINUX_REBOOT_CMD_RESTART:
            LOG_INFO("Reboot: RESTART requested");
            kernel::reboot();
            // Does not return
            break;

        case LINUX_REBOOT_CMD_POWER_OFF:
            LOG_INFO("Reboot: POWER_OFF requested");
            kernel::shutdown();
            // Does not return
            break;

        case LINUX_REBOOT_CMD_HALT:
            LOG_INFO("Reboot: HALT requested");
            kernel::shutdown();  // Treat halt as shutdown
            // Does not return
            break;

        case LINUX_REBOOT_CMD_RESTART2:
            LOG_WARN("Reboot: RESTART2 with custom command not supported, using regular restart");
            kernel::reboot();
            // Does not return
            break;

        case LINUX_REBOOT_CMD_CAD_ON:
        case LINUX_REBOOT_CMD_CAD_OFF:
            // Ctrl-Alt-Del enable/disable - not applicable for now
            LOG_WARN("Reboot: CAD_ON/OFF command ignored (not supported)");
            regs->eax = 0;  // Success (no-op)
            return;

        default:
            LOG_ERROR("Reboot: Unknown command 0x%X", cmd);
            regs->eax = -EINVAL;
            return;
    }

    // Should never reach here for RESTART/POWER_OFF/HALT
    regs->eax = 0;
}

// ==================== Socket Syscalls ====================

void PalmyraOS::kernel::SystemCallsManager::handleSocket(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int socket(int domain, int type, int protocol);
    int domain   = (int) regs->ebx;
    int type     = (int) regs->ecx;
    int protocol = (int) regs->edx;

    LOG_INFO("SYSCALL socket(domain=%d, type=%d, protocol=%d)", domain, type, protocol);

    // Get current process
    auto* proc = TaskManager::getCurrentProcess();
    if (!proc) {
        regs->eax = -ESRCH;
        return;
    }

    // Create SocketDescriptor
    auto* socketDesc = heapManager.createInstance<SocketDescriptor>(domain, type, protocol);
    if (!socketDesc) {
        LOG_ERROR("SYSCALL socket() -> failed to create socket descriptor");
        regs->eax = -ENOMEM;
        return;
    }

    // Allocate file descriptor
    fd_t sockfd = proc->descriptorTable_.allocate(socketDesc);
    if (sockfd < 0) {
        LOG_ERROR("SYSCALL socket() -> failed to allocate descriptor");
        delete socketDesc;
        regs->eax = -EMFILE;
        return;
    }

    LOG_INFO("SYSCALL socket() -> fd=%d", sockfd);
    regs->eax = sockfd;
}

void PalmyraOS::kernel::SystemCallsManager::handleBind(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
    fd_t sockfd      = (fd_t) regs->ebx;
    const void* addr = (const void*) regs->ecx;
    uint32_t addrlen = regs->edx;

    LOG_INFO("SYSCALL bind(sockfd=%d, addr=%p, addrlen=%u)", sockfd, addr, addrlen);

    // Validate address
    if (!isValidAddress((void*) addr)) {
        regs->eax = -EFAULT;
        return;
    }

    // Get current process
    auto* proc = TaskManager::getCurrentProcess();
    if (!proc) {
        regs->eax = -ESRCH;
        return;
    }

    // Get descriptor
    Descriptor* desc = proc->descriptorTable_.get(sockfd);
    if (!desc || desc->kind() != Descriptor::Kind::Socket) {
        LOG_ERROR("SYSCALL bind() -> invalid socket descriptor %d", sockfd);
        regs->eax = -EBADF;
        return;
    }

    // Cast to SocketDescriptor
    auto* socketDesc = static_cast<SocketDescriptor*>(desc);

    // Call bind
    int result       = socketDesc->bind(addr, addrlen);
    regs->eax        = result;

    LOG_INFO("SYSCALL bind() -> %d", result);
}

void PalmyraOS::kernel::SystemCallsManager::handleConnect(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
    fd_t sockfd      = (fd_t) regs->ebx;
    const void* addr = (const void*) regs->ecx;
    uint32_t addrlen = regs->edx;

    LOG_INFO("SYSCALL connect(sockfd=%d, addr=%p, addrlen=%u)", sockfd, addr, addrlen);

    // Validate address
    if (!isValidAddress((void*) addr)) {
        regs->eax = -EFAULT;
        return;
    }

    // Get current process
    auto* proc = TaskManager::getCurrentProcess();
    if (!proc) {
        regs->eax = -ESRCH;
        return;
    }

    // Get descriptor
    Descriptor* desc = proc->descriptorTable_.get(sockfd);
    if (!desc || desc->kind() != Descriptor::Kind::Socket) {
        LOG_ERROR("SYSCALL connect() -> invalid socket descriptor %d", sockfd);
        regs->eax = -EBADF;
        return;
    }

    // Cast to SocketDescriptor
    auto* socketDesc = static_cast<SocketDescriptor*>(desc);

    // Call connect
    int result       = socketDesc->connect(addr, addrlen);
    regs->eax        = result;

    LOG_INFO("SYSCALL connect() -> %d", result);
}

void PalmyraOS::kernel::SystemCallsManager::handleSendto(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
    //                const struct sockaddr *dest_addr, socklen_t addrlen);
    fd_t sockfd          = (fd_t) regs->ebx;
    const char* buffer   = (const char*) regs->ecx;
    size_t length        = regs->edx;
    int flags            = (int) regs->esi;
    const void* destAddr = (const void*) regs->edi;
    uint32_t addrlen     = regs->ebp;

    LOG_INFO("SYSCALL sendto(sockfd=%d, len=%u, flags=%d)", sockfd, length, flags);

    // Validate buffer
    if (!isValidAddress((void*) buffer)) {
        regs->eax = -EFAULT;
        return;
    }

    // Validate dest address if provided
    if (destAddr && !isValidAddress((void*) destAddr)) {
        regs->eax = -EFAULT;
        return;
    }

    // Get current process
    auto* proc = TaskManager::getCurrentProcess();
    if (!proc) {
        regs->eax = -ESRCH;
        return;
    }

    // Get descriptor
    Descriptor* desc = proc->descriptorTable_.get(sockfd);
    if (!desc || desc->kind() != Descriptor::Kind::Socket) {
        LOG_ERROR("SYSCALL sendto() -> invalid socket descriptor %d", sockfd);
        regs->eax = -EBADF;
        return;
    }

    // Cast to SocketDescriptor
    auto* socketDesc = static_cast<SocketDescriptor*>(desc);

    // Call sendto
    size_t result    = socketDesc->sendto(buffer, length, flags, destAddr, addrlen);
    regs->eax        = result;

    LOG_INFO("SYSCALL sendto() -> %d bytes", result);
}

void PalmyraOS::kernel::SystemCallsManager::handleRecvfrom(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
    //                  struct sockaddr *src_addr, socklen_t *addrlen);
    fd_t sockfd       = (fd_t) regs->ebx;
    char* buffer      = (char*) regs->ecx;
    size_t length     = regs->edx;
    int flags         = (int) regs->esi;
    void* srcAddr     = (void*) regs->edi;
    uint32_t* addrlen = (uint32_t*) regs->ebp;

    LOG_INFO("SYSCALL recvfrom(sockfd=%d, len=%u, flags=%d)", sockfd, length, flags);

    // Validate buffer
    if (!isValidAddress(buffer)) {
        regs->eax = -EFAULT;
        return;
    }

    // Validate src address if provided
    if (srcAddr && !isValidAddress(srcAddr)) {
        regs->eax = -EFAULT;
        return;
    }

    // Validate addrlen if provided
    if (addrlen && !isValidAddress(addrlen)) {
        regs->eax = -EFAULT;
        return;
    }

    // Get current process
    auto* proc = TaskManager::getCurrentProcess();
    if (!proc) {
        regs->eax = -ESRCH;
        return;
    }

    // Get descriptor
    Descriptor* desc = proc->descriptorTable_.get(sockfd);
    if (!desc || desc->kind() != Descriptor::Kind::Socket) {
        LOG_ERROR("SYSCALL recvfrom() -> invalid socket descriptor %d", sockfd);
        regs->eax = -EBADF;
        return;
    }

    // Cast to SocketDescriptor
    auto* socketDesc = static_cast<SocketDescriptor*>(desc);

    // Call recvfrom
    size_t result    = socketDesc->recvfrom(buffer, length, flags, srcAddr, addrlen);
    regs->eax        = result;

    LOG_INFO("SYSCALL recvfrom() -> %d bytes", result);
}

void PalmyraOS::kernel::SystemCallsManager::handleSetsockopt(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int setsockopt(int sockfd, int level, int optname,
    //                const void *optval, socklen_t optlen);
    fd_t sockfd        = (fd_t) regs->ebx;
    int level          = (int) regs->ecx;
    int optname        = (int) regs->edx;
    const void* optval = (const void*) regs->esi;
    uint32_t optlen    = regs->edi;

    LOG_INFO("SYSCALL setsockopt(sockfd=%d, level=%d, optname=%d, optlen=%u)", sockfd, level, optname, optlen);

    // Validate optval
    if (optval && !isValidAddress((void*) optval)) {
        regs->eax = -EFAULT;
        return;
    }

    // Get current process
    auto* proc = TaskManager::getCurrentProcess();
    if (!proc) {
        regs->eax = -ESRCH;
        return;
    }

    // Get descriptor
    Descriptor* desc = proc->descriptorTable_.get(sockfd);
    if (!desc || desc->kind() != Descriptor::Kind::Socket) {
        LOG_ERROR("SYSCALL setsockopt() -> invalid socket descriptor %d", sockfd);
        regs->eax = -EBADF;
        return;
    }

    // Cast to SocketDescriptor
    auto* socketDesc = static_cast<SocketDescriptor*>(desc);

    // Call setsockopt
    int result       = socketDesc->setsockopt(level, optname, optval, optlen);
    regs->eax        = result;

    LOG_INFO("SYSCALL setsockopt() -> %d", result);
}

void PalmyraOS::kernel::SystemCallsManager::handleGetsockopt(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int getsockopt(int sockfd, int level, int optname,
    //                void *optval, socklen_t *optlen);
    fd_t sockfd      = (fd_t) regs->ebx;
    int level        = (int) regs->ecx;
    int optname      = (int) regs->edx;
    void* optval     = (void*) regs->esi;
    uint32_t* optlen = (uint32_t*) regs->edi;

    LOG_INFO("SYSCALL getsockopt(sockfd=%d, level=%d, optname=%d)", sockfd, level, optname);

    // Validate optval
    if (optval && !isValidAddress(optval)) {
        regs->eax = -EFAULT;
        return;
    }

    // Validate optlen
    if (optlen && !isValidAddress(optlen)) {
        regs->eax = -EFAULT;
        return;
    }

    // Get current process
    auto* proc = TaskManager::getCurrentProcess();
    if (!proc) {
        regs->eax = -ESRCH;
        return;
    }

    // Get descriptor
    Descriptor* desc = proc->descriptorTable_.get(sockfd);
    if (!desc || desc->kind() != Descriptor::Kind::Socket) {
        LOG_ERROR("SYSCALL getsockopt() -> invalid socket descriptor %d", sockfd);
        regs->eax = -EBADF;
        return;
    }

    // Cast to SocketDescriptor
    auto* socketDesc = static_cast<SocketDescriptor*>(desc);

    // Call getsockopt
    int result       = socketDesc->getsockopt(level, optname, optval, optlen);
    regs->eax        = result;

    LOG_INFO("SYSCALL getsockopt() -> %d", result);
}

void PalmyraOS::kernel::SystemCallsManager::handleGetsockname(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
    fd_t sockfd       = (fd_t) regs->ebx;
    void* addr        = (void*) regs->ecx;
    uint32_t* addrlen = (uint32_t*) regs->edx;

    LOG_INFO("SYSCALL getsockname(sockfd=%d)", sockfd);

    // Validate address
    if (!isValidAddress(addr)) {
        regs->eax = -EFAULT;
        return;
    }

    // Validate addrlen
    if (!isValidAddress(addrlen)) {
        regs->eax = -EFAULT;
        return;
    }

    // Get current process
    auto* proc = TaskManager::getCurrentProcess();
    if (!proc) {
        regs->eax = -ESRCH;
        return;
    }

    // Get descriptor
    Descriptor* desc = proc->descriptorTable_.get(sockfd);
    if (!desc || desc->kind() != Descriptor::Kind::Socket) {
        LOG_ERROR("SYSCALL getsockname() -> invalid socket descriptor %d", sockfd);
        regs->eax = -EBADF;
        return;
    }

    // Cast to SocketDescriptor
    auto* socketDesc = static_cast<SocketDescriptor*>(desc);

    // Call getsockname
    int result       = socketDesc->getsockname(addr, addrlen);
    regs->eax        = result;

    LOG_INFO("SYSCALL getsockname() -> %d", result);
}

void PalmyraOS::kernel::SystemCallsManager::handleGetpeername(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
    fd_t sockfd       = (fd_t) regs->ebx;
    void* addr        = (void*) regs->ecx;
    uint32_t* addrlen = (uint32_t*) regs->edx;

    LOG_INFO("SYSCALL getpeername(sockfd=%d)", sockfd);

    // Validate address
    if (!isValidAddress(addr)) {
        regs->eax = -EFAULT;
        return;
    }

    // Validate addrlen
    if (!isValidAddress(addrlen)) {
        regs->eax = -EFAULT;
        return;
    }

    // Get current process
    auto* proc = TaskManager::getCurrentProcess();
    if (!proc) {
        regs->eax = -ESRCH;
        return;
    }

    // Get descriptor
    Descriptor* desc = proc->descriptorTable_.get(sockfd);
    if (!desc || desc->kind() != Descriptor::Kind::Socket) {
        LOG_ERROR("SYSCALL getpeername() -> invalid socket descriptor %d", sockfd);
        regs->eax = -EBADF;
        return;
    }

    // Cast to SocketDescriptor
    auto* socketDesc = static_cast<SocketDescriptor*>(desc);

    // Call getpeername
    int result       = socketDesc->getpeername(addr, addrlen);
    regs->eax        = result;

    LOG_INFO("SYSCALL getpeername() -> %d", result);
}

void PalmyraOS::kernel::SystemCallsManager::handleListen(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int listen(int sockfd, int backlog);
    fd_t sockfd = (fd_t) regs->ebx;
    int backlog = (int) regs->ecx;

    LOG_INFO("SYSCALL listen(sockfd=%d, backlog=%d)", sockfd, backlog);

    // Get current process
    auto* proc = TaskManager::getCurrentProcess();
    if (!proc) {
        regs->eax = -ESRCH;
        return;
    }

    // Get descriptor
    Descriptor* desc = proc->descriptorTable_.get(sockfd);
    if (!desc || desc->kind() != Descriptor::Kind::Socket) {
        LOG_ERROR("SYSCALL listen() -> invalid socket descriptor %d", sockfd);
        regs->eax = -EBADF;
        return;
    }

    // Cast to SocketDescriptor
    auto* socketDesc = static_cast<SocketDescriptor*>(desc);

    // Call listen
    int result       = socketDesc->listen(backlog);
    regs->eax        = result;

    LOG_INFO("SYSCALL listen() -> %d", result);
}

void PalmyraOS::kernel::SystemCallsManager::handleAccept(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
    fd_t sockfd       = (fd_t) regs->ebx;
    void* addr        = (void*) regs->ecx;
    uint32_t* addrlen = (uint32_t*) regs->edx;

    LOG_INFO("SYSCALL accept(sockfd=%d)", sockfd);

    // Validate address if provided
    if (addr && !isValidAddress(addr)) {
        regs->eax = -EFAULT;
        return;
    }

    // Validate addrlen if provided
    if (addrlen && !isValidAddress(addrlen)) {
        regs->eax = -EFAULT;
        return;
    }

    // Get current process
    auto* proc = TaskManager::getCurrentProcess();
    if (!proc) {
        regs->eax = -ESRCH;
        return;
    }

    // Get descriptor
    Descriptor* desc = proc->descriptorTable_.get(sockfd);
    if (!desc || desc->kind() != Descriptor::Kind::Socket) {
        LOG_ERROR("SYSCALL accept() -> invalid socket descriptor %d", sockfd);
        regs->eax = -EBADF;
        return;
    }

    // Cast to SocketDescriptor
    auto* socketDesc            = static_cast<SocketDescriptor*>(desc);

    // Call accept
    SocketDescriptor* newSocket = socketDesc->accept(addr, addrlen);
    if (!newSocket) {
        LOG_ERROR("SYSCALL accept() -> failed");
        regs->eax = -EOPNOTSUPP;  // Not implemented yet
        return;
    }

    // Allocate file descriptor for new socket
    fd_t newSockfd = proc->descriptorTable_.allocate(newSocket);
    if (newSockfd < 0) {
        LOG_ERROR("SYSCALL accept() -> failed to allocate descriptor");
        delete newSocket;
        regs->eax = -EMFILE;
        return;
    }

    LOG_INFO("SYSCALL accept() -> new fd=%d", newSockfd);
    regs->eax = newSockfd;
}

void PalmyraOS::kernel::SystemCallsManager::handleShutdown(PalmyraOS::kernel::interrupts::CPURegisters* regs) {
    // int shutdown(int sockfd, int how);
    fd_t sockfd = (fd_t) regs->ebx;
    int how     = (int) regs->ecx;

    LOG_INFO("SYSCALL shutdown(sockfd=%d, how=%d)", sockfd, how);

    // Get current process
    auto* proc = TaskManager::getCurrentProcess();
    if (!proc) {
        regs->eax = -ESRCH;
        return;
    }

    // Get descriptor
    Descriptor* desc = proc->descriptorTable_.get(sockfd);
    if (!desc || desc->kind() != Descriptor::Kind::Socket) {
        LOG_ERROR("SYSCALL shutdown() -> invalid socket descriptor %d", sockfd);
        regs->eax = -EBADF;
        return;
    }

    // Cast to SocketDescriptor
    auto* socketDesc = static_cast<SocketDescriptor*>(desc);

    // Call shutdown
    int result       = socketDesc->shutdown(how);
    regs->eax        = result;

    LOG_INFO("SYSCALL shutdown() -> %d", result);
}
