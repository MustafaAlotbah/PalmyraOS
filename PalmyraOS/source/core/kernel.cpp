#include "core/kernel.h"
#include "core/VBE.h"
#include "core/memory/paging.h"
#include "core/panic.h"
#include "tests/pagingTests.h"
#include "tests/allocatorTests.h"
#include "core/peripherals/Logger.h"
#include "core/files/partitions/MasterBootRecord.h"
#include "core/files/partitions/VirtualDisk.h"
#include "core/files/partitions/Fat32.h"
#include "core/files/Fat32FileSystem.h"
#include "core/files/VirtualFileSystem.h"
#include "core/tasks/elf.h"
#include "core/tasks/ProcessManager.h"
#include <new>

// Globals


namespace PalmyraOS::kernel
{

  // Graphics
  PalmyraOS::kernel::VBE         * vbe_ptr          = nullptr;
  PalmyraOS::kernel::Brush       * brush_ptr        = nullptr;
  PalmyraOS::kernel::TextRenderer* textRenderer_ptr = nullptr;

  // CPU
  PalmyraOS::kernel::GDT::GlobalDescriptorTable     * gdt_ptr = nullptr;
  PalmyraOS::kernel::interrupts::InterruptController* idt_ptr = nullptr;;

  // Memory
  PalmyraOS::kernel::PagingDirectory* kernelPagingDirectory_ptr = nullptr;
  PalmyraOS::kernel::HeapManager heapManager;
  uint32_t                       kernelLastPage = 0;

  // Drivers
  PalmyraOS::kernel::ATA* ata_primary_master = nullptr;
  PalmyraOS::kernel::ATA* ata_primary_slave  = nullptr;


}

bool PalmyraOS::kernel::initializeGraphics(vbe_mode_info_t* vbe_mode_info, vbe_control_info_t* vbe_control_info)
{
	/**
	 * @brief Initializes the graphics subsystem.
	 *
	 * This function allocates memory and initializes various components
	 * necessary for the graphics subsystem, including the VBE object,
	 * the font manager, the brush, and the text renderer.
	 *
	 * @param vbe_mode_info Pointer to the VBE mode information structure.
	 * @param vbe_control_info Pointer to the VBE control information structure.
	 * @return True if the graphics subsystem is successfully initialized, false otherwise.
	 */

	// Calculate the size of the VBE buffer
	uint32_t VBE_buffer_size = vbe_mode_info->width * vbe_mode_info->height * sizeof(uint32_t);

	// initialize VBE and framebuffer
	{
		// Allocate memory for the VBE object
		kernel::vbe_ptr = (VBE*)kernel::kmalloc(sizeof(VBE));
		if (kernel::vbe_ptr == nullptr) return false;

		// Construct the VBE object in the allocated memory
		new(kernel::vbe_ptr) VBE(
			vbe_mode_info,
			vbe_control_info,
			(uint32_t*)kernel::kmalloc(VBE_buffer_size)
		);
	}

	// Initialize the font manager
	fonts::FontManager::initialize();

	// Initialize kernel's brush
	{
		// Allocate memory for the brush object
		kernel::brush_ptr = (Brush*)kernel::kmalloc(sizeof(Brush));
		if (kernel::brush_ptr == nullptr) return false;

		// Construct the brush object in the allocated memory
		new(kernel::brush_ptr) Brush(kernel::vbe_ptr->getFrameBuffer());
	}

	// Initialize kernel's text renderer
	{
		// Allocate memory for the text renderer object
		kernel::textRenderer_ptr = (TextRenderer*)kernel::kmalloc(sizeof(TextRenderer));
		if (kernel::textRenderer_ptr == nullptr) return false;

		// Construct the text renderer object in the allocated memory
		new(kernel::textRenderer_ptr) TextRenderer(
			kernel::vbe_ptr->getFrameBuffer(),
			fonts::FontManager::getFont("Arial-12")
		);
	}

	// Everything is initialized successfully
	return true;
}

void PalmyraOS::kernel::clearScreen(bool drawLogo)
{
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

	if (drawLogo)
	{
		// Draw the logo text
		textRenderer << Color::Orange << "Palmyra" << Color::LightBlue << "OS ";
		textRenderer << Color::White << "v0.01\n";

		// Draw a horizontal line below the logo text
		brush_ptr->drawHLine(1, 150, textRenderer.getCursorY() + 2, Color::White);
	}

	// Swap the buffers to update the display
	vbe_ptr->swapBuffers();
}

bool PalmyraOS::kernel::initializeGlobalDescriptorTable()
{
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
		kernel::gdt_ptr = (GDT::GlobalDescriptorTable*)kernel::kmalloc(sizeof(GDT::GlobalDescriptorTable));
		if (kernel::gdt_ptr == nullptr) return false;

		// Construct the GDT object in the allocated memory
		new(kernel::gdt_ptr) GDT::GlobalDescriptorTable(kernel::InitialKernelStackPointer);
	}
	return true;
}

