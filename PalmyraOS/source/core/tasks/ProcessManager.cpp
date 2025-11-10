

#include <algorithm>
#include <elf.h>
#include <new>

#include "core/SystemClock.h"
#include "core/tasks/ProcessManager.h"

#include "libs/memory.h"
#include "libs/stdio.h"
#include "libs/stdlib.h"  // uitoa64
#include "libs/string.h"

#include "palmyraOS/unistd.h"  // _exit()

#include "core/tasks/WindowManager.h"  // for cleaning up windows upon terminating

#include "core/files/VirtualFileSystem.h"
#include "core/peripherals/Logger.h"
/// region Task Manager

// Globals
PalmyraOS::kernel::KVector<PalmyraOS::kernel::Process> PalmyraOS::kernel::TaskManager::processes_;
uint32_t PalmyraOS::kernel::TaskManager::currentProcessIndex_ = MAX_PROCESSES;
uint32_t PalmyraOS::kernel::TaskManager::atomicSectionLevel_  = 0;
uint32_t PalmyraOS::kernel::TaskManager::pid_count            = 0;

void PalmyraOS::kernel::TaskManager::initialize() {
    // Attach the task switching interrupt handler to the system clock.
    SystemClock::attachHandler(interruptHandler);

    // Clear and reserve space in the processes vector.
    processes_.clear();
    processes_.reserve(MAX_PROCESSES);
}

PalmyraOS::kernel::Process*
PalmyraOS::kernel::TaskManager::newProcess(Process::ProcessEntry entryPoint, Process::Mode mode, Process::Priority priority, uint32_t argc, char* const* argv, bool isInternal) {
    // Check if the maximum number of processes has been reached.
    if (processes_.size() == MAX_PROCESSES - 1) return nullptr;

    // Create a new process and add it to the processes vector.
    processes_.emplace_back(entryPoint, pid_count++, mode, priority, argc, argv, isInternal);

    // Return a pointer to the newly created process.
    return &processes_.back();
}

