

#include "core/tasks/SystemCalls.h"
#include "core/tasks/ProcessManager.h"


void PalmyraOS::kernel::SystemCallsManager::initialize()
{
	interrupts::InterruptController::setInterruptHandler(0x80, &handleInterrupt);
}

uint32_t* PalmyraOS::kernel::SystemCallsManager::handleInterrupt(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	// Arguments: eax, ebx, ecx, edx, esi, edi

	// uint32_t getpid()
	if (regs->eax == 20)
	{
		auto* proc = TaskManager::getCurrentProcess();
		regs->eax = proc->getPid();
	}


	return (uint32_t*)(regs);
}
