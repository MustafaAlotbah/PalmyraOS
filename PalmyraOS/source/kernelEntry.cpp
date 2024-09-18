#include "core/boot/multiboot.h"
#include "core/FrameBuffer.h"
#include "core/Font.h"
#include "core/VBE.h"
#include "core/kernel.h"
#include "core/panic.h"
#include "core/cpu.h"
#include "core/Interrupts.h"
#include "core/SystemClock.h"
#include "core/peripherals/Logger.h"
#include "core/tasks/ProcessManager.h"
#include "core/tasks/SystemCalls.h"
#include "core/tasks/WindowManager.h"
#include "core/peripherals/RTC.h"
#include "core/peripherals/Keyboard.h"
#include "core/peripherals/Mouse.h"

#include "palmyraOS/time.h"

#include "core/files/VirtualFileSystem.h"

#include "libs/palmyraSDK.h"

#include "userland/userland.h"

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

  void increase(uint64_t* integer)
  {
	  (*integer)++;
  }

  int process_1(uint32_t argc, char* argv[])
  {
	  while (true)
	  {
		  proc_1_counter++;
		  sched_yield();
//		  return 0;
	  }
  }

  int process_2(uint32_t argc, char* argv[])
  {
	  if (argc > 0)
	  {
		  write(1, argv[0], strlen(argv[0]));
		  const char* message = "\n";
		  write(1, message, 1);
	  }

	  // Set initial window position and dimensions
	  PalmyraOS::SDK::Window    window(40, 40, 640, 480, true, "Tests");
	  PalmyraOS::SDK::WindowGUI windowFrame(window);

	  while (true)
	  {
		  increase(&proc_2_counter);
		  windowFrame.text() << PalmyraOS::Color::LighterBlue;

		  windowFrame.text() << "Counter: " << proc_2_counter << "\n";
		  windowFrame.text() << "Counter proc0: " << proc_1_counter << "\n";
		  windowFrame.text() << "my pid: " << get_pid() << "\n";

		  windowFrame.swapBuffers();
		  sched_yield();

		  if (proc_2_counter >= 1'000) break;
	  }

	  windowFrame.text() << "I will exit in 2 seconds" << "\n";
	  windowFrame.swapBuffers();

	  // wait
	  {
		  timespec start_time{};
		  timespec current_time{};

		  clock_gettime(CLOCK_MONOTONIC, &start_time);
		  clock_gettime(CLOCK_MONOTONIC, &current_time);

		  while (current_time.tv_sec - start_time.tv_sec < 2)
		  {
			  clock_gettime(CLOCK_MONOTONIC, &current_time);
			  sched_yield();
		  }
	  }

	  const char* message = "I am exiting now!!\n";
	  write(1, message, strlen(message));
	  _exit(0);
	  return -1;
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
	constexpr uint64_t SHORT_DELAY = 2'500'000L;

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

	// initialize system clock with frequency of 250 Hz initially
	PalmyraOS::kernel::SystemClock::initialize(kernel::SystemClockFrequency);
	textRenderer << "Initialized System Clock at " << kernel::SystemClockFrequency << " Hz.\n" << SWAP_BUFF();
	LOG_INFO("Initialized System Clock at %d Hz.", kernel::SystemClockFrequency);
	kernel::CPU::delay(SHORT_DELAY);

	// ----------------------- Initialize Physical Memory -------------------------------
	textRenderer << "Initializing Physical Memory: " << x86_multiboot_info->mem_upper << " KiB\n" << SWAP_BUFF();
	kernel::initializePhysicalMemory(x86_multiboot_info);
	kernel::CPU::delay(SHORT_DELAY);

	// ----------------------- Initialize Virtual Memory -------------------------------
	textRenderer << "Initializing Virtual Memory..." << SWAP_BUFF();
	PalmyraOS::kernel::interrupts::InterruptController::enableInterrupts();
	kernel::initializeVirtualMemory(x86_multiboot_info);
	PalmyraOS::kernel::interrupts::InterruptController::disableInterrupts();
	textRenderer << " Done.\n" << SWAP_BUFF();
	kernel::CPU::delay(SHORT_DELAY);

//	kernel::testMemory();
//	textRenderer << "Passed Heap Tests\n" << SWAP_BUFF();


	// ----------------------- Virtual File System -------------------------------

	textRenderer << "Initializing Virtual File System..." << SWAP_BUFF();
	kernel::vfs::VirtualFileSystem::initialize();
	textRenderer << " Done.\n" << SWAP_BUFF();

	kernel::RTC::initialize();
	textRenderer << "RTC is initialized.\n" << SWAP_BUFF();

	// Measure Frequencies based on RTC
	textRenderer << "Measuring CPU frequency.." << SWAP_BUFF();
	{
		PalmyraOS::kernel::interrupts::InterruptController::enableInterrupts();
		PalmyraOS::kernel::CPU::initialize();
		PalmyraOS::kernel::interrupts::InterruptController::disableInterrupts();
		textRenderer << "[CPU: " << PalmyraOS::kernel::CPU::getCPUFrequency() << " MHz] " << SWAP_BUFF();
		textRenderer << "[HSC: " << PalmyraOS::kernel::CPU::getHSCFrequency() << " Hz] " << SWAP_BUFF();

		uint32_t hsc_frequency = PalmyraOS::kernel::CPU::getHSCFrequency();
		if (hsc_frequency > 50)
		{

			textRenderer << " Updating HSC to " << hsc_frequency << " Hz] " << SWAP_BUFF();
			PalmyraOS::kernel::SystemClock::setFrequency(hsc_frequency);
		}

		kernel::CPU::delay(SHORT_DELAY);
	}
	textRenderer << " Done.\n" << SWAP_BUFF();



	// ----------------------- Initialize Tasks -------------------------------


	{
		textRenderer << "Initializing ATA.." << SWAP_BUFF();
		PalmyraOS::kernel::interrupts::InterruptController::enableInterrupts();
		kernel::initializeDrivers(); // ATAs after interrupts run
		textRenderer << " Done.\n" << SWAP_BUFF();
		LOG_INFO("Initialized Drivers.");

		textRenderer << "Initializing Partitions..." << SWAP_BUFF();
		kernel::initializePartitions();
		PalmyraOS::kernel::interrupts::InterruptController::disableInterrupts();
		textRenderer << " Done.\n" << SWAP_BUFF();
		LOG_INFO("Initialized Partitions.");
	}

	textRenderer << "Initializing SystemCallsManager..." << SWAP_BUFF();
	kernel::SystemCallsManager::initialize();
	textRenderer << " Done.\n" << SWAP_BUFF();
	kernel::CPU::delay(SHORT_DELAY);

	textRenderer << "Initializing WindowManager..." << SWAP_BUFF();
	kernel::WindowManager::initialize();
	textRenderer << " Done.\n" << SWAP_BUFF();
	kernel::CPU::delay(SHORT_DELAY);


	// ----------------------- Initialize Peripherals -------------------------------

	textRenderer << "Initializing Keyboard Driver..." << SWAP_BUFF();
	kernel::Keyboard::initialize();
	textRenderer << " Done.\n" << SWAP_BUFF();

	textRenderer << "Initializing Mouse Driver..." << SWAP_BUFF();
	kernel::Mouse::initialize();
	textRenderer << " Done.\n" << SWAP_BUFF();

	textRenderer << "Initializing Binaries.." << SWAP_BUFF();
	kernel::initializeBinaries();
	textRenderer << " Done.\n" << SWAP_BUFF();

	textRenderer << "Measuring CPU frequency.." << SWAP_BUFF();
	{
		PalmyraOS::kernel::interrupts::InterruptController::enableInterrupts();
		PalmyraOS::kernel::CPU::initialize();
		PalmyraOS::kernel::interrupts::InterruptController::disableInterrupts();

		textRenderer << "[CPU: " << PalmyraOS::kernel::CPU::getCPUFrequency() << " MHz] " << SWAP_BUFF();
		textRenderer << "[HSC: " << PalmyraOS::kernel::CPU::getHSCFrequency() << " Hz] " << SWAP_BUFF();
		for (int i = 0; i < 5; ++i)
		{
			kernel::CPU::delay(SHORT_DELAY);
		}
	}
	textRenderer << " Done.\n" << SWAP_BUFF();

	textRenderer << "Initializing TaskManager..." << SWAP_BUFF();
	kernel::TaskManager::initialize();
	textRenderer << " Done.\n" << SWAP_BUFF();
	kernel::CPU::delay(SHORT_DELAY);

	// ---------------------------- Add Processes -----------------------------------

	// Add Important Processes
	{
		// Initialize the Window Manager in Kernel Mode
		{
			char* argv[] = { const_cast<char*>("windowsManager.exe"), nullptr };
			kernel::TaskManager::newProcess(
				kernel::WindowManager::thread,
				kernel::Process::Mode::Kernel,
				kernel::Process::Priority::Medium,
				0, argv, true
			);
		}

		// Initialize the menu bar
		{
			char* argv[] = {
				const_cast<char*>("menuBar.exe"), nullptr
			};
			kernel::TaskManager::newProcess(
				PalmyraOS::Userland::builtin::MenuBar::main,
				kernel::Process::Mode::User,
				kernel::Process::Priority::Low,
				1,
				argv, true
			);
		}

		// Run the kernel terminal
		{
			char* argv[] = {
				const_cast<char*>("kernelTerminal.exe"),
				const_cast<char*>("uname"),
				nullptr
			};
			kernel::TaskManager::newProcess(
				PalmyraOS::Userland::builtin::KernelTerminal::main,
				kernel::Process::Mode::User,
				kernel::Process::Priority::Low,
				3,
				argv, true
			);
		}

		// Background process that counts just for testing
		{
			char* argv[] = { const_cast<char*>("proc1.exe"), const_cast<char*>("-count"), nullptr };
			kernel::TaskManager::newProcess(
				Processes::process_1,
				kernel::Process::Mode::User,
				kernel::Process::Priority::Low,
				2,
				argv, true
			);
		}

		// Process with a window that exists later for testing
		{
			char* argv[] = {
				const_cast<char*>("proc2.exe"),
				const_cast<char*>("-count"),
				const_cast<char*>("arg3"), nullptr
			};
			kernel::TaskManager::newProcess(
				Processes::process_2,
				kernel::Process::Mode::User,
				kernel::Process::Priority::Low,
				3,
				argv, true
			);
		}

		// Process with a window that exists later for testing
		{
			char* argv[] = {
				const_cast<char*>("clock.exe"), nullptr
			};
			kernel::TaskManager::newProcess(
				PalmyraOS::Userland::builtin::KernelClock::main,
				kernel::Process::Mode::User,
				kernel::Process::Priority::Low,
				1,
				argv, true
			);
		}

		// Process with a window that exists later for testing
//		{
//			char* argv[] = {
//				const_cast<char*>("events.exe"), nullptr
//			};
//			kernel::TaskManager::newProcess(
//				PalmyraOS::Userland::Tests::events::main,
//				kernel::Process::Mode::User,
//				kernel::Process::Priority::Low,
//				1,
//				argv, true
//			);
//		}

		// Process with a window that exists later for testing
		{
			char* argv[] = {
				const_cast<char*>("explorer.exe"), nullptr
			};
			kernel::TaskManager::newProcess(
				PalmyraOS::Userland::builtin::Explorer::main,
				kernel::Process::Mode::User,
				kernel::Process::Priority::Low,
				1,
				argv, true
			);
		}


	}


	// ----------------------- System Entry End -------------------------------


	// Now enable maskable interrupts
	{
		LOG_INFO("Enabling Interrupts.");
		PalmyraOS::kernel::interrupts::InterruptController::enableInterrupts();
		textRenderer << "Interrupts are enabled.\n" << SWAP_BUFF();
		LOG_INFO("Interrupts are enabled.");
	}

	// Scheduler must start somewhere inside the loop
	while (true);

	LOG_ERROR("Passed after while(true)!");
	// Here we loop, until a system clock interrupts and starts scheduling.
	while (true);
}


