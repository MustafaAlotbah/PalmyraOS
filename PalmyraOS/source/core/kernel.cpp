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
#include "core/cpu.h"
#include <new>
#include <algorithm>

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
  PalmyraOS::kernel::ATA* ata_primary_master   = nullptr;
  PalmyraOS::kernel::ATA* ata_primary_slave    = nullptr;
  PalmyraOS::kernel::ATA* ata_secondary_master = nullptr;
  PalmyraOS::kernel::ATA* ata_secondary_slave  = nullptr;


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
	FontManager::initialize();

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
		new(kernel::textRenderer_ptr) TextRenderer(kernel::vbe_ptr->getFrameBuffer(), Font::Arial12);
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
		textRenderer << Color::Orange << "Palmyra" << Color::LighterBlue << "OS ";
		textRenderer << Color::Gray100 << "v0.01\n";

		// Draw a horizontal line below the logo text
		brush_ptr->drawHLine(1, 150, textRenderer.getCursorY() + 2, Color::Gray100);
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

	auto& textRenderer = *kernel::textRenderer_ptr;

	// Initialize and ensure kernel directory is aligned ~ 8 KiB = 3 frames
	uint32_t PagingDirectoryFrames = (sizeof(PagingDirectory) >> PAGE_BITS) + 1;
	kernel::kernelPagingDirectory_ptr = (PagingDirectory*)PhysicalMemory::allocateFrames(PagingDirectoryFrames);

	// Ensure the pointer we have is aligned
	if ((uint32_t)kernel::kernelPagingDirectory_ptr & (PAGE_SIZE - 1))
		kernel::kernelPanic("Unaligned Kernel Directory at 0x%X", kernel::kernelPagingDirectory_ptr);

	// Map kernel space by identity
	{
		textRenderer << "Kernel.." << SWAP_BUFF();
		kernel::CPU::delay(2'500'000'000L);
		auto kernelSpace = (uint32_t)PhysicalMemory::allocateFrame();
		kernel::kernelLastPage = kernelSpace >> PAGE_BITS;
		kernel::kernelPagingDirectory_ptr->mapPages(
			nullptr,
			nullptr,
			kernel::kernelLastPage,
			PageFlags::Present | PageFlags::ReadWrite
		);
	}

	textRenderer << "Tables.." << SWAP_BUFF();
	kernel::CPU::delay(2'500'000'000L);
	// Initialize all kernel's directory tables, to avoid Recursive Page Table Mapping Problem
	size_t   max_pages = (x86_multiboot_info->mem_upper >> 12) + 1; // Kilobytes to 4 Megabytes
	for (int i         = 0; i < max_pages; ++i)
	{
		kernel::kernelPagingDirectory_ptr->getTable(i, PageFlags::Present | PageFlags::ReadWrite);
	}

	// Map video memory by identity
	textRenderer << "Video.." << SWAP_BUFF();
	kernel::CPU::delay(2'500'000'000L);
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
	textRenderer << "switching.." << SWAP_BUFF();
	kernel::CPU::delay(2'500'000'000L);
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

	auto& textRenderer = *kernel::textRenderer_ptr;

	LOG_INFO("Start");

	ata_primary_master   = heapManager.createInstance<ATA>(0x1F0, ATA::Type::Master);
	ata_primary_slave    = heapManager.createInstance<ATA>(0x1F0, ATA::Type::Slave);
	ata_secondary_master = heapManager.createInstance<ATA>(0x170, ATA::Type::Master);
	ata_secondary_slave  = heapManager.createInstance<ATA>(0x170, ATA::Type::Slave);

	auto atas = {
		std::pair(ata_primary_master, "pM"),
		std::pair(ata_primary_slave, "pS"),
		std::pair(ata_secondary_master, "sM"),
		std::pair(ata_secondary_slave, "sS")
	};

	for (auto& [ata, name] : atas)
	{
		if (!ata)
		{
			LOG_ERROR("Failed to allocate memory for ATA %s Device!", name);
			continue; // TODO: or handle in a different way
		}

		textRenderer << "\nAt [" << name << "] ";
		if (!ata->identify(1000))
		{
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
				 ata->getStorageSize() / 1048576
		);
		textRenderer << "Model [" << ata->getModelNumber()
					 << "] Serial [" << ata->getSerialNumber()
					 << "] Firmware [" << ata->getFirmwareVersion()
					 << "] Sectors [" << ata->getSectors28Bit()
					 << "] Space [" << (ata->getStorageSize() / 1048576)
					 << " MiB]";
	}

	textRenderer << "\n";
}

void PalmyraOS::kernel::initializePartitions()
{
	LOG_INFO("Start");
	// dereference graphics tools
	auto& textRenderer = *kernel::textRenderer_ptr;

	if (!kernel::ata_primary_master)
	{
		textRenderer << "No ATA..";
		LOG_WARN("ATA Primary Master is not available. Cannot initialize Partitions.");
		return;
	}


	auto atas = {
		std::pair(kernel::ata_primary_master, KString("/dev/sda")),
		std::pair(kernel::ata_primary_slave, KString("/dev/sdb")),
		std::pair(kernel::ata_secondary_master, KString("/dev/sdc")),
		std::pair(kernel::ata_secondary_slave, KString("/dev/sde"))
	};

	for (auto& [ata, name] : atas)
	{
		// Read the Master Boot Record (MBR)
		uint8_t masterSector[512];
		ata->readSector(0, masterSector, 100);
		auto mbr = kernel::vfs::MasterBootRecord(masterSector);

		// Loop through partition entries
		for (int i = 0; i < 4; ++i)
		{
			if (!ata) continue; // should not evaluate to true, but for the sake of completeness

			// get MBR entry
			auto mbr_entry = mbr.getEntry(i);
			textRenderer << "[" << i << ": ";
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
					*ata,
					mbr_entry.lbaStart,
					mbr_entry.lbaCount
				);

				// Pass the virtual disk to Fat32 Partition TODO add to partitions vector somewhere
				auto fat32_p = heapManager.createInstance<vfs::FAT32Partition>(
					*virtualDisk, mbr_entry.lbaStart, mbr_entry.lbaCount
				);
				auto& fat32 = *fat32_p;
				// TODO: Mounting System
				// TODO: check if the type is FAT32 not FAT32 etc..

				if (fat32.getType() == vfs::FAT32Partition::Type::Invalid) textRenderer << "Invalid Fat (X)] ";
				else if (fat32.getType() == vfs::FAT32Partition::Type::FAT12) textRenderer << "FAT12 (X)] ";
				else if (fat32.getType() == vfs::FAT32Partition::Type::FAT16) textRenderer << "FAT16 (X)] ";
				if (fat32.getType() != vfs::FAT32Partition::Type::FAT32)
				{
					// Invalid
					heapManager.free(fat32_p);
					continue;
				}

				textRenderer << "FAT32]";
				auto rootNode = heapManager.createInstance<vfs::FAT32Directory>(
					fat32,
					2,
					vfs::InodeBase::Mode::USER_READ,
					vfs::InodeBase::UserID::ROOT,
					vfs::InodeBase::GroupID::ROOT
				);
				vfs::VirtualFileSystem::setInodeByPath(name, rootNode);

				// Debug: Analyze directory entries to see how Windows writes LFNs
				LOG_WARN("=== ANALYZING WINDOWS-RENAMED FILE ===");
				
				vfs::DirectoryEntry folder = fat32.resolvePathToEntry(KString("/exp4/"));


				auto newfile = fat32.createFile(folder, KString("e7newfile19.txt"));
				//auto newfile = fat32.resolvePathToEntry(KString("/exp/mynewfile2.txt"));


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



				if (folder.getAttributes() != vfs::EntryAttribute::Invalid)
				{
					LOG_WARN("Found /exp2/ directory, reading entries...");
					uint32_t dirCluster = folder.getFirstCluster();
					KVector<uint8_t> dirData = fat32.readEntireFile(dirCluster);
					LOG_WARN("Directory has %u bytes of data", dirData.size());

					struct LfnPart { uint8_t ord; KWString w; size_t off; };
					KVector<LfnPart> lfnParts; // collected after seeing LAST
					uint8_t  lfnChecksum = 0;
					bool     lfnActive = false;

					auto dumpHex = [&](size_t off)
					{
						char hexDump[100]; size_t pos = 0;
						for (size_t j = 0; j < 32 && pos < 99; ++j)
						{
							if (j == 16) { hexDump[pos++]=' '; hexDump[pos++]='|'; hexDump[pos++]=' '; }
							else if (j > 0 && j % 4 == 0) { hexDump[pos++]=' '; }
							uint8_t b = dirData[off + j];
							static const char hx[] = "0123456789ABCDEF";
							hexDump[pos++] = hx[b >> 4];
							hexDump[pos++] = hx[b & 0x0F];
							hexDump[pos++] = ' ';
						}
						hexDump[pos] = '\0';
						LOG_WARN("  Hex: %s", hexDump);
					};

					auto trimSfn = [](const char* p, int n)->int { while (n>0 && p[n-1]==' ') --n; return n; };
					auto asciiFromUtf16 = [](const KWString& w, char* out, size_t outCap)
					{
						size_t o=0; for (size_t i=0;i<w.size() && o+1<outCap;i++){ uint16_t ch=w[i]; if (ch==0x0000) break; out[o++]=(ch<128)?(char)ch:'?'; }
						out[o]='\0';
					};

					for (size_t i = 0; i + 32 <= dirData.size(); i += 32)
					{
						uint8_t first = dirData[i];
						if (first == 0x00) break;
						if (first == 0xE5) continue;

						LOG_WARN("=== Entry at offset %u ===", (uint32_t)i);
						dumpHex(i);

						uint8_t attr = dirData[i + 11];
						if ((attr & 0x3F) == 0x0F)
						{
							uint8_t seq = dirData[i];
							uint8_t chk = dirData[i + 13];
							bool isLast = (seq & 0x40) != 0;
							uint8_t ord = (uint8_t)(seq & 0x3F);
							LOG_WARN("  Type: LFN, Seq=0x%02X%s, Checksum=0x%02X", ord, isLast?" (LAST)":"", chk);

							if (isLast) { lfnParts.clear(); lfnChecksum = chk; lfnActive = true; }

							KWString part;
							auto addRange = [&](size_t start, size_t count)
							{
								for (size_t j=0;j<count;j++)
								{
									uint16_t ch = (uint16_t)(dirData[i + start + j*2] | (dirData[i + start + j*2 + 1] << 8));
									if (ch == 0x0000 || ch == 0xFFFF) break;
									part.push_back(ch);
								}
							};
							addRange(1,5); addRange(14,6); addRange(28,2);
							if (lfnActive) lfnParts.push_back({ord, part, i});
						}
						else
						{
							const char* sfn = (const char*)&dirData[i];
							int baseLen = trimSfn(sfn, 8);
							int extLen  = trimSfn(sfn+8, 3);
							char sfnPretty[16];
							int p=0; for (int k=0;k<baseLen && p<15;k++) sfnPretty[p++]=sfn[k];
							if (extLen>0 && p<15) sfnPretty[p++]='.';
							for (int k=0;k<extLen && p<15;k++) sfnPretty[p++]=sfn[8+k];
							sfnPretty[p]='\0';

							uint8_t sum=0; for (int k=0;k<11;k++) sum = ((sum & 1)<<7) + (sum>>1) + (uint8_t)sfn[k];
							LOG_WARN("  Type: SFN, Attr=0x%02X", attr);
							LOG_WARN("  Name: '%s'", sfnPretty);
							LOG_WARN("  First char byte: 0x%02X", (uint8_t)sfn[0]);
							LOG_WARN("  Checksum: 0x%02X", sum);
							LOG_WARN("  SFN[11]: %02X %02X %02X %02X %02X %02X %02X %02X . %02X %02X %02X",
								(uint8_t)sfn[0],(uint8_t)sfn[1],(uint8_t)sfn[2],(uint8_t)sfn[3],(uint8_t)sfn[4],(uint8_t)sfn[5],(uint8_t)sfn[6],(uint8_t)sfn[7],(uint8_t)sfn[8],(uint8_t)sfn[9],(uint8_t)sfn[10]);

							if (lfnActive && !lfnParts.empty())
							{
								// Build full LFN in order ord=1..N
								std::sort(lfnParts.begin(), lfnParts.end(), [](const LfnPart& a, const LfnPart& b){ return a.ord < b.ord; });
								KWString lfnFull;
								for (auto& part : lfnParts) for (uint16_t ch : part.w) lfnFull.push_back(ch);
								char lfnAscii[64]; asciiFromUtf16(lfnFull, lfnAscii, sizeof(lfnAscii));
								LOG_WARN("  LFN->SFN Pair: long='%s', sfn='%s', matchChk=%s", lfnAscii, sfnPretty, (sum==lfnChecksum?"YES":"NO"));
								// Highlight target zynewfile17.txt
								auto ieq = [](char a){ return (a>='a'&&a<='z')? (char)(a-32):a; };
								const char* tgt = "ZYNEWFILE17.TXT";
								bool isTarget = true; for (size_t ti=0; tgt[ti] || lfnAscii[ti]; ++ti) { if (ieq(tgt[ti])!=ieq(lfnAscii[ti])) { isTarget=false; break; } }
								if (isTarget)
								{
									LOG_WARN(">>> TARGET /exp2/zynewfile17.txt: SFNoff=%u, LFNchk=0x%02X, SFNchk=0x%02X", (uint32_t)i, lfnChecksum, sum);
									// Dump SFN fields
									uint8_t ntRes = dirData[i+12];
									uint8_t cTenth = dirData[i+13];
									uint16_t cTime = (uint16_t)(dirData[i+14] | (dirData[i+15]<<8));
									uint16_t cDate = (uint16_t)(dirData[i+16] | (dirData[i+17]<<8));
									uint16_t aDate = (uint16_t)(dirData[i+18] | (dirData[i+19]<<8));
									uint16_t cluHi = (uint16_t)(dirData[i+20] | (dirData[i+21]<<8));
									uint16_t wTime = (uint16_t)(dirData[i+22] | (dirData[i+23]<<8));
									uint16_t wDate = (uint16_t)(dirData[i+24] | (dirData[i+25]<<8));
									uint16_t cluLo = (uint16_t)(dirData[i+26] | (dirData[i+27]<<8));
									uint32_t fSize = (uint32_t)(dirData[i+28] | (dirData[i+29]<<8) | (dirData[i+30]<<16) | (dirData[i+31]<<24));
									LOG_WARN("  SFN fields: ntRes=0x%02X cTenth=%u cTime=0x%04X cDate=0x%04X aDate=0x%04X wTime=0x%04X wDate=0x%04X clu=%u size=%u",
										ntRes, cTenth, cTime, cDate, aDate, wTime, wDate, ((uint32_t)cluHi<<16)|cluLo, fSize);
									// Dump LFN chain offsets and ords
									for (auto& part : lfnParts) LOG_WARN("  LFN part at off=%u ord=%u len=%u", (uint32_t)part.off, part.ord, (uint32_t)part.w.size());
								}
							}

							// Reset for next chain
							lfnParts.clear(); lfnActive = false; lfnChecksum = 0;
						}
					}
				}
				else
				{
					LOG_ERROR("Could not find /exp2/ directory!");
				}
				LOG_WARN("=== END WINDOWS FILE ANALYSIS ===");

				// TODO: Create new file test after analysis






				


			}
			else if (mbr_entry.type == PalmyraOS::kernel::vfs::MasterBootRecord::NTFS)
			{
				textRenderer << "NTFS (X)]"; // Unsupported
			}
			else
			{
				textRenderer << "(X)]"; // Unsupported and unrecognized
			}

		}
	}


}

void PalmyraOS::kernel::initializeBinaries()
{

	// Create a test inode with a lambda function for reading test string
	auto terminalNode = kernel::heapManager.createInstance<vfs::FunctionInode>(nullptr, nullptr, nullptr);
	if (!terminalNode) return;

	// Set the test inode at "/bin/terminal"
	vfs::VirtualFileSystem::setInodeByPath(KString("/bin/terminal.elf"), terminalNode);


}


