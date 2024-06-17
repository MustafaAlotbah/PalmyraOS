

#include "core/Interrupts.h"
#include "core/panic.h"
#include "core/memory/paging.h"
#include "core/kernel.h"


// External functions for the IDT. (Check interrupt_asm.asm)
extern "C" void flush_idt_table(uint32_t idt_pointer);
extern "C" void enable_interrupts();
extern "C" void disable_interrupts();
extern "C" void _default_isr_handler();
extern "C" uint32_t get_esp();
extern "C" uint32_t get_ss();

///region ASM ISRs
extern "C" void InterruptServiceRoutine_0x00(); // Division Error (Fault)
extern "C" void InterruptServiceRoutine_0x01(); // Debug (Fault/Trap)
extern "C" void InterruptServiceRoutine_0x02(); // Non-maskable Interrupt (Interrupt)
extern "C" void InterruptServiceRoutine_0x03(); // Breakpoint (Trap)
extern "C" void InterruptServiceRoutine_0x04(); // Overflow (Trap)
extern "C" void InterruptServiceRoutine_0x05(); // Bound Range Exceeded (Fault)
extern "C" void InterruptServiceRoutine_0x06(); // Invalid Opcode (Fault)
extern "C" void InterruptServiceRoutine_0x07(); // Device Not Available	(Fault)
extern "C" void InterruptServiceRoutine_0x08(); // Double Fault (Abort)
extern "C" void InterruptServiceRoutine_0x0A(); // Invalid TSS (Fault)
extern "C" void InterruptServiceRoutine_0x0B(); // Segment Not Present (Fault)
extern "C" void InterruptServiceRoutine_0x0C(); // Stack-Segment Fault (Fault)
extern "C" void InterruptServiceRoutine_0x0D(); // General Protection Fault (Fault)
extern "C" void InterruptServiceRoutine_0x0E(); // Page Fault (Fault)
// ...
extern "C" void InterruptServiceRoutine_0x20(); // timer
extern "C" void InterruptServiceRoutine_0x21(); // keyboard
extern "C" void InterruptServiceRoutine_0x22();
extern "C" void InterruptServiceRoutine_0x23();
extern "C" void InterruptServiceRoutine_0x24();
extern "C" void InterruptServiceRoutine_0x25();
extern "C" void InterruptServiceRoutine_0x26();
extern "C" void InterruptServiceRoutine_0x27();
extern "C" void InterruptServiceRoutine_0x28();
extern "C" void InterruptServiceRoutine_0x29();
extern "C" void InterruptServiceRoutine_0x2A();
extern "C" void InterruptServiceRoutine_0x2B();
extern "C" void InterruptServiceRoutine_0x2C(); // Mouse
extern "C" void InterruptServiceRoutine_0x2D();
extern "C" void InterruptServiceRoutine_0x2E();
extern "C" void InterruptServiceRoutine_0x2F();
extern "C" void InterruptServiceRoutine_0x80();
///endregion


// Globals
PalmyraOS::kernel::interrupts::InterruptHandler secondary_interrupt_handlers[256] = { nullptr };
PalmyraOS::kernel::interrupts::PICManager
	* PalmyraOS::kernel::interrupts::InterruptController::activePicManager = nullptr;

uint64_t sysClock = 0;

// Primary ISR handler, all activated handlers point here first.
extern "C" uint32_t* primary_isr_handler(PalmyraOS::kernel::interrupts::CPURegisters* registers)
{
	using namespace PalmyraOS::kernel;
	using namespace PalmyraOS::kernel::interrupts;

	// if there is a paging directory, switch to kernel directory
	if (PagingManager::isEnabled())
		PagingManager::switchPageDirectory(PalmyraOS::kernel::kernelPagingDirectory_ptr);


	// flag to panic or not
	bool handled = false;

	// send EOI: End of Interrupt (to get more interrupts) if IRQ
	bool isPICAvailable = InterruptController::activePicManager != nullptr;
	if (!isPICAvailable) PalmyraOS::kernel::kernelPanic("PIC Manager is not activated.");

	// Check if there is an active PIC manager and the interrupt is from an IRQ (0x20 to 0x2F)
	if (isPICAvailable && registers->intNo >= 0x20 && registers->intNo < 0x30)
	{
		// Check if the interrupt is from the slave PIC (IRQs 8-15, corresponding to vectors 0x28 to 0x2F)
		if (registers->intNo >= 0x28)
		{
			// Send EOI to the slave PIC
			InterruptController::activePicManager->getSlavePicCommand().write(PICManager::PIC_EOI);
		}
		// Always send EOI to the master PIC
		InterruptController::activePicManager->getMasterPicCommand().write(PICManager::PIC_EOI);
		handled = true;
	}

	// Check secondary handlers array if a handler exists for this particular interrupt number
	if (secondary_interrupt_handlers[registers->intNo] != nullptr)
	{
		auto newStackPointer = secondary_interrupt_handlers[registers->intNo](registers);
		return newStackPointer - 1;
	}

	if (!handled)
	{
		panicRegisters("Unhandled Interrupt!", registers);
	}

	return (uint32_t*)(registers) - 1;
}


