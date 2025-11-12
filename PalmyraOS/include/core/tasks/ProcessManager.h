
#pragma once

#include "core/definitions.h"
#include "core/files/VirtualFileSystem.h"
#include "core/memory/KernelHeapAllocator.h"
#include "core/tasks/Process.h"


namespace PalmyraOS::kernel {

    /**
     * @class TaskManager
     * @brief Class for managing tasks (processes) in the operating system.
     */
    class TaskManager {
    public:
        /**
         * @brief Initializes the TaskManager.
         */
        static void initialize();

        /**
         * @brief Executes a builtin (internal) executable as a new process
         * @param entryPoint Entry point function for the builtin
         * @param mode Execution mode
         * @param priority Process priority
         * @param argc Argument count
         * @param argv Argument values
         * @param envp Environment variables
         * @return Pointer to the created process
         */
        static Process* execv_builtin(Process::ProcessEntry entryPoint, Process::Mode mode, Process::Priority priority, uint32_t argc, char* const* argv, char* const* envp);

        /**
         * @brief Loads and executes an ELF binary as a new process
         * @param elfFileContent ELF file data
         * @param mode Execution mode
         * @param priority Process priority
         * @param argc Argument count
         * @param argv Argument values
         * @param envp Environment variables
         * @return Pointer to the created process
         */
        static Process* execv_elf(KVector<uint8_t>& elfFileContent, Process::Mode mode, Process::Priority priority, uint32_t argc, char* const* argv, char* const* envp);

        /**
         * @brief Gets the current running process.
         * @return Pointer to the current process
         */
        static Process* getCurrentProcess();

        /**
         * @brief Gets a process by its PID.
         * @param pid Process ID
         * @return Pointer to the process
         */
        static Process* getProcess(uint32_t pid);

        // atomic TODO: other solution?
        static void startAtomicOperation();
        static void endAtomicOperation();
        static uint32_t getAtomicLevel();

    public:
        /**
         * @brief Interrupt handler for process switching.
         * @param regs Pointer to CPU registers at the time of the interrupt
         * @return Pointer to the updated CPU registers
         */
        static uint32_t* interruptHandler(interrupts::CPURegisters*);

    private:
        /**
         * @brief Internal process factory (used by execv_builtin and execv_elf)
         * @param entryPoint Entry point function (nullptr for ELF processes)
         * @param mode Execution mode
         * @param priority Process priority
         * @param argc Argument count
         * @param argv Argument values
         * @param envp Environment variables
         * @param isInternal True for builtin executables, false for external ELF binaries
         * @return Pointer to the created process
         */
        static Process*
        newProcess(Process::ProcessEntry entryPoint, Process::Mode mode, Process::Priority priority, uint32_t argc, char* const* argv, char* const* envp, bool isInternal);

        static KVector<Process> processes_;    ///< Vector of processes
        static uint32_t currentProcessIndex_;  ///< Index of the current process
        static uint32_t atomicSectionLevel_;   ///< Level of atomic section nesting
        static uint32_t pid_count;             ///< Counter for assigning PIDs
    };


}  // namespace PalmyraOS::kernel