bool PalmyraOS::kernel::initializeInterrupts()
{
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
		kernel::idt_ptr = (interrupts::InterruptController*)kernel::kmalloc(sizeof(interrupts::InterruptController));
		if (kernel::idt_ptr == nullptr) return false;

		// Construct the Interrupt Controller object in the allocated memory
		new(kernel::idt_ptr) interrupts::InterruptController(gdt_ptr);
	}
	return true;
}

bool PalmyraOS::kernel::initializePhysicalMemory(multiboot_info_t* x86_multiboot_info)
{
	/**
	 * @brief Initializes the physical memory manager.
	 *
	 * This function reserves all kernel space and some additional safe space,
	 * initializes the physical memory system, and reserves the video memory.
	 *
	 * @param x86_multiboot_info Pointer to the multiboot information structure.
	 * @return True if the physical memory manager is successfully initialized, false otherwise.
	 */

	// Reserve all kernel space and add some safe space
	// This method automatically reserves all kmalloc()ed space + SafeSpace
	PalmyraOS::kernel::PhysicalMemory::initialize(SafeSpace, x86_multiboot_info->mem_upper * 1024);

	// Reserve video memory to prevent other frames from overwriting it
	{
		// Number of frames/pages needed for the buffer
		auto* vbe_mode_info = (vbe_mode_info_t*)(uintptr_t)x86_multiboot_info->vbe_mode_info;
		uint32_t frameBufferSize   = vbe_ptr->getVideoMemorySize();
		uint32_t frameBufferFrames = (frameBufferSize >> PAGE_BITS) + 1;

		for (int i = 0; i < frameBufferFrames; ++i)
		{
			PalmyraOS::kernel::PhysicalMemory::reserveFrame(
				(void*)(vbe_mode_info->framebuffer + (i << PAGE_BITS))
			);
		}
	}
	return true;
}

bool PalmyraOS::kernel::initializeVirtualMemory(multiboot_info_t* x86_multiboot_info)
{
	/**
	 * @brief Initializes the virtual memory manager.
	 *
	 * This function initializes the kernel paging directory, maps the kernel and video memory by identity,
	 * switches to the new kernel paging directory, and initializes the paging system.
	 *
	 * @param x86_multiboot_info Pointer to the multiboot information structure.
	 * @return True if the virtual memory manager is successfully initialized, false otherwise.
	 */

	/* Here we assume physical memory has been initialized, hence we do not use kmalloc anymore
	 * Instead, we use PhysicalMemory::allocateFrames()
	*/

	// Initialize and ensure kernel directory is aligned ~ 8 KiB = 3 frames
	uint32_t PagingDirectoryFrames = (sizeof(PagingDirectory) >> PAGE_BITS) + 1;
	kernel::kernelPagingDirectory_ptr = (PagingDirectory*)PhysicalMemory::allocateFrames(PagingDirectoryFrames);

	// Ensure the pointer we have is aligned
	if ((uint32_t)kernel::kernelPagingDirectory_ptr & (PAGE_SIZE - 1))
		kernel::kernelPanic("Unaligned Kernel Directory at 0x%X", kernel::kernelPagingDirectory_ptr);

	// Map kernel space by identity
	{
		auto kernelSpace = (uint32_t)PhysicalMemory::allocateFrame();
		kernel::kernelLastPage = kernelSpace >> PAGE_BITS;
		kernel::kernelPagingDirectory_ptr->mapPages(
			nullptr,
			nullptr,
			kernel::kernelLastPage,
			PageFlags::Present | PageFlags::ReadWrite
		);
	}

	// Initialize all kernel's directory tables, to avoid Recursive Page Table Mapping Problem
	size_t   max_pages = (x86_multiboot_info->mem_upper >> 12) + 1; // Kilobytes to 4 Megabytes
	for (int i         = 0; i < max_pages; ++i)
	{
		kernel::kernelPagingDirectory_ptr->getTable(i, PageFlags::Present | PageFlags::ReadWrite);
	}

	// Map video memory by identity
	auto* vbe_mode_info = (vbe_mode_info_t*)(uintptr_t)x86_multiboot_info->vbe_mode_info;
	{
		uint32_t frameBufferSize   = vbe_ptr->getVideoMemorySize();
		uint32_t frameBufferFrames = (frameBufferSize >> PAGE_BITS) + 1;
		kernel::kernelPagingDirectory_ptr->mapPages(
			(void*)vbe_mode_info->framebuffer,
			(void*)vbe_mode_info->framebuffer,
			frameBufferFrames,
			PageFlags::Present | PageFlags::ReadWrite
		);
	}



	// Switch to the new kernel paging directory and initialize paging
	PalmyraOS::kernel::PagingManager::switchPageDirectory(kernel::kernelPagingDirectory_ptr);
	PalmyraOS::kernel::PagingManager::initialize();

	return true;
}

