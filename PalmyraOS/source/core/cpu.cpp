
#include "core/cpu.h"
#include "libs/memory.h"
#include "core/SystemClock.h"
#include "core/peripherals/RTC.h"
#include "core/peripherals/Logger.h"

extern "C" uint64_t read_tsc_low();
extern "C" uint64_t read_tsc_high();

uint32_t PalmyraOS::kernel::CPU::CPU_frequency_ = 0;
uint32_t PalmyraOS::kernel::CPU::HSC_frequency_ = 0;


uint64_t PalmyraOS::kernel::CPU::getTSC()
{
	uint32_t low  = read_tsc_low();
	uint32_t high = read_tsc_high();
	return (static_cast<uint64_t>(high) << 32) | low;
}

void PalmyraOS::kernel::CPU::delay(uint64_t cpu_ticks)
{
	uint64_t end = getTSC() + cpu_ticks;
	while (getTSC() < end);
}

PalmyraOS::kernel::CPU::CPUIDOutput PalmyraOS::kernel::CPU::cpuid(uint32_t leaf, uint32_t subleaf)
{
	CPUIDOutput result{};
	__asm__ volatile("cpuid"
		: "=a"(result.eax), "=b"(result.ebx), "=c"(result.ecx), "=d"(result.edx)
		: "a"(leaf), "c"(subleaf));
	return result;
}

void PalmyraOS::kernel::CPU::initialize()
{
	detectCpuFrequency();
}



uint32_t PalmyraOS::kernel::CPU::getNumLogicalCores()
{
	auto result = cpuid(1, 0);
	return (result.ebx >> 16) & 0xFF;
}

uint32_t PalmyraOS::kernel::CPU::getNumPhysicalCores()
{
	auto result = cpuid(4, 0);
	return ((result.eax >> 26) & 0x3F) + 1;
}

void PalmyraOS::kernel::CPU::getVendorID(char* vendorID)
{
	auto result = cpuid(0, 0);
	*((uint32_t*)(vendorID + 0)) = result.ebx;
	*((uint32_t*)(vendorID + 4)) = result.edx;
	*((uint32_t*)(vendorID + 8)) = result.ecx;
	vendorID[12] = '\0';
}

void PalmyraOS::kernel::CPU::getProcessorBrand(char* brand)
{

	for (int i = 0; i < 3; ++i)
	{
		auto result = cpuid(0x80000002 + i, 0);
		*((uint32_t*)(brand + i * 16 + 0)) = result.eax;
		*((uint32_t*)(brand + i * 16 + 4))  = result.ebx;
		*((uint32_t*)(brand + i * 16 + 8))  = result.ecx;
		*((uint32_t*)(brand + i * 16 + 12)) = result.edx;
	}
	brand[48] = '\0';
}

bool PalmyraOS::kernel::CPU::isSSEAvailable()
{
	auto result = cpuid(1, 0);
	return result.edx & (1 << 25);
}

bool PalmyraOS::kernel::CPU::isSSE2Available()
{
	auto result = cpuid(1, 0);
	return result.edx & (1 << 26);
}

bool PalmyraOS::kernel::CPU::isSSE3Available()
{
	auto result = cpuid(1, 0);
	return result.ecx & 1;
}

bool PalmyraOS::kernel::CPU::isSSSE3Available()
{
	auto result = cpuid(1, 0);
	return result.ecx & (1 << 9);
}

bool PalmyraOS::kernel::CPU::isSSE41Available()
{
	auto result = cpuid(1, 0);
	return result.ecx & (1 << 19);
}

bool PalmyraOS::kernel::CPU::isSSE42Available()
{
	auto result = cpuid(1, 0);
	return result.ecx & (1 << 20);
}

bool PalmyraOS::kernel::CPU::isAVXAvailable()
{
	auto result = cpuid(1, 0);
	return result.ecx & (1 << 28);
}

bool PalmyraOS::kernel::CPU::isAVX2Available()
{
	auto result = cpuid(7, 0);
	return result.ebx & (1 << 5);
}

uint32_t PalmyraOS::kernel::CPU::getCacheLineSize()
{
	CPUIDOutput output = cpuid(4, 0);
	return (output.ebx & 0xFFF) + 1;
}

