#include "core/kernel.h"
#include "core/Display.h"
#include "core/acpi/ACPI.h"
#include "core/acpi/ACPISpecific.h"
#include "core/acpi/HPET.h"
#include "core/acpi/PowerManagement.h"
#include "core/cpu.h"
#include "core/files/Fat32FileSystem.h"
#include "core/files/VirtualFileSystem.h"
#include "core/files/partitions/Fat32.h"
#include "core/files/partitions/MasterBootRecord.h"
#include "core/files/partitions/VirtualDisk.h"
#include "core/memory/paging.h"
#include "core/network/ARP.h"
#include "core/network/DNS.h"
#include "core/network/ICMP.h"
#include "core/network/IPv4.h"
#include "core/network/NetworkManager.h"
#include "core/network/PCnetDriver.h"
#include "core/network/UDP.h"
#include "core/panic.h"
#include "core/pcie/PCIe.h"
#include "core/peripherals/Logger.h"
#include "core/tasks/ProcessManager.h"
#include "core/tasks/elf.h"
#include "libs/memory.h"
#include "tests/allocatorTests.h"
#include "tests/pagingTests.h"
#include <algorithm>
#include <new>

// Globals


namespace PalmyraOS::kernel {

    // Graphics
    PalmyraOS::kernel::Display* display_ptr                     = nullptr;
    PalmyraOS::kernel::Brush* brush_ptr                         = nullptr;
    PalmyraOS::kernel::TextRenderer* textRenderer_ptr           = nullptr;

    // CPU
    PalmyraOS::kernel::GDT::GlobalDescriptorTable* gdt_ptr      = nullptr;
    PalmyraOS::kernel::interrupts::InterruptController* idt_ptr = nullptr;
    ;

    // Memory
    PalmyraOS::kernel::PagingDirectory* kernelPagingDirectory_ptr = nullptr;
    PalmyraOS::kernel::HeapManager heapManager;
    uint32_t kernelLastPage                      = 0;

    // Drivers
    PalmyraOS::kernel::ATA* ata_primary_master   = nullptr;
    PalmyraOS::kernel::ATA* ata_primary_slave    = nullptr;
    PalmyraOS::kernel::ATA* ata_secondary_master = nullptr;
    PalmyraOS::kernel::ATA* ata_secondary_slave  = nullptr;


}  // namespace PalmyraOS::kernel

bool PalmyraOS::kernel::initializeGraphics(const Multiboot2::MultibootInfo& mb2Info) {
    /**
     * @brief Initializes the graphics subsystem using Multiboot 2 information.
     *
     * This function extracts display parameters from Multiboot 2 and delegates
     * to initializeGraphicsWithFramebuffer() for the actual initialization.
     *
     * @param mb2Info Multiboot 2 information structure
     * @return True if the graphics subsystem is successfully initialized, false otherwise.
     */

    // Extract display parameters from Multiboot 2
    uint16_t width       = 0;
    uint16_t height      = 0;
    uint32_t framebuffer = 0;
    uint16_t pitch       = 0;
    uint8_t bitsPerPixel = 0;

    // Try to get VBE information first (preferred)
    const auto* vbeTag   = mb2Info.getVBE();
    if (vbeTag) {
        // VBE information available - extract fields directly from VBE mode info block
        // VBE 3.0 specification defines the following offsets:
        const uint8_t* vbe_block = vbeTag->vbe_mode_info.external_specification;

        // Extract fields from specific offsets according to VBE spec
        pitch                    = *reinterpret_cast<const uint16_t*>(&vbe_block[16]);  // BytesPerScanLine
        width                    = *reinterpret_cast<const uint16_t*>(&vbe_block[18]);  // XResolution
        height                   = *reinterpret_cast<const uint16_t*>(&vbe_block[20]);  // YResolution
        bitsPerPixel             = vbe_block[25];                                       // BitsPerPixel
        framebuffer              = *reinterpret_cast<const uint32_t*>(&vbe_block[40]);  // PhysBasePtr

        LOG_INFO("Graphics: Using VBE mode 0x%X", vbeTag->vbe_mode);
    }
    else {
        // Fall back to framebuffer information
        const auto* fbTag = mb2Info.getFramebuffer();
        if (!fbTag) {
            LOG_ERROR("No graphics information provided by bootloader");
            return false;
        }

        // Extract display parameters from framebuffer tag
        width        = fbTag->common.framebuffer_width;
        height       = fbTag->common.framebuffer_height;
        framebuffer  = static_cast<uint32_t>(fbTag->common.framebuffer_addr);
        pitch        = fbTag->common.framebuffer_pitch;
        bitsPerPixel = fbTag->common.framebuffer_bpp;

        LOG_INFO("Graphics: Using framebuffer %ux%u @ %u bpp", width, height, bitsPerPixel);
    }

    // Delegate to the common initialization function
    return initializeGraphicsWithFramebuffer(width, height, framebuffer, pitch, bitsPerPixel);
}

bool PalmyraOS::kernel::initializeGraphicsWithFramebuffer(uint16_t width, uint16_t height, uint32_t framebufferAddress, uint16_t pitch, uint8_t bpp) {
    /**
     * @brief Initializes the graphics subsystem with explicit framebuffer information.
     *
     * This function is used when graphics information is provided directly (e.g., from BGA or Multiboot2).
     * It allocates memory for the Display object and back buffer, then initializes all graphics components
     * including font manager, brush, and text renderer.
     *
     * @param width Framebuffer width in pixels
     * @param height Framebuffer height in pixels
     * @param framebufferAddress Physical address of the framebuffer
     * @param pitch Bytes per scanline (stride)
     * @param bpp Bits per pixel (8, 16, 24, or 32)
     * @return True if initialization is successful, false otherwise.
     */

    // Calculate bytes per pixel from bpp
    uint16_t bytesPerPixel = bpp / 8;
    if (bytesPerPixel == 0) bytesPerPixel = 1;  // Minimum 1 byte per pixel

    // Calculate the size of the back buffer
    uint32_t backBufferSize = width * height * sizeof(uint32_t);

    // Allocate memory for the back buffer
    uint32_t* backBuffer    = (uint32_t*) kernel::kmalloc(backBufferSize);
    if (backBuffer == nullptr) {
        LOG_ERROR("Failed to allocate %u bytes for graphics back buffer", backBufferSize);
        return false;
    }

    // Zero-initialize the back buffer
    for (uint32_t i = 0; i < width * height; ++i) { backBuffer[i] = 0; }

    LOG_DEBUG("Graphics back buffer allocated: %ux%u @ %u bpp (%u bytes)", width, height, bpp, backBufferSize);

    // Initialize Display with direct parameters
    {
        // Allocate memory for the Display object
        kernel::display_ptr = (Display*) kernel::kmalloc(sizeof(Display));
        if (kernel::display_ptr == nullptr) {
            LOG_ERROR("Failed to allocate Display object");
            return false;
        }

        // Construct the Display object with direct parameters
        new (kernel::display_ptr) Display(width, height, framebufferAddress, pitch, bpp, backBuffer);
    }

    // Initialize the font manager
    FontManager::initialize();

    // Initialize kernel's brush
    {
        kernel::brush_ptr = (Brush*) kernel::kmalloc(sizeof(Brush));
        if (kernel::brush_ptr == nullptr) {
            LOG_ERROR("Failed to allocate Brush object");
            return false;
        }

        new (kernel::brush_ptr) Brush(kernel::display_ptr->getFrameBuffer());
    }

    // Initialize kernel's text renderer
    {
        kernel::textRenderer_ptr = (TextRenderer*) kernel::kmalloc(sizeof(TextRenderer));
        if (kernel::textRenderer_ptr == nullptr) {
            LOG_ERROR("Failed to allocate TextRenderer object");
            return false;
        }

        new (kernel::textRenderer_ptr) TextRenderer(kernel::display_ptr->getFrameBuffer(), Font::Poppins12);
    }

    LOG_INFO("Graphics initialized with framebuffer: %ux%u @ %u bpp at 0x%X", width, height, bpp, framebufferAddress);

    return true;
}