void PalmyraOS::kernel::testMemory()
{
	/**
	 * @brief Tests the memory system.
	 *
	 * This function performs various tests on the paging and heap systems
	 * to ensure their correct operation.
	 */

	// paging
	if (!Tests::Paging::testPagingBoundaries())
		kernel::kernelPanic("Testing Paging boundaries failed!");

	if (!Tests::Paging::testPageTableAllocation())
		kernel::kernelPanic("Testing Paging table allocation failed!");

	if (!Tests::Paging::testNullPointerException())
		kernel::kernelPanic("Testing Paging nullptr allocation failed!");

	// heap
	if (!Tests::Heap::testHeapAllocation())
		kernel::kernelPanic("Testing Heap allocation failed!");

//	if (!Tests::Heap::testHeapCoalescence())
//		kernel::kernelPanic("Testing Heap Coalescence failed!");

	// standard library
	if (!Tests::Allocator::testVector())
		kernel::kernelPanic("Testing Allocator Vector failed!");

	if (!Tests::Allocator::testVectorOfClasses())
		kernel::kernelPanic("Testing Allocator Vector Classes failed!");

	if (!Tests::Allocator::testString())
		kernel::kernelPanic("Testing Allocator String failed!");

	if (!Tests::Allocator::testMap())
		kernel::kernelPanic("Testing Allocator Map failed!");

	if (!Tests::Allocator::testSet())
		kernel::kernelPanic("Testing Allocator Map failed!");

	if (!Tests::Allocator::testUnorderedMap())
		kernel::kernelPanic("Testing Allocator Unordered Map failed!");

//	if (!Tests::Allocator::testQueue())
//		kernel::kernelPanic("Testing Allocator Queue failed!");
}

void PalmyraOS::kernel::initializeDrivers()
{
	LOG_INFO("Start");

	ata_primary_master = heapManager.createInstance<ATA>(0x1F0, ATA::Type::Master);
	ata_primary_slave  = heapManager.createInstance<ATA>(0x1F0, ATA::Type::Slave);

	if (!ata_primary_master)
	{
		LOG_ERROR("Failed to allocate memory for ATA Primary Master Device!");
		return;
	}

	if (!ata_primary_slave)
	{
		LOG_ERROR("Failed to allocate memory for ATA Primary Slave Device!");
		return;
	}

	if (ata_primary_master->identify(1000))
	{
		char buffer[512];
		ata_primary_master->readSector(0, (uint8_t*)buffer, 1000);
		LOG_INFO("Sector 0: '%s'", buffer);
	}

	if (ata_primary_slave->identify(1000))
	{
		LOG_INFO("ATA Device Identified: "
				 "Model [%s], Serial [%s], Firmware [%s], Sectors [%zu], Space [%zu MiB]",
				 ata_primary_slave->getModelNumber(),
				 ata_primary_slave->getSerialNumber(),
				 ata_primary_slave->getFirmwareVersion(),
				 ata_primary_slave->getSectors28Bit(),
				 ata_primary_slave->getStorageSize() / 1048576
		);

		char buffer[512];
		ata_primary_slave->readSector(0, (uint8_t*)buffer, 1000);
		LOG_INFO("Sector 0: '%s'", buffer);
	}

}

