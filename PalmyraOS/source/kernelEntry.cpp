#include "core/Display.h"
#include "core/FrameBuffer.h"
#include "core/Interrupts.h"
#include "core/SystemClock.h"
#include "core/boot/multiboot2.h"
#include "core/cpu.h"
#include "core/kernel.h"
#include "core/panic.h"
#include "core/peripherals/Keyboard.h"
#include "core/peripherals/Logger.h"
#include "core/peripherals/Mouse.h"
#include "core/peripherals/RTC.h"
#include "core/tasks/ProcessManager.h"
#include "core/tasks/SystemCalls.h"
#include "core/tasks/WindowManager.h"

#include "palmyraOS/time.h"

#include "core/files/VirtualFileSystem.h"

#include "palmyraOS/palmyraSDK.h"

#include "userland/userland.h"

// Pointers to the start and end of the constructors section (see linker.ld)
extern "C" void (*first_constructor)();
extern "C" void (*last_constructor)();

// Enable protected mode. Defined in bootloader.asm.
extern "C" void enable_protected_mode();
extern "C" void enable_sse();
extern "C" uint32_t test_sse();
extern "C" uint32_t get_kernel_stack_start();
extern "C" uint32_t get_kernel_stack_end();
extern "C" uint32_t get_esp();
extern "C" uint32_t get_ss();

namespace Processes {
    uint64_t proc_1_counter = 0;
    uint64_t proc_2_counter = 0;

    void increase(uint64_t* integer) { (*integer)++; }

    /**
     * @brief Idle process - runs when no other processes are ready
     *
     * The idle process serves several important purposes:
     * 1. Ensures the scheduler always has a ready process
     * 2. Prevents CPU busy-waiting and reduces power consumption
     * 3. Allows CPU to enter low-power states via HLT instruction
     * 4. Provides graceful behavior when all user processes are blocked
     *
     * This should have the LOWEST priority so it only runs when no other
     * processes are ready to execute.
     */
    int idle_process(uint32_t argc, char* argv[]) {
        LOG_INFO("Idle process started (PID 0)");

        while (true) {
            // Yield to allow other processes to run
            // If no other process is ready, we'll be rescheduled immediately
            // sched_yield();

            // Optional: Could add HLT instruction here for power saving
            // asm volatile("hlt");  // Put CPU to sleep until next interrupt
            asm volatile("hlt");
        }

        return 0;
    }

    int process_1(uint32_t argc, char* argv[]) {
        while (true) {
            proc_1_counter++;
            sched_yield();
            //		  return 0;
        }
    }

    int process_2(uint32_t argc, char* argv[]) {
        if (argc > 0) {
            write(1, argv[0], strlen(argv[0]));
            const char* message = "\n";
            write(1, message, 1);
        }

        // Set initial window position and dimensions
        PalmyraOS::SDK::Window window(40, 40, 640, 480, true, "Tests");
        PalmyraOS::SDK::WindowGUI windowFrame(window);

        while (true) {
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

            while (current_time.tv_sec - start_time.tv_sec < 2) {
                clock_gettime(CLOCK_MONOTONIC, &current_time);
                sched_yield();
            }
        }

        const char* message = "I am exiting now!!\n";
        write(1, message, strlen(message));
        _exit(0);
        return -1;
    }
}  // namespace Processes


/**
 * Kernel entry point called from the bootloader with Multiboot 2 information.
 * @param magic Multiboot 2 magic number (0x36d76289)
 * @param multiboot_addr Physical address of Multiboot 2 info structure
 */
extern "C" [[noreturn]] [[maybe_unused]] void kernelEntry(uint32_t magic, uint32_t multiboot_addr);

//  Calls all constructors for global/static objects.
void callConstructors() {
    // Iterate through the constructors and call each one
    // i: pointer to function address
    for (void (**i)() = &first_constructor; i != &last_constructor; i++) { (*i)(); }
}

/**
 * Kernel entry function that is called from the bootloader.
 * Initializes VBE, sets up the kernel, and enters the main loop.
 * @param magic Multiboot 2 magic number from EAX register
 * @param multiboot_addr Physical address of Multiboot 2 info structure
 */