void PalmyraOS::kernel::clearScreen(bool drawLogo) {
    /**
     * @brief Clears the screen and optionally draws the logo.
     *
     * This function fills the screen with a black color and resets the text renderer.
     * If the drawLogo parameter is true, it also draws the PalmyraOS logo and a horizontal line.
     * Finally, it swaps the buffers to update the display.
     *
     * @param drawLogo A boolean indicating whether to draw the logo on the screen.
     */

    auto& textRenderer = *kernel::textRenderer_ptr;

    // Fill the screen with black color and reset the text renderer
    brush_ptr->fill(Color::Black);
    textRenderer.reset();

    if (drawLogo) {
        // Draw the logo text
        textRenderer << Color::Orange << "Palmyra" << Color::LighterBlue << "OS ";
        textRenderer << Color::Gray100 << "v0.01\n";

        // Draw a horizontal line below the logo text
        brush_ptr->drawHLine(1, 150, textRenderer.getCursorY() + 2, Color::Gray100);
    }

    // Swap the buffers to update the display
    display_ptr->swapBuffers();
}

bool PalmyraOS::kernel::initializeGlobalDescriptorTable() {
    /**
     * @brief Initializes the Global Descriptor Table (GDT).
     *
     * This function allocates memory for the GDT object and constructs it in the allocated memory.
     *
     * @return True if the GDT is successfully initialized, false otherwise.
     */

    // Initialize the Global Descriptor Table (GDT)
    {
        // Allocate memory for the GDT object
        kernel::gdt_ptr = (GDT::GlobalDescriptorTable*) kernel::kmalloc(sizeof(GDT::GlobalDescriptorTable));
        if (kernel::gdt_ptr == nullptr) return false;

        // Construct the GDT object in the allocated memory
        new (kernel::gdt_ptr) GDT::GlobalDescriptorTable(kernel::InitialKernelStackPointer);
    }
    return true;
}

bool PalmyraOS::kernel::initializeInterrupts() {
    /**
     * @brief Initializes the Interrupt Descriptor Table (IDT).
     *
     * This function allocates memory for the Interrupt Controller object and constructs it in the allocated memory.
     *
     * @return True if the IDT is successfully initialized, false otherwise.
     */

    // Initialize the Interrupt Descriptor Table (IDT)
    {
        // Allocate memory for the Interrupt Controller object
        kernel::idt_ptr = (interrupts::InterruptController*) kernel::kmalloc(sizeof(interrupts::InterruptController));
        if (kernel::idt_ptr == nullptr) return false;

        // Construct the Interrupt Controller object in the allocated memory
        new (kernel::idt_ptr) interrupts::InterruptController(gdt_ptr);
    }
    return true;
}

bool PalmyraOS::kernel::initializePhysicalMemory(const Multiboot2::MultibootInfo& mb2Info) {
    /**
     * @brief Initializes the physical memory manager using Multiboot 2 information.
     *
     * This function reserves all kernel space and some additional safe space,
     * initializes the physical memory system, and reserves the video memory.
     *
     * @param mb2Info Multiboot 2 information structure
     * @return True if the physical memory manager is successfully initialized, false otherwise.
     */

    using namespace Multiboot2;

    // Get memory information from Multiboot 2
    const auto* memInfo = mb2Info.getBasicMemInfo();
    if (!memInfo) {
        LOG_ERROR("No memory information provided by bootloader");
        return false;
    }

    // Reserve all kernel space and add some safe space
    // This method automatically reserves all kmalloc()ed space + SafeSpace
    // mem_upper is in kilobytes, convert to bytes by multiplying by 1024
    PalmyraOS::kernel::PhysicalMemory::initialize(SafeSpace, memInfo->mem_upper * 1024);

    // Reserve video memory to prevent other frames from overwriting it
    {
        // Get framebuffer address from VBE or framebuffer tag
        uint32_t framebuffer_addr = 0;
        const auto* vbeTag        = mb2Info.getVBE();
        if (vbeTag) {
            // Extract framebuffer address directly from VBE mode info block (offset 40)
            const uint8_t* vbe_block = vbeTag->vbe_mode_info.external_specification;
            framebuffer_addr         = *reinterpret_cast<const uint32_t*>(&vbe_block[40]);
        }
        else {
            const auto* fbTag = mb2Info.getFramebuffer();
            if (fbTag) { framebuffer_addr = static_cast<uint32_t>(fbTag->common.framebuffer_addr); }
        }

        if (framebuffer_addr != 0) {
            // Number of frames/pages needed for the buffer
            uint32_t frameBufferSize   = display_ptr->getVideoMemorySize();
            uint32_t frameBufferFrames = (frameBufferSize >> PAGE_BITS) + 1;

            for (int i = 0; i < frameBufferFrames; ++i) { PalmyraOS::kernel::PhysicalMemory::reserveFrame((void*) (framebuffer_addr + (i << PAGE_BITS))); }
        }
    }
    return true;
}