///region InterruptDescriptorTable
PalmyraOS::kernel::interrupts::InterruptDescriptorTable::InterruptDescriptorTable(PalmyraOS::kernel::GDT::GlobalDescriptorTable* gdt)
{
	uint16_t codeSegment = gdt->getKernelCodeSegmentSelector();
	for (int i           = 0; i < 256; ++i)
	{
		setDescriptor(i, codeSegment, &_default_isr_handler, 0, GateType::InterruptGate);
	}
}

void PalmyraOS::kernel::interrupts::InterruptDescriptorTable::setDescriptor(
	uint8_t interruptVector,
	uint16_t codeSegmentSelector,
	void (* handlerFunc)(),
	uint8_t privilegeRing,
	PalmyraOS::kernel::interrupts::GateType gateType
)
{
	// Obtain a reference to the specific InterruptGateSegment to simplify further modifications
	InterruptEntry& entry = descriptors[interruptVector];

	// Set handler address parts
	entry.handlerAddressLow  = static_cast<uint16_t>(reinterpret_cast<uint32_t>(handlerFunc) & 0xFFFF);
	entry.handlerAddressHigh = static_cast<uint16_t>(reinterpret_cast<uint32_t>(handlerFunc) >> 16);

	// This selector points to the segment in which the interrupt handler code resides.
	entry.selector = codeSegmentSelector;
	entry.reserved = 0;

	// define constants for bitwise operations

	// Indicates if the gate is a storage segment (it's not for interrupts)
	// const uint8_t STORAGE_SEGMENT_BIT = 0b0100;

	// Bit index for the start of the descriptor privilege level
	const uint8_t DESCRIPTOR_PRIVILEGE_BIT = 0b0101;

	// Indicates if the segment is present in memory
	const uint8_t PRESENT_BIT = 0b0111;

	// Compute the combined GT, SS, DPL, and P value
	entry.attributes = static_cast<uint8_t>(gateType)
		// | (1 << STORAGE_SEGMENT_BIT)    // TODO in the future, disabled now
		| (privilegeRing << DESCRIPTOR_PRIVILEGE_BIT)
		| (1 << PRESENT_BIT);
}

void PalmyraOS::kernel::interrupts::InterruptDescriptorTable::flush()
{
	InterruptPointer idt_ptr_{};
	idt_ptr_.size    = 256 * sizeof(InterruptEntry) - 1;
	idt_ptr_.address = (uint32_t)(descriptors);
	flush_idt_table((uint32_t)&idt_ptr_);
}
///endregion


///region PICManager
PalmyraOS::kernel::interrupts::PICManager::PICManager()
	:
	masterPicCommand{ PORT_PIC_MASTER_CMD },
	masterPicData{ PORT_PIC_MASTER_DATA },
	slavePicCommand{ PORT_PIC_SLAVE_CMD },
	slavePicData{ PORT_PIC_SLAVE_DATA }
{
	// ICW1 - Initialization command
	masterPicCommand.write(ICW1_INIT);
	slavePicCommand.write(ICW1_INIT);

	// ICW2 - Interrupt vector offsets
	masterPicData.write(ICW2_MASTER_OFFSET); // Master starts at 0x20
	slavePicData.write(ICW2_SLAVE_OFFSET);  // Slave starts at 0x28

	// ICW3 - Master/slave relationship
	masterPicData.write(ICW3_MASTER_SLAVE); // Slave PIC is at IRQ2
	slavePicData.write(ICW3_SLAVE_ID);  // Tells slave its cascade identity

	// ICW4 - Set to 8086 mode
	masterPicData.write(ICW4_8086_MODE);
	slavePicData.write(ICW4_8086_MODE);

	// Mask all interrupts initially
	masterPicData.write(MASK_ALL_INTERRUPTS);
	slavePicData.write(MASK_ALL_INTERRUPTS);
}

