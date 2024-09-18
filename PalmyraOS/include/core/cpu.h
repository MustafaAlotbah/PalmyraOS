
#pragma    once

#include "core/definitions.h"


namespace PalmyraOS::kernel
{
  class CPU
  {
   public:
	  struct CPUIDOutput
	  {
		  uint32_t eax;
		  uint32_t ebx;
		  uint32_t ecx;
		  uint32_t edx;
	  };

	  /**
	   * @brief Get the current value of the Time Stamp Counter.
	   * @return The current value of the TSC.
	   */
	  static uint64_t getTSC();

	  /**
	   * @brief Delays execution by the specified number of CPU ticks.
	   * @param cpu_ticks The number of CPU ticks to delay.
	   */
	  static void delay(uint64_t cpu_ticks);

	  /**
	   * @brief Executes the CPUID instruction with the given leaf and subleaf.
	   * @param leaf The main CPUID function code.
	   * @param subleaf The subfunction code.
	   * @return The output of the CPUID instruction.
	   */
	  static CPUIDOutput cpuid(uint32_t leaf, uint32_t subleaf);

	  static void initialize();

	  static uint32_t detectCpuFrequency();

	  /**
	   * @brief Get the number of logical CPU cores.
	   * @return The number of logical CPU cores.
	   */
	  static uint32_t getNumLogicalCores();

	  /**
	   * @brief Get the number of physical CPU cores.
	   * @return The number of physical CPU cores.
	   */
	  static uint32_t getNumPhysicalCores();

	  /**
	   * @brief Get the CPU vendor ID string.
	   * @param vendorID Pointer to a buffer to store the vendor ID string.
	   */
	  static void getVendorID(char* vendorID);

	  /**
	   * @brief Get the CPU processor brand string.
	   * @param brand Pointer to a buffer to store the processor brand string.
	   */
	  static void getProcessorBrand(char* brand);

	  /**
	   * @brief Check if SSE instruction set is available.
	   * @return True if SSE is available, false otherwise.
	   */
	  static bool isSSEAvailable();

	  /**
	   * @brief Check if SSE2 instruction set is available.
	   * @return True if SSE2 is available, false otherwise.
	   */
	  static bool isSSE2Available();

	  /**
	   * @brief Check if SSE3 instruction set is available.
	   * @return True if SSE3 is available, false otherwise.
	   */
	  static bool isSSE3Available();

	  /**
	   * @brief Check if SSSE3 instruction set is available.
	   * @return True if SSSE3 is available, false otherwise.
	   */
	  static bool isSSSE3Available();

	  /**
	   * @brief Check if SSE4.1 instruction set is available.
	   * @return True if SSE4.1 is available, false otherwise.
	   */
	  static bool isSSE41Available();

	  /**
	   * @brief Check if SSE4.2 instruction set is available.
	   * @return True if SSE4.2 is available, false otherwise.
	   */
	  static bool isSSE42Available();

	  /**
	   * @brief Check if AVX instruction set is available.
	   * @return True if AVX is available, false otherwise.
	   */
	  static bool isAVXAvailable();

	  /**
	   * @brief Check if AVX2 instruction set is available.
	   * @return True if AVX2 is available, false otherwise.
	   */
	  static bool isAVX2Available();

	  static uint32_t getCacheLineSize();

	  /**
	   * @brief Get the L1 cache size.
	   * @return The L1 cache size in KB.
	   */
	  static uint32_t getL1CacheSize();

	  /**
	   * @brief Get the L2 cache size.
	   * @return The L2 cache size in KB.
	   */
	  static uint32_t getL2CacheSize();

	  /**
	   * @brief Get the L3 cache size.
	   * @return The L3 cache size in KB.
	   */
	  static uint32_t getL3CacheSize();

	  /**
	   * @brief Check if Hyper-Threading is available.
	   * @return True if Hyper-Threading is available, false otherwise.
	   */
	  static bool isHyperThreadingAvailable();

	  /**
	   * @brief Check if the CPU supports 64-bit mode.
	   * @return True if 64-bit mode is supported, false otherwise.
	   */
	  static bool is64BitSupported();

	  /**
	   * @brief Check if BMI1 (Bit Manipulation Instruction) instruction set is available.
	   * @return True if BMI1 is available, false otherwise.
	   */
	  static bool isBMI1Available();

	  /**
	   * @brief Check if BMI2 (Bit Manipulation Instruction) instruction set is available.
	   * @return True if BMI2 is available, false otherwise.
	   */
	  static bool isBMI2Available();

	  /**
	   * @brief Check if FMA (Fused Multiply-Add) instruction set is available.
	   * @return True if FMA is available, false otherwise.
	   */
	  static bool isFMAAvailable();

	  /**
	   * @brief Check if AES (Advanced Encryption Standard) instruction set is available.
	   * @return True if AES is available, false otherwise.
	   */
	  static bool isAESAvailable();

	  /**
	   * @brief Check if SHA (Secure Hash Algorithm) instruction set is available.
	   * @return True if SHA is available, false otherwise.
	   */
	  static bool isSHAAvailable();

	  static uint32_t getCPUFrequency()
	  { return CPU_frequency_; }
	  static uint32_t getHSCFrequency()
	  { return HSC_frequency_; }

   private:
	  static uint32_t  CPU_frequency_;
	  static uint32_t  HSC_frequency_;
  };
}