uint32_t* PalmyraOS::kernel::TaskManager::interruptHandler(PalmyraOS::kernel::interrupts::CPURegisters* regs) {

    // If there are no processes, or we are in an atomic section, return the current registers.
    if (processes_.empty()) return reinterpret_cast<uint32_t*>(regs);
    if (atomicSectionLevel_ > 0) return reinterpret_cast<uint32_t*>(regs);
    /**
     * @Note TaskScheduler can be called in an atomicSection
     * If the WindowsManager is composing windows, and here we kill the process -> close the window
     * -> erase the window, memcpy would be writing to an illegal address.
     * -> Hence Atomic Section guarantees that it is fully atomic
     * @short Once should also implement deferred erasing.
     */

    size_t nextProcessIndex;
    uint32_t* result;

    // Debug Information
    processes_[currentProcessIndex_].debug_.lastWorkingEip = regs->eip;

    // kill terminated processes
    for (int i = 0; i < processes_.size(); ++i) {
        if (i == currentProcessIndex_) continue;  // we cannot kill the process in its own stack (-> Page Fault)
        if (processes_[i].state_ == Process::State::Terminated) { processes_[i].kill(); }
    }

    // Save the current process state if a process is running.
    if (currentProcessIndex_ < MAX_PROCESSES) {
        // Increment CPU time for the process that is about to yield
        processes_[currentProcessIndex_].cpuTimeTicks_++;

        // save current process state
        processes_[currentProcessIndex_].stack_ = *regs;

        // check stackOverflow
        if (!processes_[currentProcessIndex_].checkStackOverflow()) {
            // TODO handle here e.g. .terminate(-3)
        }

        // if the process is not terminated or killed
        if (processes_[currentProcessIndex_].state_ != Process::State::Terminated && processes_[currentProcessIndex_].state_ != Process::State::Killed) {
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

        for (size_t i = 0; i < processes_.size(); ++i) {
            nextProcessIndex = (currentProcessIndex_ + 1 + i) % processes_.size();
            if (processes_[nextProcessIndex].getState() == Process::State::Ready) break;
        }
        currentProcessIndex_ = nextProcessIndex;
    }

    // Set the new process state to running.
    processes_[currentProcessIndex_].state_ = Process::State::Running;
    processes_[currentProcessIndex_].upTime_++;

    // If the new process is in user mode, set the kernel stack.
    if (processes_[currentProcessIndex_].mode_ == Process::Mode::User) {
        // set the kernel stack at the top of the kernel stack
        kernel::gdt_ptr->setKernelStack(reinterpret_cast<uint32_t>(processes_[currentProcessIndex_].kernelStack_) + PAGE_SIZE * PROCESS_KERNEL_STACK_SIZE - 1);
    }

    // Return the new process's stack pointer.
    result = reinterpret_cast<uint32_t*>(processes_[currentProcessIndex_].stack_.esp - offsetof(interrupts::CPURegisters, intNo));


    return result;
}

PalmyraOS::kernel::Process* PalmyraOS::kernel::TaskManager::getCurrentProcess() { return &processes_[currentProcessIndex_]; }

PalmyraOS::kernel::Process* PalmyraOS::kernel::TaskManager::getProcess(uint32_t pid) {
    if (pid >= processes_.size()) return nullptr;
    return &processes_[pid];
}

void PalmyraOS::kernel::TaskManager::startAtomicOperation() {
    if (processes_[currentProcessIndex_].mode_ != Process::Mode::Kernel) return;
    atomicSectionLevel_++;
}

void PalmyraOS::kernel::TaskManager::endAtomicOperation() {
    if (atomicSectionLevel_ > 0) atomicSectionLevel_--;
}

PalmyraOS::kernel::Process* PalmyraOS::kernel::TaskManager::execv_elf(KVector<uint8_t>& elfFileContent,
                                                                      PalmyraOS::kernel::Process::Mode mode,
                                                                      PalmyraOS::kernel::Process::Priority priority,
                                                                      uint32_t argc,
                                                                      char* const* argv) {
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

    uint32_t highest_vaddr     = 0;  // To track the highest loaded segment's address for initializing current_brk

    // Load program headers
    const auto* programHeaders = reinterpret_cast<const Elf32_Phdr*>(elfFileContent.data() + elfHeader->e_phoff);
    for (int i = 0; i < elfHeader->e_phnum; ++i) {
        const Elf32_Phdr& ph = programHeaders[i];

        // Only load PT_LOAD segments
        if (ph.p_type != PT_LOAD) continue;

        // Align the virtual address down to the nearest page boundary
        uint32_t aligned_vaddr = ph.p_vaddr & ~(PAGE_SIZE - 1);

        // Calculate the page offset
        uint32_t page_offset   = ph.p_vaddr - aligned_vaddr;

        // Adjust the size to include the page offset
        uint32_t segment_size  = ph.p_memsz + page_offset;

        // Calculate the number of pages needed
        size_t num_pages       = (segment_size + PAGE_SIZE - 1) >> PAGE_BITS;

        LOG_DEBUG("Loading Section %d: at 0x%X for %d pages", i, aligned_vaddr, num_pages);
        void* segmentAddress = process->allocatePagesAt(reinterpret_cast<void*>(aligned_vaddr), num_pages);
        if (!segmentAddress) return nullptr;

        // Copy the segment into memory
        memcpy(reinterpret_cast<uint8_t*>(segmentAddress) + page_offset, elfFileContent.data() + ph.p_offset, ph.p_filesz);

        // Zero the remaining memory if p_memsz > p_filesz
        if (ph.p_memsz > ph.p_filesz) { memset(reinterpret_cast<uint8_t*>(segmentAddress) + page_offset + ph.p_filesz, 0, ph.p_memsz - ph.p_filesz); }


        // Update the highest virtual address to track the end of the loaded segments
        uint32_t segment_end = ph.p_vaddr + ph.p_memsz;
        if (segment_end > highest_vaddr) { highest_vaddr = segment_end; }
    }
    LOG_DEBUG("Loading headers completed.");

    // Initialize the program break to the end of the last loaded segment
    // Set initial_brk and current_brk to just after the highest loaded segment
    process->initial_brk = (highest_vaddr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);  // Align to next page boundary

    // Initial and current brk set to the same value
    process->current_brk = process->initial_brk;
    process->max_brk     = process->initial_brk;

    LOG_DEBUG("Program break (brk) initialized at: 0x%X", process->initial_brk);

    // Set up the initial CPU state (already initialized in newProcess)
    // TODO make stuff here more logical PLEASE (intNo + 2 * sizeof(uint32_t) for eip)
    *(uint32_t*) (process->stack_.esp + 8) = elfHeader->e_entry;
    process->stack_.eip                    = elfHeader->e_entry;
    process->debug_.entryEip               = reinterpret_cast<uint32_t>(elfHeader->e_entry);

    // Set process state to Ready
    process->setState(Process::State::Ready);

    LOG_DEBUG("userEsp finally at 0x%X.", process->stack_.userEsp);

    return process;
}

uint32_t PalmyraOS::kernel::TaskManager::getAtomicLevel() { return atomicSectionLevel_; }


/// endregion