void PalmyraOS::kernel::interrupts::PICManager::enableInterrupts()
{
	// Unmask all IRQs on both PICs
	masterPicData.write(UNMASK_ALL_INTERRUPTS);
	slavePicData.write(UNMASK_ALL_INTERRUPTS);
}

///endregion


///region InterruptController
PalmyraOS::kernel::interrupts::InterruptController::InterruptController(GDT::GlobalDescriptorTable* gdt)
	:
	idtHandler(gdt)
{

	uint16_t codeSegment = gdt->getKernelCodeSegmentSelector();

	idtHandler.setDescriptor(0x00, codeSegment, &InterruptServiceRoutine_0x00, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x01, codeSegment, &InterruptServiceRoutine_0x01, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x02, codeSegment, &InterruptServiceRoutine_0x02, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x03, codeSegment, &InterruptServiceRoutine_0x03, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x04, codeSegment, &InterruptServiceRoutine_0x04, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x05, codeSegment, &InterruptServiceRoutine_0x05, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x06, codeSegment, &InterruptServiceRoutine_0x06, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x07, codeSegment, &InterruptServiceRoutine_0x07, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x08, codeSegment, &InterruptServiceRoutine_0x08, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x0A, codeSegment, &InterruptServiceRoutine_0x0A, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x0B, codeSegment, &InterruptServiceRoutine_0x0B, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x0C, codeSegment, &InterruptServiceRoutine_0x0C, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x0D, codeSegment, &InterruptServiceRoutine_0x0D, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x0E, codeSegment, &InterruptServiceRoutine_0x0E, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x20, codeSegment, &InterruptServiceRoutine_0x20, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x21, codeSegment, &InterruptServiceRoutine_0x21, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x22, codeSegment, &InterruptServiceRoutine_0x22, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x23, codeSegment, &InterruptServiceRoutine_0x23, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x24, codeSegment, &InterruptServiceRoutine_0x24, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x25, codeSegment, &InterruptServiceRoutine_0x25, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x26, codeSegment, &InterruptServiceRoutine_0x26, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x27, codeSegment, &InterruptServiceRoutine_0x27, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x28, codeSegment, &InterruptServiceRoutine_0x28, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x29, codeSegment, &InterruptServiceRoutine_0x29, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x2A, codeSegment, &InterruptServiceRoutine_0x2A, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x2B, codeSegment, &InterruptServiceRoutine_0x2B, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x2C, codeSegment, &InterruptServiceRoutine_0x2C, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x2D, codeSegment, &InterruptServiceRoutine_0x2D, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x2E, codeSegment, &InterruptServiceRoutine_0x2E, 0, GateType::InterruptGate);
	idtHandler.setDescriptor(0x2F, codeSegment, &InterruptServiceRoutine_0x2F, 0, GateType::InterruptGate);

	// Desired Privilege Level (DPL) 3, so that it can be invoked by User Processes
	idtHandler.setDescriptor(0x80, codeSegment, &InterruptServiceRoutine_0x80, 3, GateType::InterruptGate);

//	setInterruptHandler(0x00, divisionByZero);

	picManager.enableInterrupts();
	idtHandler.flush();
	activePicManager = &picManager;
}

void PalmyraOS::kernel::interrupts::InterruptController::enableInterrupts()
{
	// from ASM
	enable_interrupts();
}

void PalmyraOS::kernel::interrupts::InterruptController::disableInterrupts()
{
	disable_interrupts();
}

void PalmyraOS::kernel::interrupts::InterruptController::setInterruptHandler(
	uint8_t interrupt_number,
	InterruptHandler interrupt_handler
)
{
	secondary_interrupt_handlers[interrupt_number] = interrupt_handler;
}

///endregion


