#include "boot/multiboot.h"
#include "core/FrameBuffer.h"
#include "core/Font.h"
#include "core/VBE.h"
#include "core/kernel.h"
#include "core/panic.h"
#include "core/cpu.h"
#include "core/Interrupts.h"
#include "core/SystemClock.h"
#include "core/memory/PhysicalMemory.h"
#include "core/Logger.h"
#include "core/tasks/ProcessManager.h"
#include "core/tasks/SystemCalls.h"

#include "palmyraOS/unistd.h"
#include "libs/string.h"


// Pointers to the start and end of the constructors section (see linker.ld)
extern "C" void (* first_constructor)();
extern "C" void (* last_constructor)();

// Enable protected mode. Defined in bootloader.asm.
extern "C" void enable_protected_mode();
extern "C" void enable_sse();
extern "C" uint32_t test_sse();
extern "C" uint32_t get_kernel_stack_start();
extern "C" uint32_t get_kernel_stack_end();
extern "C" uint32_t get_esp();
extern "C" uint32_t get_ss();

namespace Processes
{
  uint64_t proc_1_counter = 0;
  uint64_t proc_2_counter = 0;
  uint32_t proc_1_pid     = 0;
  uint32_t proc_2_pid     = 0;

  void print_number()
  {
	  using namespace PalmyraOS;
	  auto& vbe = *PalmyraOS::kernel::vbe_ptr;
	  kernel::Brush        brush(vbe.getFrameBuffer());
	  kernel::TextRenderer textRenderer(vbe.getFrameBuffer(), fonts::FontManager::getFont("Arial-12"));

	  // Clear the screen
	  brush.fill(Color::DarkBlue);

	  // Logo
	  textRenderer << Color::Orange << "Palmyra" << Color::LightBlue << "OS ";
	  textRenderer << Color::White << "v0.01\n";
	  brush.drawHLine(1, 150, textRenderer.getCursorY() + 2, Color::White);
	  textRenderer << "\n";

	  textRenderer << "Allocated Pages: " << DEC() << kernel::kernelPagingDirectory_ptr->getNumAllocatedPages() << "\n";

	  // Information
	  textRenderer << "Counter 1: " << DEC() << proc_1_counter << "\n";
	  textRenderer << "Counter 2: " << DEC() << proc_2_counter << "\n";

	  textRenderer << "proc_0_pid: " << DEC() << get_pid() << "\n";
	  textRenderer << "proc_1_pid: " << DEC() << proc_1_pid << "\n";
	  textRenderer << "proc_2_pid: " << DEC() << proc_2_pid << "\n";

	  textRenderer << "proc 0 esp: " << HEX() << kernel::TaskManager::getProcess(0)->stack_.esp << "\n";
	  textRenderer << "proc 0 usp: " << HEX() << kernel::TaskManager::getProcess(0)->stack_.userEsp << "\n";

	  textRenderer << "proc 1 esp: " << HEX() << kernel::TaskManager::getProcess(1)->stack_.esp << "\n";
	  textRenderer << "proc 1 usp: " << HEX() << kernel::TaskManager::getProcess(1)->stack_.userEsp << "\n";

	  textRenderer << "proc 2 ec: " << DEC() << kernel::TaskManager::getProcess(2)->exitCode_ << "\n";
	  textRenderer << "proc 2 state: " <<
				   (
					   kernel::TaskManager::getProcess(2)->state_ == kernel::Process::State::Terminated ?
					   "Terminated" : "Not terminated"
				   )
				   << "\n";

	  char     proc_stdout[50] = { 0 };
	  for (int i               = 0; i < kernel::TaskManager::getProcess(2)->stdout_.size(); ++i)
	  {
		  if (i >= 50) break;
		  proc_stdout[i] = kernel::TaskManager::getProcess(2)->stdout_[i];
	  }
	  textRenderer << "proc 2 stdout: " << proc_stdout << "\n";

	  textRenderer << SWAP_BUFF();
  }

  int process_0(uint32_t argc, char* argv[])
  {
	  while (true)
	  {
		  print_number();
	  }
  }

  int process_1(uint32_t argc, char* argv[])
  {
	  while (true)
	  {
		  proc_1_counter++;
		  proc_1_pid = get_pid();
	  }
  }

  int process_2(uint32_t argc, char* argv[])
  {
	  if (argc > 0)
	  {
		  write(1, argv[0], strlen(argv[0]));
	  }
	  while (true)
	  {
		  proc_2_counter++;
		  proc_2_pid = get_pid();
		  if (proc_2_counter >= 1'000'000)
		  {
			  const char* message = "\nI am exiting now!!\n";
			  write(1, message, 19);
			  _exit(0);
		  }
	  }
  }


}



/**
 * Kernel entry point called from the bootloader with multiboot information.
 * @param x86_multiboot_info Pointer to the multiboot information structure.
 */
extern "C" [[noreturn]] [[maybe_unused]] void kernelEntry(multiboot_info_t* x86_multiboot_info);

