
#include "core/cpu.h"
#include "libs/memory.h"


extern "C" uint64_t read_tsc_low();
extern "C" uint64_t read_tsc_high();


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
	*((uint32_t*)vendorID)       = result.ebx;
	*((uint32_t*)(vendorID + 4)) = result.edx;
	*((uint32_t*)(vendorID + 8)) = result.ecx;
	vendorID[12] = '\0';
}

void PalmyraOS::kernel::CPU::getProcessorBrand(char* brand)
{

	for (int i = 0; i < 3; ++i)
	{
		auto result = cpuid(0x80000002 + i, 0);
		*((uint32_t*)(brand + i * 16))      = result.eax;
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
	auto result = cpuid(1, 0);
	return (result.ebx & 0xFF00) >> 5;
}

uint32_t PalmyraOS::kernel::CPU::getL1CacheSize()
{
	auto result = cpuid(4, 0);
	return ((result.ebx >> 22) + 1) * ((result.ebx >> 12 & 0x3FF) + 1) * ((result.ebx & 0xFFF) + 1) * (result.ecx + 1)
		/ 1024;
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