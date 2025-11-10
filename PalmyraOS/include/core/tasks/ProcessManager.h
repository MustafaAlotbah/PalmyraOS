
#pragma once

#include "core/definitions.h"
#include "core/files/VirtualFileSystem.h"
#include "core/memory/KernelHeapAllocator.h"
#include "libs/MutexTracker.h"


namespace PalmyraOS::kernel {

    // Forward declaration
    class Mutex;

    // Maximum number of processes supported
    constexpr uint32_t MAX_PROCESSES             = 512;
    constexpr uint32_t PROCESS_KERNEL_STACK_SIZE = 10;
    constexpr uint32_t PROCESS_USER_STACK_SIZE   = 128;

    struct ProcessDebug {
        uint32_t entryEip       = 0;
        uint32_t lastWorkingEip = 0;
        uint32_t argvBlock      = 0;
    };

    /**
     * @enum EFlags
     * @brief Enum class representing the CPU EFlags register bits.
     */
    enum class EFlags : uint8_t {
        CF_Carry              = 0,
        PF_Parity             = 2,
        AF_Adjust             = 4,
        ZF_Zero               = 6,
        SF_Sign               = 7,
        TF_Trap               = 8,
        IF_Interrupt          = 9,
        DF_Direction          = 10,
        OF_Overflow           = 11,
        NT_NestedTask         = 14,
        RF_Resume             = 16,
        VM_Virtualization8086 = 17
    };

    /**
     * @class Process
     * @brief Class representing a process in the operating system.
     */
    class Process {
    public:
        using ProcessEntry = int (*)(uint32_t, char**);

        /**
         * @enum Mode (Privilege Level / Ring)
         * @brief Enum class representing the execution mode of a process.
         */
        enum class Mode : uint32_t { Kernel = 0, User = 3 };

        /**
         * @enum State
         * @brief Enum class representing the state of a process.
         */
        enum class State : uint32_t {
            New,         // Unused
            Ready,       // Ready to run
            Running,     // Currently running
            Terminated,  // Awaiting to be killed
            Waiting,     // IO Resource Operations
            Killed       // Killed, memory freed
        };

        /**
         * @brief Returns a C-string representation of the process state.
         * @return C-string corresponding to the process state.
         */
        [[nodiscard]] const char* stateToString() const {
            switch (state_) {
                case State::New: return "New";
                case State::Ready: return "Ready";
                case State::Running: return "Running";
                case State::Terminated: return "Terminated";
                case State::Waiting: return "Waiting";
                case State::Killed: return "Killed";
                default: return "Unknown";
            }
        }

        /**
         * @enum Priority
         * @brief Enum class representing the execution priority of a process.
         */
        enum class Priority : uint32_t { VeryLow = 1, Low = 2, Medium = 5, High = 7, VeryHigh = 10 };

        /**
         * @struct Arguments
         * @brief Struct representing the arguments to be passed to a process.
         */
        struct Arguments {
            ProcessEntry entryPoint;  ///< Entry point function for the process
            uint32_t argc;            ///< Argument count
            char** argv;              ///< Argument values
        };

    public:
        /**
         * @brief Constructs a Process object.
         * @param entryPoint Entry point function for the process
         * @param pid Process ID
         * @param mode Execution mode of the process
         * @param priority Priority of the process
         * @param argc Argument count
         * @param argv Argument values
         */
        Process(ProcessEntry entryPoint, uint32_t pid, Mode mode, Priority priority, uint32_t argc, char* const* argv, bool isInternal);

        /**
         * @brief Destructor for Process.
         */
        ~Process() = default;

        /**
         * @brief Terminates the process with the given exit code.
         * @param exitCode Exit code for the process
         */
        void terminate(int exitCode);

        /**
         * @brief Kills the process.
         * Note: This cannot be called within the process stack, as memory will be freed!
         */
        void kill();

        // ==================== Mutex Management ====================

        /**
         * @brief Acquire a mutex with automatic tracking
         *
         * Blocks until the mutex is acquired. Automatically adds mutex to
         * tracking list for cleanup on process death.
         *
         * @param mutex The mutex to acquire
         */
        void acquireMutex(Mutex& mutex);