[[noreturn]] void kernelEntry(uint32_t magic, uint32_t multiboot_addr) {
    using namespace PalmyraOS;
    using namespace PalmyraOS::kernel::Multiboot2;

    constexpr uint64_t SHORT_DELAY = 2'500'000L;

    // ----------------------- Call Kernel Constructors -----------------------
    // first construct globals
    callConstructors();

    // ----------------------- Initialize Serial Port for Logging ---------------
    // Initialize the serial port (COM1) for kernel logging output.
    // MUST be called before any LOG_* macros to ensure logs are transmitted correctly.
    // This configures the UART hardware, sets baud rate, enables FIFO buffering,
    // and enables status checking for reliable character transmission.
    kernel::initializeSerialPort(115200);

    LOG_INFO("Entered protected mode.");

    // ----------------------- Validate Multiboot 2 -------------------------------------
    if (!isMultiboot2(magic)) { kernel::kernelPanic("Invalid Multiboot magic! Expected 0x%X, got 0x%X", MULTIBOOT2_BOOTLOADER_MAGIC, magic); }

    LOG_INFO("Multiboot 2 bootloader detected (magic: 0x%X)", magic);

    // Parse Multiboot 2 information
    MultibootInfo multiboot2_info(multiboot_addr);
    if (!multiboot2_info.isValid()) { kernel::kernelPanic("Invalid Multiboot 2 info structure at 0x%X", multiboot_addr); }

    // Log detailed Multiboot 2 information
    logMultiboot2Info(multiboot2_info);

    // Log memory information
    const auto* memInfo = multiboot2_info.getBasicMemInfo();
    if (memInfo) {
        LOG_INFO("Memory Lower: %u KiB", memInfo->mem_lower);
        LOG_INFO("Memory Upper: %u KiB", memInfo->mem_upper);
    }

    // Store ACPI RSDP for future ACPI implementation
    const uint8_t* acpiRSDP = multiboot2_info.getACPIRSDP();
    if (acpiRSDP) {
        LOG_INFO("ACPI RSDP provided by bootloader at 0x%p", acpiRSDP);
        // TODO: Store globally for ACPI initialization
    }

    enable_sse();
    LOG_INFO("Enabled SSE.");

    // ----------------------- Initialize Graphics ----------------------------
    // Initialize graphics using native Multiboot 2 information
    kernel::initializeGraphics(multiboot2_info);
    LOG_INFO("Initialized Graphics.");

    // ----------------------- Check BGA Graphics Adapter -----------------------
    auto [width, height, bpp] = std::tuple{1920, 1080, 32};
    if (kernel::BGA::isAvailable()) {

        if (kernel::BGA::initialize(width, height, bpp)) {
            LOG_INFO("BGA initialized successfully at %dx%dx%d", width, height, bpp);

            // Reinitialize graphics with BGA framebuffer
            if (kernel::initializeGraphicsWithFramebuffer(kernel::BGA::getWidth(), kernel::BGA::getHeight(), kernel::BGA::getFramebufferAddress(), kernel::BGA::getBpp())) {
                LOG_INFO("Graphics reinitialized with BGA framebuffer");
            }
            else { LOG_ERROR("Failed to reinitialize graphics with BGA framebuffer"); }
        }
        else {
            //    textRenderer << " Failed.\n" << SWAP_BUFF();
            LOG_ERROR("BGA initialization failed");
        }
    }
    else { LOG_INFO("BGA Graphics Adapter not available."); }


    // ensure we have enough stack here
    if (get_esp() < get_kernel_stack_end()) kernel::kernelPanic("Kernel Stack overflow by 0x%X bytes", (get_kernel_stack_end() - get_esp()));

    // Logo
    kernel::clearScreen(true);

    // dereference graphics tools
    auto& textRenderer = *kernel::textRenderer_ptr;


    // ----------------------- Initialize Global Descriptor Tables -------------
    // enter protected mode (32-bit)
    enable_protected_mode();
    if (kernel::initializeGlobalDescriptorTable()) {
        textRenderer << "Initialized GDT\n" << SWAP_BUFF();
        LOG_INFO("Initialized GDT.");
    }
    else { kernel::kernelPanic("Failed to initialize the GDT"); }


    // ----------------------- Initialize Interrupts ----------------------------
    if (kernel::initializeInterrupts()) {
        textRenderer << "Initialized Interrupts\n" << SWAP_BUFF();
        LOG_INFO("Initialized Interrupts.");
    }
    else { kernel::kernelPanic("Failed to initialize Interrupts"); }

    // initialize system clock with frequency of 250 Hz initially
    PalmyraOS::kernel::SystemClock::initialize(kernel::SystemClockFrequency);
    textRenderer << "Initialized System Clock at " << kernel::SystemClockFrequency << " Hz.\n" << SWAP_BUFF();
    LOG_INFO("Initialized System Clock at %d Hz.", kernel::SystemClockFrequency);
    kernel::CPU::delay(SHORT_DELAY);

    // ----------------------- Initialize Physical Memory -------------------------------
    textRenderer << "Initializing Physical Memory: " << (memInfo ? memInfo->mem_upper : 0) << " KiB\n" << SWAP_BUFF();
    kernel::initializePhysicalMemory(multiboot2_info);
    kernel::CPU::delay(SHORT_DELAY);

    // ----------------------- Initialize Virtual Memory -------------------------------
    textRenderer << "Initializing Virtual Memory..." << SWAP_BUFF();
    PalmyraOS::kernel::interrupts::InterruptController::enableInterrupts();
    kernel::initializeVirtualMemory(multiboot2_info);
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
        if (hsc_frequency > 50) {

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
        kernel::initializeDrivers();  // ATAs after interrupts run
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
        for (int i = 0; i < 5; ++i) { kernel::CPU::delay(SHORT_DELAY); }
    }
    textRenderer << " Done.\n" << SWAP_BUFF();

    textRenderer << "Initializing TaskManager..." << SWAP_BUFF();
    kernel::TaskManager::initialize();
    textRenderer << " Done.\n" << SWAP_BUFF();
    kernel::CPU::delay(SHORT_DELAY);

    // ---------------------------- Add Processes -----------------------------------

    // Add Important Processes
    {
        // Create the idle process FIRST (will be PID 0) - runs when nothing else is ready
        // The idle process is the fallback task that ensures the scheduler always has work
        //{
        //    char* argv[] = {const_cast<char*>("idle"), nullptr};
        //    kernel::TaskManager::newProcess(Processes::idle_process, kernel::Process::Mode::Kernel, kernel::Process::Priority::VeryLow, 0, argv, true);
        //    LOG_INFO("Idle process created - ensures CPU has a ready task at all times");
        //}

        // Initialize the Window Manager in Kernel Mode
        {
            char* argv[] = {const_cast<char*>("windowsManager.elf"), nullptr};
            kernel::TaskManager::newProcess(kernel::WindowManager::thread, kernel::Process::Mode::Kernel, kernel::Process::Priority::Medium, 0, argv, true);
        }

        // Initialize the menu bar
        {
            char* argv[] = {const_cast<char*>("menuBar.elf"), nullptr};
            kernel::TaskManager::newProcess(PalmyraOS::Userland::builtin::MenuBar::main, kernel::Process::Mode::User, kernel::Process::Priority::Low, 1, argv, true);
        }

        // Run the kernel terminal
        // {
        //     char* argv[] = {const_cast<char*>("terminal.elf"), const_cast<char*>("uname"), nullptr};
        //     // TODO new process must count argv, instead of hard coded -> error prone
        //     kernel::TaskManager::newProcess(PalmyraOS::Userland::builtin::KernelTerminal::main, kernel::Process::Mode::User, kernel::Process::Priority::Low, 2, argv, true);
        // }

        // Background process that counts just for testing
        //{
        //    char* argv[] = {const_cast<char*>("proc1.elf"), const_cast<char*>("-count"), nullptr};
        //    kernel::TaskManager::newProcess(Processes::process_1, kernel::Process::Mode::User, kernel::Process::Priority::Low, 2, argv, true);
        //}

        // Process with a window that exists later for testing
        //{
        //    char* argv[] = {const_cast<char*>("proc2.elf"), const_cast<char*>("-count"), const_cast<char*>("arg3"), nullptr};
        //    kernel::TaskManager::newProcess(Processes::process_2, kernel::Process::Mode::User, kernel::Process::Priority::Low, 3, argv, true);
        //}

        // Process with a window that exists later for testing
        {
            char* argv[] = {const_cast<char*>("clock.elf"), nullptr};
            kernel::TaskManager::newProcess(PalmyraOS::Userland::builtin::KernelClock::main, kernel::Process::Mode::User, kernel::Process::Priority::Low, 1, argv, true);
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
        // {
        //     char* argv[] = {const_cast<char*>("fileManager.elf"), nullptr};
        //     kernel::TaskManager::newProcess(PalmyraOS::Userland::builtin::fileManager::main, kernel::Process::Mode::User, kernel::Process::Priority::Low, 1, argv, true);
        // }

        // Process with a window that exists later for testing
        // {
        //     char* argv[] = {const_cast<char*>("taskManager.elf"), nullptr};
        //     kernel::TaskManager::newProcess(PalmyraOS::Userland::builtin::taskManager::main, kernel::Process::Mode::User, kernel::Process::Priority::Low, 1, argv, true);
        // }
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