uint32_t PalmyraOS::kernel::CPU::getL1CacheSize()
{
	// Call cpuid with leaf 0x04 and subleaf 1 to get information about the L1 cache.
	CPUIDOutput output = cpuid(0x04, 1);

	// Extract the number of ways of associativity.
	// This indicates the number of cache lines in a set (how many ways the cache is associative).
	// This is found in bits 22-31 of EBX and is represented as (ways - 1).
	uint32_t ways = ((output.ebx >> 22) & 0x3FF) + 1;

	// Extract the number of partitions.
	// This indicates how the cache is partitioned.
	// This is found in bits 12-21 of EBX and is represented as (partitions - 1).
	uint32_t partitions = ((output.ebx >> 12) & 0x3FF) + 1;

	// Extract the system coherency line size.
	// This indicates the size of a cache line in bytes.
	// This is found in bits 0-11 of EBX and is represented as (line size - 1).
	uint32_t line_size = (output.ebx & 0xFFF) + 1;

	// Extract the number of sets.
	// This indicates the number of unique sets in the cache.
	// This is found in ECX and is represented as (sets - 1).
	uint32_t sets = output.ecx + 1;

	// Calculate the L1 cache size in KB.
	// The formula is: cache size = (ways * partitions * line_size * sets) / 1024
	uint32_t l1_cache_size = (ways * partitions * line_size * sets) / 1024;
	return l1_cache_size;
}

uint32_t PalmyraOS::kernel::CPU::getL2CacheSize()
{
	auto result = cpuid(4, 2);
	return ((result.ebx >> 22) + 1) * ((result.ebx >> 12 & 0x3FF) + 1) * ((result.ebx & 0xFFF) + 1) * (result.ecx + 1)
		/ 1024;
}

uint32_t PalmyraOS::kernel::CPU::getL3CacheSize()
{
	auto result = cpuid(4, 3);
	return ((result.ebx >> 22) + 1) * ((result.ebx >> 12 & 0x3FF) + 1) * ((result.ebx & 0xFFF) + 1) * (result.ecx + 1)
		/ 1024;
}

bool PalmyraOS::kernel::CPU::isHyperThreadingAvailable()
{
	auto result = cpuid(1, 0);
	return result.edx & (1 << 28);
}

bool PalmyraOS::kernel::CPU::is64BitSupported()
{
	auto result = cpuid(0x80000001, 0);
	return result.edx & (1 << 29);
}

bool PalmyraOS::kernel::CPU::isBMI1Available()
{
	auto result = cpuid(7, 0);
	return result.ebx & (1 << 3);
}

bool PalmyraOS::kernel::CPU::isBMI2Available()
{
	auto result = cpuid(7, 0);
	return result.ebx & (1 << 8);
}

bool PalmyraOS::kernel::CPU::isFMAAvailable()
{
	auto result = cpuid(1, 0);
	return result.ecx & (1 << 12);
}

bool PalmyraOS::kernel::CPU::isAESAvailable()
{
	auto result = cpuid(1, 0);
	return result.ecx & (1 << 25);
}

bool PalmyraOS::kernel::CPU::isSHAAvailable()
{
	auto result = cpuid(7, 0);
	return result.ebx & (1 << 29);
}

uint32_t PalmyraOS::kernel::CPU::detectCpuFrequency()
{
	/*
	 * Here we measure the difference between the System Clock (ticks)
	 * and the CPU ticks register TSC
	 * Elapsed Seconds = Elapsed Ticks / Clock Frequency
	 * CPU Frequency = Elapsed TSC / (Elapsed Seconds * 10^6)
	 */

	//if (CPU_frequency_ > 500) return CPU_frequency_;

	// Ensure the CPUID serializing instruction
	cpuid(0, 0);


	uint64_t secondsOfDayStart = RTC::getSecondsOfDay();
	uint64_t secondsOfDayEnd   = RTC::getSecondsOfDay();

	// wait for a change
	while (secondsOfDayEnd == secondsOfDayStart) secondsOfDayEnd = RTC::getSecondsOfDay();

	// Capture the initial timestamp counter and clock tick
	uint64_t start_tsc  = getTSC();
	uint64_t start_tick = SystemClock::getTicks();
	secondsOfDayStart = secondsOfDayEnd;

	// wait for 2 seconds
	constexpr uint64_t secondsMeasurement = 2; // Measure over 25 ticks
	while (secondsOfDayEnd < secondsOfDayStart + secondsMeasurement)
	{
		secondsOfDayEnd = RTC::getSecondsOfDay();
	}

	// Capture the timestamps at the end of the measurement interval
	uint64_t end_ticks = SystemClock::getTicks();
	uint64_t end_tsc = getTSC();

	// Calculate the number of ticks in the measurement interval
	uint64_t elapsed_ticks = end_ticks - start_tick;

	// Calculate the number of TSC ticks in the measurement interval
	uint64_t elapsed_tsc = end_tsc - start_tsc;

	CPU_frequency_ = elapsed_tsc / secondsMeasurement / 1e6;
	HSC_frequency_ = elapsed_ticks / secondsMeasurement;

	LOG_WARN("CPU (TSC)   frequency %d MHz", CPU_frequency_);
	LOG_WARN("HSC (Ticks) frequency %d Hz", HSC_frequency_);

	return CPU_frequency_;
}

