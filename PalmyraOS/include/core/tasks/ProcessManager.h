
#pragma once

#include "core/definitions.h"
#include "core/memory/HeapAllocator.h"
#include <vector>


namespace PalmyraOS::kernel
{

  // Maximum number of processes supported
  constexpr int MAX_PROCESSES = 512;

  /**
   * @enum EFlags
   * @brief Enum class representing the CPU EFlags register bits.
   */
  enum class EFlags : uint8_t
  {
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
  class Process
  {
   public:

	  /**
	   * @enum Mode (Privilege Level / Ring)
	   * @brief Enum class representing the execution mode of a process.
	   */
	  enum class Mode : uint32_t
	  {
		  Kernel = 0,
		  User   = 3
	  };

	  /**
	   * @enum State
	   * @brief Enum class representing the state of a process.
	   */
	  enum class State : uint32_t
	  {
		  New,
		  Ready,
		  Running,
		  Terminated,
		  Killed
	  };


   public:
	  /**
	   * @brief Constructs a Process object.
	   * @param entryPoint Entry point function for the process
	   * @param pid Process ID
	   * @param mode Execution mode of the process
	   */
	  Process(int (* entryPoint)(), uint32_t pid, Mode mode);

	  /**
	   * @brief Destructor for Process.
	   */
	  ~Process() = default;

	  /**
	   * @brief Initializes the paging directory for the process.
	   * @param mode Execution mode of the process
	   */
	  void initializePagingDirectory(Process::Mode mode);
//	  void loadContext(interrupts::CPURegisters& context) const;
//	  void saveContext(const interrupts::CPURegisters& context);

	  /**
	   * @brief Gets the execution mode of the process.
	   * @return Execution mode
	   */
	  [[nodiscard]] Mode getMode() const
	  { return mode_; }

	  /**
	   * @brief Gets the state of the process.
	   * @return Process state
	   */
	  [[nodiscard]] State getState() const
	  { return state_; }

	  /**
	   * @brief Sets the state of the process.
	   * @param state New state of the process
	   */
	  void setState(State state)
	  { state_ = state; }

	  /**
	   * @brief Gets the Process ID.
	   * @return Process ID
	   */
	  [[nodiscard]] uint32_t getPid() const
	  { return pid_; }

	  /**
	   * @brief Gets the CPU context of the process.
	   * @return CPU context
	   */
	  [[nodiscard]] const interrupts::CPURegisters& getContext() const
	  { return stack_; }


	  DEFINE_DEFAULT_MOVE(Process);

   public:
	  // Delete copy constructor and assignment operator to prevent copying
	  REMOVE_COPY(Process);
   public:
	  friend class TaskManager;

	  uint32_t                 pid_;                   ///< Process ID
	  uint32_t                 age_;                   ///< Age of the process
	  State                    state_;                 ///< State of the process
	  Mode                     mode_;                  ///< Execution mode of the process
	  interrupts::CPURegisters stack_{};               ///< CPU context stack
	  PagingDirectory* pagingDirectory_;               ///< Pointer to the paging directory
	  void           * userStack_{ nullptr };          ///< Pointer to the user stack
	  void           * kernelStack_{ nullptr };        ///< Pointer to the kernel stack
  };

  // Type alias for a vector of processes
  typedef std::vector<Process, KernelHeapAllocator<Process> > ProcessVector;

  /**
   * @class TaskManager
   * @brief Class for managing tasks (processes) in the operating system.
   */
  class TaskManager
  {
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
	  static Process* newProcess(int (* entryPoint)(), Process::Mode mode);

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

   private:
	  /**
	   * @brief Interrupt handler for process switching.
	   * @param regs Pointer to CPU registers at the time of the interrupt
	   * @return Pointer to the updated CPU registers
	   */
	  static uint32_t* interruptHandler(interrupts::CPURegisters*);

   private:
	  static ProcessVector processes_;           ///< Vector of processes
	  static uint32_t      currentProcessIndex_; ///< Index of the current process
	  static uint32_t      atomicSectionLevel_;  ///< Level of atomic section nesting
	  static uint32_t      pid_count;            ///< Counter for assigning PIDs
  };


}