///region Help Functions
const char* interruptName(uint32_t intNo)
{
	if (intNo == 0x00) return "Division Error";
	if (intNo == 0x01) return "Debug";
	if (intNo == 0x02) return "Non-maskable Interrupt";
	if (intNo == 0x03) return "Breakpoint";
	if (intNo == 0x04) return "Overflow";
	if (intNo == 0x05) return "Bound Range Exceeded Fault";
	if (intNo == 0x06) return "Invalid Opcode Fault";
	if (intNo == 0x07) return "Device Not Available Fault";
	if (intNo == 0x08) return "Double Fault";
	if (intNo == 0x0A) return "Invalid TSS Fault";
	if (intNo == 0x0B) return "Segment Not Present Fault";
	if (intNo == 0x0C) return "Stack-Segment Fault";
	if (intNo == 0x0D) return "General Protection Fault";
	if (intNo == 0x0E) return "Paging Fault";
	if (intNo == 0x20) return "IRQ0 System Clock";
	if (intNo == 0x21) return "IRQ1 Keyboard";
	if (intNo == 0x22) return "IRQ2 Cascade (for 2nd PIC)";
	if (intNo == 0x23) return "IRQ3 COM2 (shared with COM4)";
	if (intNo == 0x24) return "IRQ4 COM1 (shared with COM3)";
	if (intNo == 0x25) return "IRQ5 LPT2 (shared with sound card)";
	if (intNo == 0x26) return "IRQ6 Floppy Disk Controller";
	if (intNo == 0x27) return "IRQ7 LPT1";
	if (intNo == 0x28) return "IRQ8 CMOS Real-time Clock";
	if (intNo == 0x29) return "IRQ9 Free for Peripherals";
	if (intNo == 0x2A) return "IRQ10 Free for Peripherals";
	if (intNo == 0x2B) return "IRQ11 Free for Peripherals";
	if (intNo == 0x2C) return "IRQ12 Mouse";
	if (intNo == 0x2D) return "IRQ13 FPU/Coprocessor/Interrupt for CPU to Communicate with FPU";
	if (intNo == 0x2E) return "IRQ14 Primary ATA Hard Disk";
	if (intNo == 0x2F) return "IRQ15 Secondary ATA Hard Disk";
	if (intNo == 0x80) return "System Call";
	return "Unknown Interrupt";
}

const char* segmentName(uint32_t num)
{
	if (num < 8) return "Null Segment";
	if (num < 16) return "Kernel Code Segment";
	if (num < 24) return "Kernel Data Segment";
	if (num < 32) return "User Code Segment";
	if (num < 40) return "User Data Segment";
	if (num < 48) return "Task Switch Segment";
	return "Invalid Segment";
}

void panicRegisters(const char* message, PalmyraOS::kernel::interrupts::CPURegisters* regs)
{
	PalmyraOS::kernel::kernelPanic(
		R"(
	%s
	----------------------------
	Registers:
	Data Segment:	gs:		%d	(%s)
				fs:		%d	(%s)
				es:		%d	(%s)
				ds:		%d	(%s)
	Pointers:		edi:		%d
				esi:		%d
				ebp:		%d
				esp:		%d
	General P:		eax:		%d
				ebx:		%d
				ecx:		%d
				edx:		%d
	Interrupt:		intNo:	%d	(%s)
				errorCode:%d
	Hardware:		eip:		%d
				cs:		%d	(%s)
				eflags:	%d
				ss:esp:	%d
				ss:		%d	(%s)
	----------------------------
	curr_esp:	%d
	curr_ss:	%d	(%s)
)",
		message,
		regs->gs,
		segmentName(regs->gs),
		regs->fs,
		segmentName(regs->fs),
		regs->es,
		segmentName(regs->es),
		regs->ds,
		segmentName(regs->ds),
		regs->edi,
		regs->esi,
		regs->ebp,
		regs->esp,
		regs->eax,
		regs->ebx,
		regs->ecx,
		regs->edx,
		regs->intNo,
		interruptName(regs->intNo),
		regs->errorCode,
		regs->eip,
		regs->cs,
		segmentName(regs->cs),
		regs->eflags,
		regs->userEsp,
		regs->ss,
		segmentName(regs->ss),
		get_esp(),
		get_ss(),
		segmentName(get_ss())
	);
}
///endregion