//  Calls all constructors for global/static objects.
void callConstructors()
{
	// Iterate through the constructors and call each one
	// i: pointer to function address
	for (void (** i)() = &first_constructor; i != &last_constructor; i++)
	{
		(*i)();
	}
}

/**
 * Kernel entry function that is called from the bootloader.
 * Initializes VBE, sets up the kernel, and enters the main loop.
 * @param x86_multiboot_info Pointer to the multiboot information structure.
 */
[[noreturn]] void kernelEntry(multiboot_info_t* x86_multiboot_info)
{
	using namespace PalmyraOS;

	// ----------------------- Call Kernel Constructors -----------------------
	// first construct globals
	callConstructors();

	// ----------------------- Enable Protected Mode --------------------------
	// enter protected mode (32-bit)
	enable_protected_mode();
	LOG_INFO("Entered protected mode.");
	LOG_INFO("Memory Lower: %d KiB", x86_multiboot_info->mem_lower);
	LOG_INFO("Memory Upper: %d KiB", x86_multiboot_info->mem_upper);

	enable_sse();
	LOG_INFO("Enabled SSE.");
	// ----------------------- Initialize Graphics ----------------------------

	// Retrieve VBE mode information from the multiboot info structure
	auto* vbe_mode_info    = (vbe_mode_info_t*)(uintptr_t)x86_multiboot_info->vbe_mode_info;
	auto* vbe_control_info = (vbe_control_info_t*)(uintptr_t)x86_multiboot_info->vbe_control_info;
	kernel::initializeGraphics(vbe_mode_info, vbe_control_info);
	LOG_INFO("Initialized Graphics.");

	// dereference graphics tools
	auto& textRenderer = *kernel::textRenderer_ptr;

	// ensure we have enough stack here
	if (get_esp() < get_kernel_stack_end())
		kernel::kernelPanic("Kernel Stack overflow by 0x%X bytes", (get_kernel_stack_end() - get_esp()));

	// Logo
	kernel::clearScreen(true);


	// ----------------------- Initialize Global Descriptor Tables -------------
	if (kernel::initializeGlobalDescriptorTable())
	{
		textRenderer << "Initialized GDT\n" << SWAP_BUFF();
		LOG_INFO("Initialized GDT.");
	}
	else
	{
		kernel::kernelPanic("Failed to initialize the GDT");
	}


	// ----------------------- Initialize Interrupts ----------------------------
	if (kernel::initializeInterrupts())
	{
		textRenderer << "Initialized Interrupts\n" << SWAP_BUFF();
		LOG_INFO("Initialized Interrupts.");
	}
	else
	{
		kernel::kernelPanic("Failed to initialize Interrupts");
	}

	// initialize system clock with frequency of 250 Hz
	PalmyraOS::kernel::SystemClock::initialize(kernel::SystemClockFrequency);
	LOG_INFO("Initialized System Clock at %d Hz.", kernel::SystemClockFrequency);



	// ----------------------- Initialize Physical Memory -------------------------------
	textRenderer << "Initializing Physical Memory\n" << SWAP_BUFF();
	kernel::initializePhysicalMemory(x86_multiboot_info);


	// ----------------------- Initialize Virtual  Memory -------------------------------
	textRenderer << "Initializing Virtual Memory\n" << SWAP_BUFF();
	kernel::initializeVirtualMemory(x86_multiboot_info);
	textRenderer << "Virtual Memory is initialized\n" << SWAP_BUFF();


	kernel::testMemory();
	// from here we can use th

	{
		std::vector<int, kernel::KernelHeapAllocator<int>> vec;
		vec.push_back(5);

	}


	kernel::TaskManager::initialize();
	kernel::SystemCallsManager::initialize();

	{
		char* argv0[] = { nullptr };
		kernel::TaskManager::newProcess(
			Processes::process_0,
			kernel::Process::Mode::Kernel,
			kernel::Process::Priority::Medium,
			0, argv0
		);

		char* argv1[] = { const_cast<char*>("proc1.exe"), const_cast<char*>("-count"), nullptr };
		kernel::TaskManager::newProcess(
			Processes::process_1,
			kernel::Process::Mode::User,
			kernel::Process::Priority::Low,
			2,
			argv1
		);

		char* argv2[] =
				{ const_cast<char*>("proc2.exe"), const_cast<char*>("-count"), const_cast<char*>("arg3"), nullptr };
		kernel::TaskManager::newProcess(
			Processes::process_2,
			kernel::Process::Mode::User,
			kernel::Process::Priority::Low,
			2,
			argv2
		);
	}



	// Now enable maskable interrupts
	LOG_INFO("Enabling Interrupts.");
	textRenderer << "Enabling Interrupts.\n" << SWAP_BUFF();
	PalmyraOS::kernel::interrupts::InterruptController::enableInterrupts();


	while (true);

	LOG_INFO("Moving to setup()...");

	// Here we loop, until a system clock interrupts and starts scheduling.
	while (true);
}


