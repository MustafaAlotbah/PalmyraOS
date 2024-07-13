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

#include "palmyraOS/unistd.h"
#include "palmyraOS/time.h"

#include "core/files/VirtualFileSystem.h"

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
  uint32_t proc_1_pid     = 0;
  uint32_t proc_2_pid     = 0;

  void increase(uint64_t* integer)
  {
	  (*integer)++;
  }

  [[noreturn]] int windowManager(uint32_t argc, char* argv[])
  {
	  using namespace PalmyraOS;
	  timespec start_time{};
	  timespec current_time{};

	  clock_gettime(CLOCK_MONOTONIC, &start_time);
	  while (true)
	  {
		  // wait update every 16.666 ms ~ 60 fps
		  clock_gettime(CLOCK_MONOTONIC, &current_time);
		  while (current_time.tv_nsec - start_time.tv_nsec < 16'666L)
		  {
			  clock_gettime(CLOCK_MONOTONIC, &current_time);
			  sched_yield();
		  }
		  start_time = current_time;

		  kernel::WindowManager::composite();
		  sched_yield();
	  }
  }

  int process_1(uint32_t argc, char* argv[])
  {
	  while (true)
	  {
		  proc_1_counter++;
		  proc_1_pid = get_pid();
//		  sched_yield();
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

	  size_t x      = 40;
	  size_t y      = 40;
	  size_t width  = 640;
	  size_t height = 480;

	  // Calculate the total size for 300 pages
	  size_t total_size = 301 * 4096;

	  // Request 300 pages using mmap
	  void* addr = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	  if (addr == MAP_FAILED)
	  {
		  const char* message = "Failed to map memory\n";
		  write(1, message, strlen(message));
	  }
	  else
	  {
		  const char* message = "Success to map memory\n";
		  write(1, message, strlen(message));
	  }

	  uint32_t* frameBuffer = (uint32_t*)0xff11ff22;
	  uint32_t fb_id = initializeWindow(&frameBuffer, x, y, width, height);
	  if (fb_id == 0)
	  {
		  const char* message = "Failed to initialize window\n";
		  write(1, message, strlen(message));
	  }
	  else
	  {
		  const char* message = "Success to initialize window\n";
		  write(1, message, strlen(message));
	  }

	  PalmyraOS::kernel::FrameBuffer fb(width, height, frameBuffer, (uint32_t*)addr);
	  PalmyraOS::kernel::Brush        brush(fb);
	  PalmyraOS::kernel::TextRenderer textRenderer(fb, PalmyraOS::fonts::FontManager::getFont("Arial-12"));


	  for (uint32_t i = 0; i < 640 * 480; ++i) frameBuffer[i] = PalmyraOS::Color::DarkRed.getColorValue();

	  while (true)
	  {
		  increase(&proc_2_counter);
		  proc_2_pid = get_pid();

		  // Draw Window
		  brush.fill(PalmyraOS::Color::Black);
		  brush.fillRectangle(0, 0, width, 20, PalmyraOS::Color::DarkRed);
		  brush.drawHLine(0, width, 0, PalmyraOS::Color::White);
		  brush.drawHLine(0, width, height - 1, PalmyraOS::Color::White);
		  brush.drawVLine(0, 0, height - 1, PalmyraOS::Color::White);
		  brush.drawVLine(width - 1, 0, height - 1, PalmyraOS::Color::White);
		  brush.drawHLine(0, width, 20, PalmyraOS::Color::White);

		  textRenderer << PalmyraOS::Color::White;
		  textRenderer.setCursor(1, 1);
		  textRenderer << "Testy Process\n";
		  textRenderer.reset();
		  textRenderer.setCursor(1, 21);
		  textRenderer << PalmyraOS::Color::LightBlue;

		  textRenderer << "Counter: " << proc_2_counter << "\n";
		  textRenderer << "Counter proc0: " << proc_1_counter << "\n";
		  textRenderer << "my pid: " << get_pid() << "\n";

		  fb.swapBuffers();
		  sched_yield();

		  if (proc_2_counter >= 1000'000) break;
	  }

//	  brush.fill(PalmyraOS::Color::DarkBlue);
	  textRenderer << "I will exit in 2 seconds" << "\n";
	  fb.swapBuffers();

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
	  closeWindow(fb_id);
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
	constexpr uint64_t SHORT_DELAY = 2500'000'0;

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
	textRenderer << "Initialized System Clock at " << kernel::SystemClockFrequency << " Hz.\n" << SWAP_BUFF();
	LOG_INFO("Initialized System Clock at %d Hz.", kernel::SystemClockFrequency);
	kernel::CPU::delay(SHORT_DELAY);

	// Measurements that depend on system clock
	{
		PalmyraOS::kernel::interrupts::InterruptController::enableInterrupts();
		PalmyraOS::kernel::CPU::initialize();
		textRenderer << "CPU Frequency: " << PalmyraOS::kernel::CPU::detectCpuFrequency() << " MHz.\n" << SWAP_BUFF();
		PalmyraOS::kernel::interrupts::InterruptController::disableInterrupts();
		kernel::CPU::delay(SHORT_DELAY);
	}

	// ----------------------- Initialize Physical Memory -------------------------------
	textRenderer << "Initializing Physical Memory\n" << SWAP_BUFF();
	kernel::initializePhysicalMemory(x86_multiboot_info);
	kernel::CPU::delay(SHORT_DELAY);

	// ----------------------- Initialize Virtual Memory -------------------------------
	textRenderer << "Initializing Virtual Memory\n" << SWAP_BUFF();
	kernel::CPU::delay(SHORT_DELAY);

	kernel::initializeVirtualMemory(x86_multiboot_info);
	textRenderer << "Virtual Memory is initialized\n" << SWAP_BUFF();
	kernel::CPU::delay(SHORT_DELAY);

//	kernel::testMemory();
//	textRenderer << "Passed Heap Tests\n" << SWAP_BUFF();


	// ----------------------- Virtual File System -------------------------------

	kernel::vfs::VirtualFileSystem::initialize();

	kernel::initializeDrivers(); // ATAs after interrupts run

	// ----------------------- Initialize Tasks -------------------------------
	kernel::TaskManager::initialize();
	textRenderer << "TaskManager is initialized.\n" << SWAP_BUFF();
	kernel::CPU::delay(SHORT_DELAY);

	kernel::SystemCallsManager::initialize();
	textRenderer << "SystemCallsManager is initialized.\n" << SWAP_BUFF();
	kernel::CPU::delay(SHORT_DELAY);

	kernel::WindowManager::initialize();
	textRenderer << "WindowManager is initialized.\n" << SWAP_BUFF();
	kernel::CPU::delay(SHORT_DELAY);


	// ----------------------- Initialize Peripherals -------------------------------

	kernel::RTC::initialize();

	kernel::Keyboard::initialize();

	// ---------------------------- Add Processes -----------------------------------

	// Add Important Processes
	{
		// Initialize the Window Manager in Kernel Mode
		{
			char* argv[] = { nullptr };
			kernel::TaskManager::newProcess(
				Processes::windowManager,
				kernel::Process::Mode::Kernel,
				kernel::Process::Priority::Medium,
				0, argv
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
				argv
			);
		}

		// Run the kernel terminal
		{
			char* argv[] = {
				const_cast<char*>("keylogger.exe"),
				const_cast<char*>("-count"),
				const_cast<char*>("arg3"), nullptr
			};
			kernel::TaskManager::newProcess(
				PalmyraOS::Userland::builtin::KernelTerminal::main,
				kernel::Process::Mode::User,
				kernel::Process::Priority::Low,
				3,
				argv
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
				argv
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
				argv
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