        /**
         * @brief Release a mutex and remove from tracking
         *
         * @param mutex The mutex to release
         */
        void releaseMutex(Mutex& mutex);

        /**
         * @brief Try to acquire mutex without blocking
         *
         * @param mutex The mutex to try acquiring
         * @return true if acquired and tracked, false if already locked
         */
        bool tryAcquireMutex(Mutex& mutex);

        /**
         * @brief Get mutex tracker for introspection/debugging
         * @return Reference to the mutex tracker
         */
        MutexTracker& getMutexTracker() { return mutexTracker_; }

        /**
         * @brief Registers pages for the process to keep track of them.
         * Note this does not allocate a new page or maps a page.
         * @param physicalAddress Starting physical address of the pages
         * @param count Number of pages to register
         */
        void registerPages(void* physicalAddress, size_t count);

        /**
         * @brief De-registers pages for the process.
         * @param physicalAddress Starting physical address of the pages
         * @param count Number of pages to deregister
         */
        void deregisterPages(void* physicalAddress, size_t count);

        /**
         * @brief Allocates pages for the process.
         * @param count Number of pages to allocate.
         * @return Pointer to the allocated pages.
         */
        void* allocatePages(size_t count);

        /**
         * @brief Allocates pages for the process.
         * @param count Number of pages to allocate.
         * @return Pointer to the allocated pages.
         */
        void* allocatePagesAt(void* virtual_address, size_t count);

        /**
         * @brief Gets the execution mode of the process.
         * @return Execution mode
         */
        [[nodiscard]] Mode getMode() const { return mode_; }

        /**
         * @brief Gets the state of the process.
         * @return Process state
         */
        [[nodiscard]] State getState() const { return state_; }

        /**
         * @brief Sets the state of the process.
         * @param state New state of the process
         */
        void setState(State state) { state_ = state; }

        /**
         * @brief Gets the Process ID.
         * @return Process ID
         */
        [[nodiscard]] uint32_t getPid() const { return pid_; }

        /**
         * @brief Gets the Process ID.
         * @return Process ID
         */
        [[nodiscard]] uint32_t getUserStack() const { return (uint32_t) userStack_; }

        /**
         * @brief Gets the Process Exit Code.
         * @return Process Exit Code
         */
        [[nodiscard]] int getExitCode() const { return exitCode_; }

        /**
         * @brief Gets the CPU context of the process.
         * @return CPU context
         */
        [[nodiscard]] const interrupts::CPURegisters& getContext() const { return stack_; }

        /**
         * @brief Checks for stack overflow.
         * @return true if stack is intact
         */
        [[nodiscard]] bool checkStackOverflow() const;

        [[nodiscard]] PagingDirectory* getPagingDirectory() { return pagingDirectory_; };

        // A process can only be moved
        DEFINE_DEFAULT_MOVE(Process);
        REMOVE_COPY(Process);

        /**
         * @brief Returns the command name (program name, e.g., "terminal.elf")
         * @return Reference to the process command name
         */
        [[nodiscard]] const KString& getCommandName() const { return commandName_; }

        /**
         * @brief Converts process state to Linux-compatible single character
         * @return Character: R(running), S(sleeping), D(disk I/O), T(stopped), Z(zombie)
         */
        [[nodiscard]] char stateToChar() const;

        /**
         * @brief Serializes command-line arguments in null-terminated format (Linux /proc/pid/cmdline style)
         * @param buffer Output buffer to write the serialized cmdline
         * @param bufferSize Maximum size of the output buffer
         * @return Number of bytes written
         */
        [[nodiscard]] size_t serializeCmdline(char* buffer, size_t bufferSize) const;

        /**
         * @brief Serializes process stats in Linux /proc/pid/stat format
         * @param buffer Output buffer to write the serialized stat
         * @param bufferSize Maximum size of the output buffer
         * @param totalSystemTicks Total system CPU ticks (for reference)
         * @return Number of bytes written
         */
        [[nodiscard]] size_t serializeStat(char* buffer, size_t bufferSize, uint64_t totalSystemTicks = 0) const;