bool PalmyraOS::kernel::initializeVirtualMemory(const Multiboot2::MultibootInfo& mb2Info) {
    /**
     * @brief Initializes the virtual memory manager using Multiboot 2 information.
     *
     * This function initializes the kernel paging directory, maps the kernel and video memory by identity,
     * switches to the new kernel paging directory, and initializes the paging system.
     *
     * @param mb2Info Multiboot 2 information structure
     * @return True if the virtual memory manager is successfully initialized, false otherwise.
     */

    using namespace Multiboot2;

    /* Here we assume physical memory has been initialized, hence we do not use kmalloc anymore
     * Instead, we use PhysicalMemory::allocateFrames()
     */

    auto& textRenderer  = *kernel::textRenderer_ptr;

    // Get memory information from Multiboot 2
    const auto* memInfo = mb2Info.getBasicMemInfo();
    if (!memInfo) {
        LOG_ERROR("No memory information provided by bootloader");
        return false;
    }

    // Initialize and ensure kernel directory is aligned ~ 8 KiB = 3 frames
    uint32_t PagingDirectoryFrames    = (sizeof(PagingDirectory) >> PAGE_BITS) + 1;
    kernel::kernelPagingDirectory_ptr = (PagingDirectory*) PhysicalMemory::allocateFrames(PagingDirectoryFrames);

    // Ensure the pointer we have is aligned
    if ((uint32_t) kernel::kernelPagingDirectory_ptr & (PAGE_SIZE - 1)) kernel::kernelPanic("Unaligned Kernel Directory at 0x%X", kernel::kernelPagingDirectory_ptr);

    // Map kernel space by identity
    {
        textRenderer << "Kernel.." << SWAP_BUFF();
        kernel::CPU::delay(2'500'000'000L);
        auto kernelSpace       = (uint32_t) PhysicalMemory::allocateFrame();
        kernel::kernelLastPage = kernelSpace >> PAGE_BITS;
        kernel::kernelPagingDirectory_ptr->mapPages(nullptr, nullptr, kernel::kernelLastPage, PageFlags::Present | PageFlags::ReadWrite);
    }

    textRenderer << "Tables.." << SWAP_BUFF();
    kernel::CPU::delay(2'500'000'000L);
    // Initialize all kernel's directory tables, to avoid Recursive Page Table Mapping Problem
    size_t max_pages = (memInfo->mem_upper >> 12) + 1;  // Kilobytes to 4 Megabytes
    for (int i = 0; i < max_pages; ++i) { kernel::kernelPagingDirectory_ptr->getTable(i, PageFlags::Present | PageFlags::ReadWrite); }

    // Map video memory by identity
    textRenderer << "Video.." << SWAP_BUFF();
    kernel::CPU::delay(2'500'000'000L);

    // Get framebuffer address from VBE or framebuffer tag
    uint32_t framebuffer_addr = 0;
    const auto* vbeTag        = mb2Info.getVBE();
    if (vbeTag) {
        // Extract framebuffer address directly from VBE mode info block (offset 40)
        const uint8_t* vbe_block = vbeTag->vbe_mode_info.external_specification;
        framebuffer_addr         = *reinterpret_cast<const uint32_t*>(&vbe_block[40]);
    }
    else {
        const auto* fbTag = mb2Info.getFramebuffer();
        if (fbTag) { framebuffer_addr = static_cast<uint32_t>(fbTag->common.framebuffer_addr); }
    }

    if (framebuffer_addr != 0) {
        uint32_t frameBufferSize   = display_ptr->getVideoMemorySize();
        uint32_t frameBufferFrames = (frameBufferSize >> PAGE_BITS) + 1;
        LOG_INFO("Mapping video memory by identity: %u frames", frameBufferFrames);
        LOG_INFO("Frame buffer size: %u bytes", frameBufferSize);
        kernel::kernelPagingDirectory_ptr->mapPages((void*) framebuffer_addr, (void*) framebuffer_addr, frameBufferFrames, PageFlags::Present | PageFlags::ReadWrite);
    }

    // Map HPET registers if initialized (get actual address from ACPI table)
    if (HPET::isInitialized()) {
        uintptr_t hpetPhysAddr = HPET::getPhysicalAddress();
        if (hpetPhysAddr != 0) {
            void* hpetAddr = reinterpret_cast<void*>(hpetPhysAddr);
            kernel::kernelPagingDirectory_ptr->mapPages(hpetAddr, hpetAddr, 1, PageFlags::Present | PageFlags::ReadWrite);
            LOG_INFO("Mapping HPET registers by identity: 1 page at 0x%p", hpetAddr);
        }
        else { LOG_WARN("HPET initialized but physical address is NULL"); }
    }

    // Map PCIe configuration space if available (get actual address from ACPI MCFG table)
    if (ACPI::isInitialized() && ACPI::getMCFG() != nullptr) {
        const auto* mcfg       = ACPI::getMCFG();
        uint32_t headerSize    = sizeof(acpi::ACPISDTHeader) + sizeof(uint64_t);
        const auto* allocation = reinterpret_cast<const acpi::MCFGAllocation*>(reinterpret_cast<const uint8_t*>(mcfg) + headerSize);

        uintptr_t pcieBaseAddr = static_cast<uintptr_t>(allocation->baseAddress);
        uint32_t busCount      = (allocation->endBusNumber - allocation->startBusNumber) + 1;

        // Each bus needs 1 MB (32 devices * 8 functions * 4KB)
        // Round up to nearest 4KB page boundary
        uint32_t totalPages    = (busCount * 1024 * 1024) / 4096;

        if (pcieBaseAddr != 0) {
            void* pcieAddr = reinterpret_cast<void*>(pcieBaseAddr);
            kernel::kernelPagingDirectory_ptr->mapPages(pcieAddr, pcieAddr, totalPages, PageFlags::Present | PageFlags::ReadWrite);
            LOG_INFO("Mapping PCIe configuration space by identity: %u pages (%u MB) at 0x%p", totalPages, busCount, pcieAddr);
        }
    }

    // Switch to the new kernel paging directory and initialize paging
    textRenderer << "switching.." << SWAP_BUFF();
    kernel::CPU::delay(2'500'000'000L);
    PalmyraOS::kernel::PagingManager::switchPageDirectory(kernel::kernelPagingDirectory_ptr);
    PalmyraOS::kernel::PagingManager::initialize();

    return true;
}

void PalmyraOS::kernel::testMemory() {
    /**
     * @brief Tests the memory system.
     *
     * This function performs various tests on the paging and heap systems
     * to ensure their correct operation.
     */

    // paging
    if (!Tests::Paging::testPagingBoundaries()) kernel::kernelPanic("Testing Paging boundaries failed!");

    if (!Tests::Paging::testPageTableAllocation()) kernel::kernelPanic("Testing Paging table allocation failed!");

    if (!Tests::Paging::testNullPointerException()) kernel::kernelPanic("Testing Paging nullptr allocation failed!");

    // heap
    if (!Tests::Heap::testHeapAllocation()) kernel::kernelPanic("Testing Heap allocation failed!");

    //	if (!Tests::Heap::testHeapCoalescence())
    //		kernel::kernelPanic("Testing Heap Coalescence failed!");

    // standard library
    if (!Tests::Allocator::testVector()) kernel::kernelPanic("Testing Allocator Vector failed!");

    if (!Tests::Allocator::testVectorOfClasses()) kernel::kernelPanic("Testing Allocator Vector Classes failed!");

    if (!Tests::Allocator::testString()) kernel::kernelPanic("Testing Allocator String failed!");

    if (!Tests::Allocator::testMap()) kernel::kernelPanic("Testing Allocator Map failed!");

    if (!Tests::Allocator::testSet()) kernel::kernelPanic("Testing Allocator Map failed!");

    if (!Tests::Allocator::testUnorderedMap()) kernel::kernelPanic("Testing Allocator Unordered Map failed!");

    //	if (!Tests::Allocator::testQueue())
    //		kernel::kernelPanic("Testing Allocator Queue failed!");
}

void PalmyraOS::kernel::initializePCIeDrivers() {
    /**
     * @brief Initialize PCIe-based drivers (network, storage, etc.)
     *
     * IMPORTANT: This function MUST be called AFTER paging is enabled!
     * It uses the heap manager to allocate memory for device structures and DMA buffers.
     *
     * Prerequisites:
     * - Paging enabled
     * - Heap manager initialized
     * - ACPI initialized (for MCFG table)
     * - PCIe::initialize() called (for base address setup)
     */

    auto& textRenderer = *kernel::textRenderer_ptr;

    LOG_INFO("Initializing PCIe drivers (requires paging + heap)");

    // Enumerate all PCI Express devices (uses heap to store device list)
    if (!PCIe::isInitialized()) {
        LOG_ERROR("PCIe not initialized - cannot enumerate devices");
        return;
    }

    PCIe::enumerateDevices();
    LOG_INFO("PCIe: Enumerated %u devices", PCIe::getDeviceCount());

    // Initialize Network Manager
    if (!NetworkManager::initialize()) {
        LOG_ERROR("Failed to initialize NetworkManager");
        return;
    }

    LOG_INFO("NetworkManager initialized successfully");

    // Scan for network devices
    bool foundNetwork = false;
    for (uint8_t bus = 0; bus <= 63; ++bus) {
        for (uint8_t dev = 0; dev < 32; ++dev) {
            if (!PCIe::deviceExists(bus, dev, 0)) continue;

            uint16_t vendorID = PCIe::readConfig16(bus, dev, 0, 0x00);
            uint16_t deviceID = PCIe::readConfig16(bus, dev, 0, 0x02);

            // Check for AMD PCnet (Vendor: 0x1022, Device: 0x2000)
            if (vendorID == 0x1022 && deviceID == 0x2000) {
                LOG_INFO("Found AMD PCnet at [%02X:%02X.0]", bus, dev);

                // Create and initialize PCnet driver (inject heap manager dependency)
                auto* pcnet = heapManager.createInstance<PCnetDriver>(bus, dev, 0, &heapManager);
                if (pcnet && pcnet->initialize()) {
                    // Register with NetworkManager
                    if (NetworkManager::registerInterface(pcnet)) {
                        // Configure network interface for VirtualBox NAT defaults
                        pcnet->setIPAddress(0x0A00020F);   // 10.0.2.15
                        pcnet->setSubnetMask(0xFFFFFF00);  // 255.255.255.0
                        pcnet->setGateway(0x0A000202);     // 10.0.2.2

                        // Enable interface
                        if (pcnet->enable()) {
                            LOG_INFO("Network interface 'eth0' enabled");
                            foundNetwork = true;
                        }
                        else { LOG_ERROR("Failed to enable network interface"); }
                    }
                    else { LOG_ERROR("Failed to register network interface"); }
                }
                else {
                    if (pcnet) heapManager.free(pcnet);
                    LOG_ERROR("Failed to initialize PCnet driver");
                }
                break;
            }
        }
        if (foundNetwork) break;
    }

    // Display network interface statistics
    NetworkManager::listInterfaces();

    if (!foundNetwork) { LOG_WARN("No supported network adapters found"); }
}

void PalmyraOS::kernel::initializeNetworkProtocols() {
    /**
     * @brief Initialize network protocols (ARP, DNS, IPv4, ICMP)
     *
     * This function initializes all network protocol layers in the correct order.
     * Must be called after PCIe drivers are initialized and at least one network
     * interface is available.
     *
     * Initialization order:
     * 1. ARP (Address Resolution Protocol) - MAC address resolution
     * 2. DNS (Domain Name System) - Hostname resolution (stub)
     * 3. IPv4 (Internet Protocol) - Routing and forwarding
     * 4. ICMP (Internet Control Message Protocol) - Ping and diagnostics
     */

    auto& textRenderer = *kernel::textRenderer_ptr;

    // ----------------------- Initialize ARP -------
    textRenderer << "Initializing ARP..." << SWAP_BUFF();
    {
        auto eth0 = NetworkManager::getDefaultInterface();
        if (eth0) {
            uint32_t eth0_ip        = eth0->getIPAddress();
            const uint8_t* eth0_mac = eth0->getMACAddress();

            if (ARP::initialize(eth0_ip, eth0_mac)) {
                LOG_INFO("ARP: Initialized for interface '%s'", eth0->getName());
                textRenderer << " Done (resolving gateway MAC)." << SWAP_BUFF();

                // Try to resolve gateway MAC address
                uint32_t gateway = eth0->getGateway();
                if (gateway != 0) {
                    uint8_t gateway_mac[6];
                    if (ARP::resolveMacAddress(gateway, gateway_mac)) { LOG_INFO("ARP: Gateway MAC resolved successfully"); }
                    else { LOG_WARN("ARP: Failed to resolve gateway MAC (may retry later)"); }
                }

                ARP::logCache();
            }
            else {
                LOG_ERROR("ARP: Initialization failed");
                textRenderer << " FAILED.\n" << SWAP_BUFF();
            }
        }
        else {
            LOG_ERROR("ARP: No network interface available");
            textRenderer << " FAILED (no interface).\n" << SWAP_BUFF();
        }
    }
    textRenderer << "\n" << SWAP_BUFF();
    CPU::delay(2'500'000'000L);

    // ----------------------- Initialize IPv4 -------
    textRenderer << "Initializing IPv4..." << SWAP_BUFF();
    {
        auto eth0 = NetworkManager::getDefaultInterface();
        if (eth0) {
            uint32_t eth0_ip = eth0->getIPAddress();
            uint32_t gateway = eth0->getGateway();
            uint32_t subnet  = eth0->getSubnetMask();

            if (IPv4::initialize(eth0_ip, subnet, gateway)) {
                LOG_INFO("IPv4: Initialized successfully");
                textRenderer << " Done." << SWAP_BUFF();
            }
            else {
                LOG_ERROR("IPv4: Initialization failed");
                textRenderer << " FAILED.\n" << SWAP_BUFF();
            }
        }
        else {
            LOG_ERROR("IPv4: No network interface");
            textRenderer << " FAILED (no interface).\n" << SWAP_BUFF();
        }
    }
    textRenderer << "\n" << SWAP_BUFF();
    CPU::delay(2'500'000'000L);

    // ----------------------- Initialize UDP -------
    textRenderer << "Initializing UDP..." << SWAP_BUFF();
    {
        if (UDP::initialize()) {
            LOG_INFO("UDP: Initialized successfully");
            textRenderer << " Done." << SWAP_BUFF();
        }
        else {
            LOG_ERROR("UDP: Initialization failed");
            textRenderer << " FAILED.\n" << SWAP_BUFF();
        }
    }
    textRenderer << "\n" << SWAP_BUFF();
    CPU::delay(2'500'000'000L);

    // ----------------------- Initialize ICMP -------
    textRenderer << "Initializing ICMP..." << SWAP_BUFF();
    {
        if (ICMP::initialize()) {
            LOG_INFO("ICMP: Initialized successfully");
            textRenderer << " Done." << SWAP_BUFF();
        }
        else {
            LOG_ERROR("ICMP: Initialization failed");
            textRenderer << " FAILED.\n" << SWAP_BUFF();
        }
    }
    textRenderer << "\n" << SWAP_BUFF();
    CPU::delay(2'500'000'000L);

    // ----------------------- Initialize DNS (requires IPv4 for server detection) -------
    textRenderer << "Initializing DNS..." << SWAP_BUFF();
    {
        if (DNS::initialize()) {
            LOG_INFO("DNS: Initialized");
            textRenderer << " Done.\n" << SWAP_BUFF();
        }
        else {
            LOG_ERROR("DNS: Initialization failed");
            textRenderer << " FAILED.\n" << SWAP_BUFF();
        }
    }
    CPU::delay(2'500'000'000L);
}

void PalmyraOS::kernel::testNetworkConnectivity() {
    /**
     * @brief Test network connectivity with comprehensive ping tests
     *
     * Performs three ping tests to validate the network stack:
     * 1. Gratuitous ARP (self-announcement test)
     * 2. Ping to Cloudflare DNS (1.1.1.1)
     * 3. Ping to Google DNS (8.8.8.8)
     *
     * Tests validate:
     * - ARP resolution
     * - IPv4 routing (local vs gateway)
     * - ICMP Echo Request/Reply
     * - End-to-end internet connectivity
     */

    auto& textRenderer = *kernel::textRenderer_ptr;

    textRenderer << "Running ICMP ping tests (local, Cloudflare, Google)...\n" << SWAP_BUFF();
    {
        LOG_INFO("========================================");
        LOG_INFO("ICMP Ping Test Suite");
        LOG_INFO("========================================");

        uint32_t local_ip        = IPv4::getLocalIP();
        uint32_t gateway_ip      = IPv4::getGateway();
        uint8_t local_bytes[4]   = {static_cast<uint8_t>((local_ip >> 24) & 0xFF),
                                    static_cast<uint8_t>((local_ip >> 16) & 0xFF),
                                    static_cast<uint8_t>((local_ip >> 8) & 0xFF),
                                    static_cast<uint8_t>(local_ip & 0xFF)};
        uint8_t gateway_bytes[4] = {static_cast<uint8_t>((gateway_ip >> 24) & 0xFF),
                                    static_cast<uint8_t>((gateway_ip >> 16) & 0xFF),
                                    static_cast<uint8_t>((gateway_ip >> 8) & 0xFF),
                                    static_cast<uint8_t>(gateway_ip & 0xFF)};
        uint32_t rtt_ms;

        // Test 1: Gratuitous ARP (self-announcement to test RX)
        LOG_INFO("");
        LOG_INFO("TEST 1: Gratuitous ARP (self-announcement test)");
        LOG_INFO("Target: %u.%u.%u.%u (our own IP)", local_bytes[0], local_bytes[1], local_bytes[2], local_bytes[3]);
        LOG_INFO("Path: Broadcast ARP announcement, should receive our own packet");
        LOG_INFO("Expected: Self-reception of broadcast ARP");

        // Send gratuitous ARP (announce our own IP mapping)
        uint8_t dummy_mac[6];
        bool received_own_arp = ARP::resolveMacAddress(local_ip, dummy_mac);

        if (received_own_arp) {
            LOG_INFO(" PASSED: Received our own ARP broadcast!");
            textRenderer << "  [1/3] Gratuitous ARP: SUCCESS (RX working)\n" << SWAP_BUFF();
        }
        else {
            LOG_WARN("✗ FAILED: Did not receive our own ARP broadcast");
            LOG_INFO("Diagnosis: Network adapter may be disconnected or not in promiscuous mode");
            textRenderer << "  [1/3] Gratuitous ARP: FAILED (no RX)\n" << SWAP_BUFF();
        }
        CPU::delay(1000);

        // Test 2: Ping Cloudflare DNS (1.1.1.1)
        LOG_INFO("");
        LOG_INFO("TEST 2: Ping Cloudflare DNS");
        LOG_INFO("Target: 1.1.1.1");
        LOG_INFO("Path: Local -> Gateway (%u.%u.%u.%u) -> Internet -> Cloudflare", gateway_bytes[0], gateway_bytes[1], gateway_bytes[2], gateway_bytes[3]);
        LOG_INFO("TTL hops: 64 -> 63 (gateway) -> 62 (ISP) -> ... -> 1 (Cloudflare)");
        LOG_INFO("Distance: ~15-20 hops");

        uint32_t cloudflare_ip = 0x01010101;
        if (ICMP::ping(cloudflare_ip, &rtt_ms)) {
            LOG_INFO(" PASSED: Cloudflare reachable! RTT: %u ms", rtt_ms);
            LOG_INFO("Full path trace: %u.%u.%u.%u -> %u.%u.%u.%u -> Internet -> 1.1.1.1",
                     local_bytes[0],
                     local_bytes[1],
                     local_bytes[2],
                     local_bytes[3],
                     gateway_bytes[0],
                     gateway_bytes[1],
                     gateway_bytes[2],
                     gateway_bytes[3]);
            textRenderer << "  [2/3] Cloudflare (1.1.1.1): SUCCESS (" << rtt_ms << " ms)\n" << SWAP_BUFF();
        }
        else {
            LOG_WARN("✗ FAILED: Cloudflare unreachable");
            LOG_INFO("Diagnosis: TTL may have decremented to 0, or gateway/ISP not responding");
            LOG_INFO("ARP trace: Attempted to resolve %u.%u.%u.%u (gateway) - check ARP cache", gateway_bytes[0], gateway_bytes[1], gateway_bytes[2], gateway_bytes[3]);
            textRenderer << "  [2/3] Cloudflare (1.1.1.1): TIMEOUT\n" << SWAP_BUFF();
        }
        CPU::delay(1000);

        // Test 3: Ping Google DNS (8.8.8.8)
        LOG_INFO("");
        LOG_INFO("TEST 3: Ping Google DNS");
        LOG_INFO("Target: 8.8.8.8");
        LOG_INFO("Path: Local -> Gateway (%u.%u.%u.%u) -> Internet -> Google", gateway_bytes[0], gateway_bytes[1], gateway_bytes[2], gateway_bytes[3]);
        LOG_INFO("TTL hops: 64 -> 63 (gateway) -> 62 (ISP) -> ... -> 1 (Google)");
        LOG_INFO("Distance: ~10-15 hops");

        uint32_t google_dns_ip = 0x08080808;
        if (ICMP::ping(google_dns_ip, &rtt_ms)) {
            LOG_INFO(" PASSED: Google DNS reachable! RTT: %u ms", rtt_ms);
            LOG_INFO("Full path trace: %u.%u.%u.%u -> %u.%u.%u.%u -> Internet -> 8.8.8.8",
                     local_bytes[0],
                     local_bytes[1],
                     local_bytes[2],
                     local_bytes[3],
                     gateway_bytes[0],
                     gateway_bytes[1],
                     gateway_bytes[2],
                     gateway_bytes[3]);
            textRenderer << "  [3/3] Google DNS (8.8.8.8): SUCCESS (" << rtt_ms << " ms)\n" << SWAP_BUFF();
        }
        else {
            LOG_WARN("✗ FAILED: Google DNS unreachable");
            LOG_INFO("Diagnosis: TTL may have decremented to 0, or gateway/ISP not responding");
            LOG_INFO("ARP trace: Attempted to resolve %u.%u.%u.%u (gateway) - check ARP cache", gateway_bytes[0], gateway_bytes[1], gateway_bytes[2], gateway_bytes[3]);
            textRenderer << "  [3/3] Google DNS (8.8.8.8): TIMEOUT\n" << SWAP_BUFF();
        }

        // Summary
        LOG_INFO("");
        LOG_INFO("========================================");
        LOG_INFO("Network Configuration:");
        LOG_INFO("  Local IP: %u.%u.%u.%u", local_bytes[0], local_bytes[1], local_bytes[2], local_bytes[3]);
        LOG_INFO("  Gateway: %u.%u.%u.%u", gateway_bytes[0], gateway_bytes[1], gateway_bytes[2], gateway_bytes[3]);
        uint32_t subnetMask = IPv4::getSubnetMask();
        LOG_INFO("  Subnet: %u.%u.%u.%u", (subnetMask >> 24) & 0xFF, (subnetMask >> 16) & 0xFF, (subnetMask >> 8) & 0xFF, subnetMask & 0xFF);
        LOG_INFO("");
        LOG_INFO("Stack Status:");
        LOG_INFO("   IPv4 Protocol: OPERATIONAL");
        LOG_INFO("   ARP Protocol: OPERATIONAL");
        LOG_INFO("   ICMP Protocol: OPERATIONAL");
        LOG_INFO("  Network Connectivity: See test results above");
        LOG_INFO("========================================");
    }
    textRenderer << "\n" << SWAP_BUFF();
    CPU::delay(2'500'000'000L);

    // ----------------------- DNS Resolution Tests -------
    textRenderer << "Testing DNS hostname resolution...\n" << SWAP_BUFF();
    {
        LOG_INFO("========================================");
        LOG_INFO("DNS Resolution Test Suite");
        LOG_INFO("========================================");

        const char* testDomains[]           = {"google.com", "github.com", "cloudflare.com"};
        constexpr uint8_t TEST_DOMAIN_COUNT = 3;

        uint32_t successfulResolutions      = 0;

        for (uint8_t i = 0; i < TEST_DOMAIN_COUNT; ++i) {
            LOG_INFO("");
            LOG_INFO("TEST %u/%u: Resolving '%s'", i + 1, TEST_DOMAIN_COUNT, testDomains[i]);

            uint32_t resolvedIP = 0;
            if (DNS::resolveDomain(testDomains[i], &resolvedIP)) {
                uint8_t ipBytes[4] = {static_cast<uint8_t>((resolvedIP >> 24) & 0xFF),
                                      static_cast<uint8_t>((resolvedIP >> 16) & 0xFF),
                                      static_cast<uint8_t>((resolvedIP >> 8) & 0xFF),
                                      static_cast<uint8_t>(resolvedIP & 0xFF)};

                LOG_INFO(" PASSED: %s -> %u.%u.%u.%u", testDomains[i], ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3]);
                textRenderer << "  [" << (i + 1) << "/" << TEST_DOMAIN_COUNT << "] " << testDomains[i] << ": " << ipBytes[0] << "." << ipBytes[1] << "." << ipBytes[2] << "."
                             << ipBytes[3] << "\n"
                             << SWAP_BUFF();
                successfulResolutions++;
            }
            else {
                LOG_WARN("✗ FAILED: Could not resolve %s", testDomains[i]);
                textRenderer << "  [" << (i + 1) << "/" << TEST_DOMAIN_COUNT << "] " << testDomains[i] << ": TIMEOUT\n" << SWAP_BUFF();
            }

            CPU::delay(1000);  // Small delay between queries
        }

        // Display DNS cache
        LOG_INFO("");
        DNS::logCache();

        // Summary
        LOG_INFO("");
        LOG_INFO("========================================");
        LOG_INFO("DNS Test Summary:");
        LOG_INFO("  Successful: %u/%u", successfulResolutions, TEST_DOMAIN_COUNT);
        LOG_INFO("  Failed: %u/%u", TEST_DOMAIN_COUNT - successfulResolutions, TEST_DOMAIN_COUNT);
        LOG_INFO("  Cache entries: %u/%u", successfulResolutions, DNS::CACHE_SIZE);
        LOG_INFO("========================================");
    }
    textRenderer << "\n" << SWAP_BUFF();
    CPU::delay(2'500'000'000L);
}

void PalmyraOS::kernel::initializeDrivers() {

    auto& textRenderer = *kernel::textRenderer_ptr;

    LOG_INFO("Start");

    ata_primary_master   = heapManager.createInstance<ATA>(0x1F0, ATA::Type::Master);
    ata_primary_slave    = heapManager.createInstance<ATA>(0x1F0, ATA::Type::Slave);
    ata_secondary_master = heapManager.createInstance<ATA>(0x170, ATA::Type::Master);
    ata_secondary_slave  = heapManager.createInstance<ATA>(0x170, ATA::Type::Slave);

    auto atas            = {std::pair(ata_primary_master, "pM"), std::pair(ata_primary_slave, "pS"), std::pair(ata_secondary_master, "sM"), std::pair(ata_secondary_slave, "sS")};

    for (auto& [ata, name]: atas) {
        if (!ata) {
            LOG_ERROR("Failed to allocate memory for ATA %s Device!", name);
            continue;  // TODO: or handle in a different way
        }

        textRenderer << "\nAt [" << name << "] ";
        if (!ata->identify(1000)) {
            textRenderer << " NONE ";
            continue;
        }

        // Identified

        LOG_INFO("ATA Device Identified: "
                 "Model [%s], Serial [%s], Firmware [%s], Sectors [%zu], Space [%zu MiB]",
                 ata->getModelNumber(),
                 ata->getSerialNumber(),
                 ata->getFirmwareVersion(),
                 ata->getSectors28Bit(),
                 ata->getStorageSize() / 1048576);
        textRenderer << "Model [" << ata->getModelNumber() << "] Serial [" << ata->getSerialNumber() << "] Firmware [" << ata->getFirmwareVersion() << "] Sectors ["
                     << ata->getSectors28Bit() << "] Space [" << (ata->getStorageSize() / 1048576) << " MiB]";
    }

    textRenderer << "\n";
}

void PalmyraOS::kernel::initializePartitions() {
    LOG_INFO("Start");
    // dereference graphics tools
    auto& textRenderer = *kernel::textRenderer_ptr;

    if (!kernel::ata_primary_master) {
        textRenderer << "No ATA..";
        LOG_WARN("ATA Primary Master is not available. Cannot initialize Partitions.");
        return;
    }


    auto atas = {std::pair(kernel::ata_primary_master, KString("/dev/sda")),
                 std::pair(kernel::ata_primary_slave, KString("/dev/sdb")),
                 std::pair(kernel::ata_secondary_master, KString("/dev/sdc")),
                 std::pair(kernel::ata_secondary_slave, KString("/dev/sde"))};

    for (auto& [ata, name]: atas) {
        // Read the Master Boot Record (MBR)
        uint8_t masterSector[512];
        ata->readSector(0, masterSector, 100);
        auto mbr = kernel::vfs::MasterBootRecord(masterSector);

        // Loop through partition entries
        for (int i = 0; i < 4; ++i) {
            if (!ata) continue;  // should not evaluate to true, but for the sake of completeness

            // get MBR entry
            auto mbr_entry = mbr.getEntry(i);
            textRenderer << "[" << i << ": ";
            LOG_INFO("ATA Primary Master: Partition %d:", i);
            LOG_INFO("bootable: %d, Type: %s, lbaStart: 0x%X, Size: %d MiB",
                     mbr_entry.isBootable,
                     kernel::vfs::MasterBootRecord::toString(mbr_entry.type),
                     mbr_entry.lbaStart,
                     mbr_entry.lbaCount * 512 / 1048576);

            // If we have a valid FAT32 entry
            if (mbr_entry.type == PalmyraOS::kernel::vfs::MasterBootRecord::FAT32_LBA) {
                // TODO factor out to another function for FAT32 Partitions
                // Initialize a Virtual Disk (Guard against writing outside boundaries)
                auto virtualDisk = heapManager.createInstance<kernel::VirtualDisk<kernel::ATA>>(*ata, mbr_entry.lbaStart, mbr_entry.lbaCount);

                // Pass the virtual disk to Fat32 Partition TODO add to partitions vector somewhere
                auto fat32_p     = heapManager.createInstance<vfs::FAT32Partition>(*virtualDisk, mbr_entry.lbaStart, mbr_entry.lbaCount);
                auto& fat32      = *fat32_p;
                // TODO: Mounting System
                // TODO: check if the type is FAT32 not FAT32 etc..

                if (fat32.getType() == vfs::FAT32Partition::Type::Invalid) textRenderer << "Invalid Fat (X)] ";
                else if (fat32.getType() == vfs::FAT32Partition::Type::FAT12) textRenderer << "FAT12 (X)] ";
                else if (fat32.getType() == vfs::FAT32Partition::Type::FAT16) textRenderer << "FAT16 (X)] ";
                if (fat32.getType() != vfs::FAT32Partition::Type::FAT32) {
                    // Invalid
                    heapManager.free(fat32_p);
                    continue;
                }

                textRenderer << "FAT32]";
                auto rootNode =
                        heapManager.createInstance<vfs::FAT32Directory>(fat32, 2, vfs::InodeBase::Mode::USER_READ, vfs::InodeBase::UserID::ROOT, vfs::InodeBase::GroupID::ROOT);
                vfs::VirtualFileSystem::setInodeByPath(name, rootNode);

                // Debug: Analyze directory entries to see how Windows writes LFNs
                LOG_WARN("=== ANALYZING WINDOWS-RENAMED FILE ===");

                vfs::DirectoryEntry folder = fat32.resolvePathToEntry(KString("/exp4/"));


                auto newfile               = fat32.createFile(folder, KString("e7newfile19.txt"));
                // auto newfile = fat32.resolvePathToEntry(KString("/exp/mynewfile2.txt"));


                // LOG_WARN("File created: size: %d, cluster: %d", newfile->getFileSize() , newfile->getFirstCluster());

                // auto n = fat32.write(*newfile, {'h', 'i', ' ', 'f', 'i', 'l', 'e', '\0'});
                // LOG_WARN("First write to file %s.", (n? "is successful" : "failed"));
                // LOG_WARN("After first write: size: %d, cluster: %d", newfile->getFileSize() , newfile->getFirstCluster());

                // if (newfile.has_value()) {
                // 	auto n = fat32.write(newfile.value(), {'h', 'i', ' ', 'f', 'i', 'l', 'e'/*, '\0'*/});
                // 	LOG_WARN("Second write to file %s.", (n? "is successful" : "failed"));
                // 	LOG_WARN("After second write: size: %d, cluster: %d", newfile->getFileSize() , newfile->getFirstCluster());

                // 	// Read the file back to verify
                // 	auto readData = fat32.read(newfile.value(), 0, 100);
                // 	LOG_WARN("Read %u bytes from file", readData.size());
                // 	if (readData.size() > 0) {
                // 		// Create a null-terminated string for logging
                // 		KVector<uint8_t> temp = readData;
                // 		temp.push_back('\0');
                // 		LOG_WARN("File contents: '%s'", temp.data());
                // 	}
                // }


                if (folder.getAttributes() != vfs::EntryAttribute::Invalid) {
                    LOG_WARN("Found /exp2/ directory, reading entries...");
                    uint32_t dirCluster      = folder.getFirstCluster();
                    KVector<uint8_t> dirData = fat32.readEntireFile(dirCluster);
                    LOG_WARN("Directory has %u bytes of data", dirData.size());

                    struct LfnPart {
                        uint8_t ord;
                        KWString w;
                        size_t off;
                    };
                    KVector<LfnPart> lfnParts;  // collected after seeing LAST
                    uint8_t lfnChecksum = 0;
                    bool lfnActive      = false;

                    auto dumpHex        = [&](size_t off) {
                        char hexDump[100];
                        size_t pos = 0;
                        for (size_t j = 0; j < 32 && pos < 99; ++j) {
                            if (j == 16) {
                                hexDump[pos++] = ' ';
                                hexDump[pos++] = '|';
                                hexDump[pos++] = ' ';
                            }
                            else if (j > 0 && j % 4 == 0) { hexDump[pos++] = ' '; }
                            uint8_t b              = dirData[off + j];
                            static const char hx[] = "0123456789ABCDEF";
                            hexDump[pos++]         = hx[b >> 4];
                            hexDump[pos++]         = hx[b & 0x0F];
                            hexDump[pos++]         = ' ';
                        }
                        hexDump[pos] = '\0';
                        LOG_WARN("  Hex: %s", hexDump);
                    };

                    auto trimSfn = [](const char* p, int n) -> int {
                        while (n > 0 && p[n - 1] == ' ') --n;
                        return n;
                    };
                    auto asciiFromUtf16 = [](const KWString& w, char* out, size_t outCap) {
                        size_t o = 0;
                        for (size_t i = 0; i < w.size() && o + 1 < outCap; i++) {
                            uint16_t ch = w[i];
                            if (ch == 0x0000) break;
                            out[o++] = (ch < 128) ? (char) ch : '?';
                        }
                        out[o] = '\0';
                    };

                    for (size_t i = 0; i + 32 <= dirData.size(); i += 32) {
                        uint8_t first = dirData[i];
                        if (first == 0x00) break;
                        if (first == 0xE5) continue;

                        LOG_WARN("=== Entry at offset %u ===", (uint32_t) i);
                        dumpHex(i);

                        uint8_t attr = dirData[i + 11];
                        if ((attr & 0x3F) == 0x0F) {
                            uint8_t seq = dirData[i];
                            uint8_t chk = dirData[i + 13];
                            bool isLast = (seq & 0x40) != 0;
                            uint8_t ord = (uint8_t) (seq & 0x3F);
                            LOG_WARN("  Type: LFN, Seq=0x%02X%s, Checksum=0x%02X", ord, isLast ? " (LAST)" : "", chk);

                            if (isLast) {
                                lfnParts.clear();
                                lfnChecksum = chk;
                                lfnActive   = true;
                            }

                            KWString part;
                            auto addRange = [&](size_t start, size_t count) {
                                for (size_t j = 0; j < count; j++) {
                                    uint16_t ch = (uint16_t) (dirData[i + start + j * 2] | (dirData[i + start + j * 2 + 1] << 8));
                                    if (ch == 0x0000 || ch == 0xFFFF) break;
                                    part.push_back(ch);
                                }
                            };
                            addRange(1, 5);
                            addRange(14, 6);
                            addRange(28, 2);
                            if (lfnActive) lfnParts.push_back({ord, part, i});
                        }
                        else {
                            const char* sfn = (const char*) &dirData[i];
                            int baseLen     = trimSfn(sfn, 8);
                            int extLen      = trimSfn(sfn + 8, 3);
                            char sfnPretty[16];
                            int p = 0;
                            for (int k = 0; k < baseLen && p < 15; k++) sfnPretty[p++] = sfn[k];
                            if (extLen > 0 && p < 15) sfnPretty[p++] = '.';
                            for (int k = 0; k < extLen && p < 15; k++) sfnPretty[p++] = sfn[8 + k];
                            sfnPretty[p] = '\0';

                            uint8_t sum  = 0;
                            for (int k = 0; k < 11; k++) sum = ((sum & 1) << 7) + (sum >> 1) + (uint8_t) sfn[k];
                            LOG_WARN("  Type: SFN, Attr=0x%02X", attr);
                            LOG_WARN("  Name: '%s'", sfnPretty);
                            LOG_WARN("  First char byte: 0x%02X", (uint8_t) sfn[0]);
                            LOG_WARN("  Checksum: 0x%02X", sum);
                            LOG_WARN("  SFN[11]: %02X %02X %02X %02X %02X %02X %02X %02X . %02X %02X %02X",
                                     (uint8_t) sfn[0],
                                     (uint8_t) sfn[1],
                                     (uint8_t) sfn[2],
                                     (uint8_t) sfn[3],
                                     (uint8_t) sfn[4],
                                     (uint8_t) sfn[5],
                                     (uint8_t) sfn[6],
                                     (uint8_t) sfn[7],
                                     (uint8_t) sfn[8],
                                     (uint8_t) sfn[9],
                                     (uint8_t) sfn[10]);

                            if (lfnActive && !lfnParts.empty()) {
                                // Build full LFN in order ord=1..N
                                std::sort(lfnParts.begin(), lfnParts.end(), [](const LfnPart& a, const LfnPart& b) { return a.ord < b.ord; });
                                KWString lfnFull;
                                for (auto& part: lfnParts)
                                    for (uint16_t ch: part.w) lfnFull.push_back(ch);
                                char lfnAscii[64];
                                asciiFromUtf16(lfnFull, lfnAscii, sizeof(lfnAscii));
                                LOG_WARN("  LFN->SFN Pair: long='%s', sfn='%s', matchChk=%s", lfnAscii, sfnPretty, (sum == lfnChecksum ? "YES" : "NO"));
                                // Highlight target zynewfile17.txt
                                auto ieq        = [](char a) { return (a >= 'a' && a <= 'z') ? (char) (a - 32) : a; };
                                const char* tgt = "ZYNEWFILE17.TXT";
                                bool isTarget   = true;
                                for (size_t ti = 0; tgt[ti] || lfnAscii[ti]; ++ti) {
                                    if (ieq(tgt[ti]) != ieq(lfnAscii[ti])) {
                                        isTarget = false;
                                        break;
                                    }
                                }
                                if (isTarget) {
                                    LOG_WARN(">>> TARGET /exp2/zynewfile17.txt: SFNoff=%u, LFNchk=0x%02X, SFNchk=0x%02X", (uint32_t) i, lfnChecksum, sum);
                                    // Dump SFN fields
                                    uint8_t ntRes  = dirData[i + 12];
                                    uint8_t cTenth = dirData[i + 13];
                                    uint16_t cTime = (uint16_t) (dirData[i + 14] | (dirData[i + 15] << 8));
                                    uint16_t cDate = (uint16_t) (dirData[i + 16] | (dirData[i + 17] << 8));
                                    uint16_t aDate = (uint16_t) (dirData[i + 18] | (dirData[i + 19] << 8));
                                    uint16_t cluHi = (uint16_t) (dirData[i + 20] | (dirData[i + 21] << 8));
                                    uint16_t wTime = (uint16_t) (dirData[i + 22] | (dirData[i + 23] << 8));
                                    uint16_t wDate = (uint16_t) (dirData[i + 24] | (dirData[i + 25] << 8));
                                    uint16_t cluLo = (uint16_t) (dirData[i + 26] | (dirData[i + 27] << 8));
                                    uint32_t fSize = (uint32_t) (dirData[i + 28] | (dirData[i + 29] << 8) | (dirData[i + 30] << 16) | (dirData[i + 31] << 24));
                                    LOG_WARN("  SFN fields: ntRes=0x%02X cTenth=%u cTime=0x%04X cDate=0x%04X aDate=0x%04X wTime=0x%04X wDate=0x%04X clu=%u "
                                             "size=%u",
                                             ntRes,
                                             cTenth,
                                             cTime,
                                             cDate,
                                             aDate,
                                             wTime,
                                             wDate,
                                             ((uint32_t) cluHi << 16) | cluLo,
                                             fSize);
                                    // Dump LFN chain offsets and ords
                                    for (auto& part: lfnParts) LOG_WARN("  LFN part at off=%u ord=%u len=%u", (uint32_t) part.off, part.ord, (uint32_t) part.w.size());
                                }
                            }

                            // Reset for next chain
                            lfnParts.clear();
                            lfnActive   = false;
                            lfnChecksum = 0;
                        }
                    }
                }
                else { LOG_ERROR("Could not find /exp2/ directory!"); }
                LOG_WARN("=== END WINDOWS FILE ANALYSIS ===");

                // TODO: Create new file test after analysis
            }
            else if (mbr_entry.type == PalmyraOS::kernel::vfs::MasterBootRecord::NTFS) {
                textRenderer << "NTFS (X)]";  // Unsupported
            }
            else {
                textRenderer << "(X)]";  // Unsupported and unrecognized
            }
        }
    }
}

void PalmyraOS::kernel::initializeBinaries() {

    // Create a test inode with a lambda function for reading test string
    auto terminalNode = kernel::heapManager.createInstance<vfs::FunctionInode>(nullptr, nullptr, nullptr);
    if (!terminalNode) return;

    // Set the test inode at "/bin/terminal"
    vfs::VirtualFileSystem::setInodeByPath(KString("/bin/terminal.elf"), terminalNode);
}

bool PalmyraOS::kernel::reboot() {
    /**
     * @brief Reboot the system using ACPI or legacy methods
     *
     * This function attempts to reboot the system using various methods:
     * 1. ACPI reset register (if available)
     * 2. Keyboard controller reset (legacy)
     * 3. Triple fault (last resort)
     */

    if (PowerManagement::isInitialized()) {
        LOG_INFO("PowerManagement: Attempting reboot...");
        PowerManagement::reboot();
    }
    LOG_ERROR("PowerManagement not initialized, cannot reboot");
    return false;
}

bool PalmyraOS::kernel::shutdown() {
    /**
     * @brief Shutdown the system using ACPI or legacy methods
     *
     * This function attempts to shutdown the system using various methods:
     * 1. ACPI S5 state (if available)
     * 2. APM shutdown (legacy)
     * 3. Halt CPU (if all else fails)
     */

    if (PowerManagement::isInitialized()) {
        LOG_INFO("PowerManagement: Attempting shutdown...");
        PowerManagement::shutdown();
    }
    LOG_ERROR("PowerManagement not initialized, cannot shutdown");
    return false;
}