void PalmyraOS::kernel::initializePartitions()
{
	LOG_INFO("Start");

	if (!kernel::ata_primary_master)
	{
		LOG_WARN("ATA Primary Master is not available. Cannot initialize Partitions.");
		return;
	}

	// dereference graphics tools
	auto& textRenderer = *kernel::textRenderer_ptr;


	// Read the Master Boot Record (MBR)
	uint8_t masterSector[512];
	kernel::ata_primary_master->readSector(0, masterSector, 100);
	auto mbr = kernel::vfs::MasterBootRecord(masterSector);

	// Loop through partition entries
	for (int i = 0; i < 4; ++i)
	{
		// get MBR entry
		auto mbr_entry = mbr.getEntry(i);
		LOG_INFO("ATA Primary Master: Partition %d:", i);
		LOG_INFO(
			"bootable: %d, Type: %s, lbaStart: 0x%X, Size: %d MiB",
			mbr_entry.isBootable,
			kernel::vfs::MasterBootRecord::toString(mbr_entry.type),
			mbr_entry.lbaStart,
			mbr_entry.lbaCount * 512 / 1048576
		);

		// If we have a valid FAT32 entry
		if (mbr_entry.type == PalmyraOS::kernel::vfs::MasterBootRecord::FAT32_LBA)
		{


			// TODO factor out to another function for FAT32 Partitions

			// Initialize a Virtual Disk (Guard against writing outside boundaries)
			auto virtualDisk = heapManager.createInstance<kernel::VirtualDisk<kernel::ATA>>(
				*kernel::ata_primary_master,
				mbr_entry.lbaStart,
				mbr_entry.lbaCount
			);
			// kernel::VirtualDisk<kernel::ATA> virtualDisk(*kernel::ata_primary_master, mbr_entry.lbaStart, mbr_entry.lbaCount);

			// Pass the virtual disk to Fat32 Partition TODO add to partitions vector somewhere
			auto fat32_p = heapManager
				.createInstance<vfs::FAT32Partition>(*virtualDisk, mbr_entry.lbaStart, mbr_entry.lbaCount);
			auto& fat32 = *fat32_p;
			// TODO: Mounting System

			auto rootNode = heapManager.createInstance<vfs::FAT32Directory>(
				fat32,
				2,
				vfs::InodeBase::Mode::USER_READ,
				vfs::InodeBase::UserID::ROOT,
				vfs::InodeBase::GroupID::ROOT
			);
			vfs::VirtualFileSystem::setInodeByPath(KString("/dev/sda"), rootNode);

			vfs::VirtualFileSystem::createDirectory(KString("/dev/sdb"), vfs::InodeBase::Mode::USER_READ);



			// TODO: Tests
			{
				// Read some well known files
//				auto data1 = fat32.readFile(15, 8);
//				auto data2 = fat32.readFile(11, 8);
//				auto data3 = fat32.readFile(8, 129);

				// Log root entries
				auto entries = fat32.getDirectoryEntries(2);
				for (auto& _entry : entries)
				{
					LOG_INFO("%s", _entry.getNameLong().c_str());
				}

				// TODO: Try to get some folders/files by path
				if (false)
				{
					auto a = fat32.resolvePathToEntry(kernel::KString("/"));
					auto b = fat32.resolvePathToEntry(kernel::KString("/folder"));
					auto c = fat32.resolvePathToEntry(kernel::KString("/folderx"));
					auto d = fat32.resolvePathToEntry(kernel::KString("/folder/subfolder"));
				}

				// TODO: Test Reading a File
				if (true)
				{
//					auto e            = fat32.resolvePathToEntry(kernel::KString("/folder2/fprint2.elf"));
					auto e            = fat32.resolvePathToEntry(kernel::KString("/folder2/fprint_args.elf"));
					auto file_content = fat32.readFile(e.getFirstCluster(), e.getFileSize());

					char* argv[] = {
						const_cast<char*>("/folder2/fprint_args.elf"),
						const_cast<char*>("arg1"),
						const_cast<char*>("arg2"),
						nullptr
					};
					kernel::TaskManager::execv_elf(
						file_content,
						kernel::Process::Mode::User,
						kernel::Process::Priority::Low,
						3,
						argv
					);
//					loadElf(file_content);
//					file_content.push_back('\0');
//					textRenderer << "\"" << (char*)file_content.data() << "\"\n" << SWAP_BUFF();
//					while (true);
				}

				// TODO: Test Changing File size
				if (false)
				{
					auto e     = fat32.resolvePathToEntry(kernel::KString("/exp1.txt"));
					auto data4 = fat32.readFile(e.getFirstCluster(), e.getFileSize());
					e.setFileSize(8);
					// fat32.flushEntry(e);
				}

				// TODO: Test Creating a file
				if (false)
				{
					auto b = fat32.resolvePathToEntry(kernel::KString("/folder"));
					if (fat32.createFile(b, kernel::KString("milx.bs")))
					{
						textRenderer << "created file!\n" << SWAP_BUFF();
					}
				}

				// TODO: Test Appending to a file
				if (false)
				{
					auto     f     = fat32.resolvePathToEntry(kernel::KString("/exp1.txt"));
					auto     data5 = fat32.read(f, 0, 50);
					for (int j     = 0; j < 50; ++j)
					{
						for (int k = 0; k < 1; ++k)
						{
							fat32.append(f, { 'w', 'h', 'a', 't', ' ', '\n' });
						}
						textRenderer << j << ", " << SWAP_BUFF();
					}
					auto     data6 = fat32.read(f, 0, 50);
				}

				// TODO: Test Overriding a file
				if (false)
					for (int j = 0; j < 100; ++j)
					{
						auto f     = fat32.resolvePathToEntry(kernel::KString("/exp2.txt"));
						auto data5 = fat32.read(f, 0, 50);

						kernel::KVector<uint8_t> data;
						for (int                 u = 0; u < 500; ++u)
						{
							data.push_back('w');
							data.push_back('h');
							data.push_back('a');
							data.push_back('t');
							data.push_back('\n');
						}

						fat32.write(f, data);
						textRenderer << j << ", " << SWAP_BUFF();

						auto data6 = fat32.read(f, 0, 50);
					}


			}


		}

	}


}