    private:
        /**
         * @brief Wrapper for the process.
         * @param args Pointer to the process arguments.
         */
        static void dispatcher(Arguments* args);

        /**
         * @brief Initializes the paging directory for the process.
         * @param mode Execution mode of the process
         */
        void initializePagingDirectory(Process::Mode mode, bool isInternal);

        /**
         * @brief Initializes the CPU state for the process.
         */
        void initializeCPUState();

        /**
         * @brief Initializes the arguments for the process.
         * @param entry Entry point function for the process.
         * @param argc Argument count.
         * @param argv Argument values.
         */
        void initializeArguments(ProcessEntry entry, uint32_t argc, char* const* argv);

        void initializeArgumentsForELF(uint32_t argc, char* const* argv);

        void captureCommandlineArguments(uint32_t argc, char* const* argv);

        void initializeProcessInVFS();


    public:
        friend class TaskManager;

        uint32_t pid_;                      ///< Process ID
        uint32_t age_;                      ///< Age of the process
        State state_;                       ///< State of the process
        Mode mode_;                         ///< Execution mode of the process
        Priority priority_;                 ///< Priority of the process
        interrupts::CPURegisters stack_{};  ///< CPU context stack
        int exitCode_{-1};                  ///< Return value of the process
        KVector<void*> physicalPages_;      ///< Holds physical pages to used by the process
        KVector<char> stdin_;               ///< proc/self/fd/0
        KVector<char> stdout_;              ///< proc/self/fd/1
        KVector<char> stderr_;              ///< proc/self/fd/2

        /// Command-line metadata (captured at process creation)
        KString commandName_;               ///< Program name (argv[0]), e.g., "terminal.elf"
        KVector<KString> commandlineArgs_;  ///< All command-line arguments (argv), stored safely

        PagingDirectory* pagingDirectory_{};  ///< Pointer to the paging directory
        void* userStack_{};                   ///< Pointer to the user stack
        void* kernelStack_{};                 ///< Pointer to the kernel stack

        KVector<uint32_t> windows_;                     ///< List of windows allocated
        vfs::FileDescriptorTable fileTableDescriptor_;  ///< File descriptor table to do VFS operations
        ProcessDebug debug_;

        uint64_t upTime_{0};
        uint64_t cpuTimeTicks_{0};
        uint64_t startTime_{0};

        uint32_t initial_brk = 0;
        uint32_t current_brk = 0;
        uint32_t max_brk     = 0;

        // ==================== Synchronization ====================

        /// @brief Tracks mutexes held by this process (for automatic cleanup on death)
        MutexTracker mutexTracker_;
    };

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
         * @brief Creates a new process.
         * @param entryPoint Entry point function for the new process
         * @param mode Execution mode of the new process
         * @return Pointer to the created process
         */
        static Process* newProcess(Process::ProcessEntry entryPoint, Process::Mode mode, Process::Priority priority, uint32_t argc, char* const* argv, bool isInternal);

        static Process* execv_elf(KVector<uint8_t>& elfFileContent, Process::Mode mode, Process::Priority priority, uint32_t argc, char* const* argv);

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

        /**
         * @brief Voluntarily yield CPU to other processes
         *
         * Causes an immediate context switch to the next ready process.
         * Used by mutexes when waiting for locks.
         */
        static void yield();

    public:
        /**
         * @brief Interrupt handler for process switching.
         * @param regs Pointer to CPU registers at the time of the interrupt
         * @return Pointer to the updated CPU registers
         */
        static uint32_t* interruptHandler(interrupts::CPURegisters*);

    private:
        static KVector<Process> processes_;    ///< Vector of processes
        static uint32_t currentProcessIndex_;  ///< Index of the current process
        static uint32_t atomicSectionLevel_;   ///< Level of atomic section nesting
        static uint32_t pid_count;             ///< Counter for assigning PIDs
    };


}  // namespace PalmyraOS::kernel