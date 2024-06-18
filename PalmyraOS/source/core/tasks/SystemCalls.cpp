

#include "core/tasks/SystemCalls.h"
#include "core/tasks/ProcessManager.h"


void PalmyraOS::kernel::SystemCallsManager::initialize()
{
	interrupts::InterruptController::setInterruptHandler(0x80, &handleInterrupt);
}

uint32_t* PalmyraOS::kernel::SystemCallsManager::handleInterrupt(PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	bool handled = false;

	// Arguments: eax, ebx, ecx, edx, esi, edi

	// uint32_t getpid()
	if (!handled && regs->eax == 20)
	{
		auto* proc = TaskManager::getCurrentProcess();
		regs->eax = proc->getPid();
		handled = true;
	}

	// void exit(int)
	if (!handled && regs->eax == 1)
	{
		auto* proc = TaskManager::getCurrentProcess();
		proc->terminate((int)regs->ebx);
		return TaskManager::interruptHandler(regs);
	}

	// ssize_t write(uint32_t fd, const void *buf, uint32_t count)
	if (!handled && regs->eax == 4)
	{
		size_t fileDescriptor = regs->ebx;
		const char* bufferPointer = (const char*)regs->ecx;
		size_t size = regs->edx;
		auto* proc = TaskManager::getCurrentProcess();

		if (fileDescriptor == 1)
		{
			for (int i = 0; i < size; ++i)
			{
				if (bufferPointer[i] == '\0') break;
				proc->stdout_.push_back(bufferPointer[i]);
			}
		}


	}

	return (uint32_t*)(regs);
}
