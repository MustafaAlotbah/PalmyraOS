
#include "core/memory/paging.h"
#include "libs/memory.h"
#include "core/panic.h"
#include "core/memory/PhysicalMemory.h"
#include "core/kernel.h"
#include "core/tasks/ProcessManager.h"

// External functions from assembly (paging.asm)
extern "C" void set_page_directory(uint32_t*);
extern "C" void enable_paging();
extern "C" bool is_paging_enabled();
extern "C" uint32_t get_cr3();


// Initialize static member
PalmyraOS::kernel::PagingDirectory* PalmyraOS::kernel::PagingManager::currentPageDirectory_ = nullptr;
PalmyraOS::kernel::PageFaultHandler PalmyraOS::kernel::PagingManager::secondaryHandler_ = nullptr;


///region PagingDirectory

PalmyraOS::kernel::PagingDirectory::PagingDirectory()
{
	// Ensure page directory and tables are aligned
	if ((uint32_t)pageDirectory_ & 0xFFF) kernel::kernelPanic("Unaligned Page Directory_ at 0x%X", pageDirectory_);
	if ((uint32_t)pageTables_ & 0xFFF) kernel::kernelPanic("Unaligned Page Tables_ at 0x%X", pageTables_);

	// Initialize page directory and tables to zero
	memset(pageDirectory_, 0, sizeof(PageDirectoryEntry) * NUM_ENTRIES);
	memset(pageTables_, 0, sizeof(PageTableEntry*) * NUM_ENTRIES);
}

uint32_t* PalmyraOS::kernel::PagingDirectory::getTable(uint32_t tableIndex, PageFlags flags)
{
	// Check if the table is already present
	if (pageDirectory_[tableIndex].present) return (uint32_t*)pageTables_[tableIndex];

	// Allocate a new frame for the table if not present
	void* newTable = PhysicalMemory::allocateFrame();
	if ((uint32_t)newTable & 0xFFF) kernel::kernelPanic("Unaligned Page Table at 0x%X", newTable);

	// Save the table address
	pageTables_[tableIndex] = (PageTableEntry*)newTable;

	// Set the table in the directory with present and writable flags
	setTable(tableIndex, (uint32_t)newTable, flags);

	// Initialize the new table, assume here we have access to newTable address (kernel must be mapped)
	// make sure you map first, before you memset (if paging is active)
	if (PagingManager::getCurrentPageDirectory())
	{
		PagingManager::getCurrentPageDirectory()->mapPage(
			newTable, newTable, PageFlags::Present | PageFlags::ReadWrite
		);
	}
	{
		/*
		 * WARNING: here we might override mapPage if 1-1 mapping points the table to itself
		 * hence allocate should be called once paging is activated, there we can prevent this,
		 * by mapping the new table in a declared table, before we allocate a new page.
		 */
		memset(newTable, 0, PAGE_SIZE);    // reset all to zero to prevent wierd behavior
	}
	// map the page again, for the case if the page is mapped in itself
	// this is only important before paging is activated
	mapPage(newTable, newTable, flags);
	pagesCount_--; // to avoid double increment by mapPage()

	return (uint32_t*)pageTables_[tableIndex];
}

void PalmyraOS::kernel::PagingDirectory::destruct()
{
	// Free all present tables in the directory
	for (auto& tableIndex : pageDirectory_)
		if (tableIndex.present)
		{
			PhysicalMemory::freeFrame((void*)(tableIndex.tableAddress << 12));
			// TODO actually free the allocated pages too
		}
}

void PalmyraOS::kernel::PagingDirectory::setTable(uint32_t tableIndex, uint32_t tableAddress, PageFlags flags)
{
	// Set the page directory entry for the table
	pageDirectory_[tableIndex].present = ((uint32_t)flags >> 0) & 0x1;
	pageDirectory_[tableIndex].rw      = ((uint32_t)flags >> 1) & 0x1;
	pageDirectory_[tableIndex].user    = ((uint32_t)flags >> 2) & 0x1;
	pageDirectory_[tableIndex].tableAddress = tableAddress >> 12;
}

void PalmyraOS::kernel::PagingDirectory::mapPage(void* physicalAddr, void* virtualAddr, PageFlags flags)
{
	// Check for null pointers
	if (physicalAddr == nullptr || virtualAddr == nullptr) return;

	// map a page without physical allocation
	// physical address:  tableIndex:10, pageIndex:10, addressInsidePage:12

	// Calculate table index	(highest 10 bits)
	uint32_t tableIndex = (uint32_t)virtualAddr >> 22;

	// Calculate page index in the table (second highest 10 bits)
	uint32_t pageIndex = ((uint32_t)virtualAddr >> 12) & 0x3FF;

	// Get or create the corresponding table
	uint32_t* table = getTable(tableIndex, flags);
	// if table == physical address --> problem
	// Set the page in the table
	setPage(table, pageIndex, (uint32_t)physicalAddr, flags);

	// Increment the page count
	pagesCount_++;
}

void PalmyraOS::kernel::PagingDirectory::unmapPage(void* virtualAddr)
{
	// Check for null pointers
	if (virtualAddr == nullptr) return;

	// map a page without physical allocation
	// physical address:  tableIndex:10, pageIndex:10, addressInsidePage:12

	// Calculate table index	(highest 10 bits)
	uint32_t tableIndex = (uint32_t)virtualAddr >> 22;

	// Calculate page index in the table (second highest 10 bits)
	uint32_t pageIndex = ((uint32_t)virtualAddr >> 12) & 0x3FF;

	// Get the table address
	auto* table = (uint32_t*)(pageDirectory_[tableIndex].tableAddress << 12);

	// Check if the table is present
	if (pageDirectory_[tableIndex].present)
	{
		// Unset the page in the table
//		setPage(table, pageIndex, 0, 0);

		auto* entry = (PageTableEntry*)&table[pageIndex];
		memset(entry, 0, sizeof(PageTableEntry));

		// Decrement the page count
		pagesCount_--;
	}

	// Flush the TLB for the unmapped page
	asm volatile("invlpg (%0)"::"r" (virtualAddr) : "memory");
}

void PalmyraOS::kernel::PagingDirectory::setPage(
	uint32_t* table, uint32_t pageIndex, uint32_t physicalAddr, PageFlags flags
)
{
	// Set the page table entry
	auto* entry = (PageTableEntry*)&table[pageIndex];
	entry->present = ((uint32_t)flags >> 0) & 0x1;
	entry->rw      = ((uint32_t)flags >> 1) & 0x1;
	entry->user    = ((uint32_t)flags >> 2) & 0x1;
	entry->physicalAddress = physicalAddr >> 12;
}

uint32_t* PalmyraOS::kernel::PagingDirectory::getDirectory() const
{
	return (uint32_t*)pageDirectory_;
}

void* PalmyraOS::kernel::PagingDirectory::allocatePage(PageFlags flags)
{
	// Allocate a frame
	void* frame = PhysicalMemory::allocateFrame();
	if (frame == nullptr) return nullptr;

	// Calculate table index	(highest 10 bits)
	uint32_t tableIndex = (uint32_t)frame >> 22;

	// Calculate page index in the table (second highest 10 bits)
	uint32_t pageIndex = ((uint32_t)frame >> 12) & 0x3FF;

	// check if pageIndex == 1023 ==> new table
	if (pageIndex == 1023)
	{
		PhysicalMemory::freeFrame(frame);
		getTable(tableIndex + 1, flags);
		return allocatePage();
	}

	// Map the frame to itself
	mapPage(frame, frame, flags);

	return frame;
}

void* PalmyraOS::kernel::PagingDirectory::allocatePages(size_t numPages)
{
	// Allocate the first frame to determine the starting point
	void* startFrame = PhysicalMemory::allocateFrames(numPages);
	if (startFrame == nullptr) return nullptr;

	auto     startAddress = reinterpret_cast<uint32_t>(startFrame);
	uint32_t endAddress   = startAddress + (numPages * 0x1000);

	// Calculate table indices for start and end addresses
	uint32_t startTableIndex = startAddress >> 22;         // Table index of the start address
	uint32_t endTableIndex   = (endAddress - 1) >> 22;     // Table index of the last byte

	// Check if page allocation crosses an unallocated page table boundary
	bool          isReallocationRequired = false;
	for (uint32_t tableIndex             = startTableIndex; tableIndex <= endTableIndex; ++tableIndex)
	{
		// if any table is not present, mark to try again
		isReallocationRequired |= !pageDirectory_[tableIndex].present;
	}

	/* If reallocation is required, free the frames and allocate the necessary tables
	 * We require new page allocation, to map the next table in previous tables
	 * Check: Recursive page table mapping problem
	 */
	if (isReallocationRequired)
	{

		// Deallocate the originally allocated frames
		PhysicalMemory::freeFrames(startFrame, numPages);

		// Ensure all required page tables are allocated
		for (uint32_t tableIndex = startTableIndex; tableIndex <= endTableIndex; ++tableIndex)
		{
			// Allocate page table if not already present
			getTable(tableIndex, PageFlags::Present | PageFlags::ReadWrite);
		}

		// Retry allocation after ensuring tables are present
		return allocatePages(numPages);
	}

	// Map each page to itself in the page table
	for (size_t i = 0; i < numPages; ++i)
	{
		auto address = (uint32_t)startFrame + (0x1000 * i);
		mapPage((void*)address, (void*)address, PageFlags::Present | PageFlags::ReadWrite);
	}

	return startFrame;
}

bool PalmyraOS::kernel::PagingDirectory::isAddressValid(void* address)
{

	if (address == nullptr) return false;

	// Calculate virtual address, table index, and page index
	auto     virtualAddr = (uint32_t)address;
	uint32_t tableIndex  = virtualAddr >> 22;
	uint32_t pageIndex   = (virtualAddr >> 12) & 0x3FF;

	// Check if the table is present
	if (!pageDirectory_[tableIndex].present) return false;

	// Get the table and entry
	PageTableEntry* table = pageTables_[tableIndex];
	PageTableEntry* entry = &table[pageIndex];

	// Check if the page is present
	if (!entry->present) return false;

	// if present, return true
	return true;
}

void* PalmyraOS::kernel::PagingDirectory::getPhysicalAddress(void* address)
{
	if (address == nullptr) return nullptr;

	// Convert the virtual address to a 32-bit unsigned integer for manipulation
	auto virtualAddr = reinterpret_cast<uint32_t>(address);

	// Extract the Page Directory Index (bits 22-31)
	uint32_t tableIndex = (virtualAddr >> 22) & 0x3FF;

	// Extract the Page Table Index (bits 12-21)
	uint32_t pageIndex = (virtualAddr >> 12) & 0x3FF;

	// Extract the Offset (bits 0-11)
	uint32_t offset = virtualAddr & 0xFFF;

	// Check if the Page Directory Entry is present
	if (!pageDirectory_[tableIndex].present) return nullptr;

	// Retrieve the Page Table
	PageTableEntry* table = pageTables_[tableIndex];
	if (table == nullptr) return nullptr;

	// Retrieve the Page Table Entry
	PageTableEntry* entry = &table[pageIndex];
	if (!entry->present) return nullptr;

	// Calculate the Physical Address
	uint32_t physicalAddr = (entry->physicalAddress << 12) | offset;

	return reinterpret_cast<void*>(physicalAddr);
}



void PalmyraOS::kernel::PagingDirectory::freePage(void* pageAddress)
{
	if (pageAddress == nullptr) return;

	// Calculate virtual address, table index, and page index
	auto     virtualAddr = (uint32_t)pageAddress;
	uint32_t tableIndex  = virtualAddr >> 22;
	uint32_t pageIndex   = (virtualAddr >> 12) & 0x3FF;

	// Check if the table is present
	if (!pageDirectory_[tableIndex].present)
	{
		kernelPanic("Attempted to free a page from a non-present table");
		return;
	}


	// Get the table and entry
	PageTableEntry* table = pageTables_[tableIndex];
	PageTableEntry* entry = &table[pageIndex];

	// Check if the page is present
	if (!entry->present)
	{
		kernelPanic("Attempted to free a non-present page");
		return;
	}

	// Free the physical frame
	void* physicalAddr = (void*)(entry->physicalAddress << 12);
	PhysicalMemory::freeFrame(physicalAddr);

	// Unmap the page
	unmapPage(pageAddress);

}

void PalmyraOS::kernel::PagingDirectory::mapPages(
	void* physicalAddr,
	void* virtualAddr,
	uint32_t numPages,
	PalmyraOS::kernel::PageFlags flags
)
{
	for (int i = 0; i < numPages; ++i)
	{
		auto physicalAddr_ = (uint32_t)physicalAddr + (i * PAGE_SIZE);
		auto virtualAddr_  = (uint32_t)virtualAddr + (i * PAGE_SIZE);
		mapPage((void*)physicalAddr_, (void*)virtualAddr_, flags);
	}
}


///endregion


///region PagingManager

void PalmyraOS::kernel::PagingManager::initialize()
{
	// Ensure a page directory is set
	if (currentPageDirectory_ == nullptr) kernelPanic("Cannot initialize paging: Invalid Page Directory");

	// Set the page fault handler
	interrupts::InterruptController::setInterruptHandler(0x0E, &handlePageFault);

	// Switch to the current page directory and enable paging
	switchPageDirectory(currentPageDirectory_);
	enable_paging();
}

void PalmyraOS::kernel::PagingManager::switchPageDirectory(PagingDirectory* newPageDirectory)
{
	// Ensure the new page directory is valid
	if (newPageDirectory == nullptr) kernelPanic("Cannot initialize paging: Invalid Page Directory");

	if (newPageDirectory->getDirectory() == (void*)get_cr3()) return;

	// Switch to the new page directory
	currentPageDirectory_ = newPageDirectory;
	set_page_directory(currentPageDirectory_->getDirectory());
}

PalmyraOS::kernel::PagingDirectory* PalmyraOS::kernel::PagingManager::createPageDirectory()
{
	// Create a new page directory (currently returns nullptr) TODO
	return nullptr;
}

PalmyraOS::kernel::PagingDirectory* PalmyraOS::kernel::PagingManager::getCurrentPageDirectory()
{
	// Return the current page directory
	return currentPageDirectory_;
}

void* PalmyraOS::kernel::PagingManager::allocatePage()
{
	// Ensure a page directory is set
	if (currentPageDirectory_ == nullptr)
		kernelPanic("PagingManager: Attempted to allocate a page before setting a directory");

	// Allocate a page in the current page directory
	return currentPageDirectory_->allocatePage();
}

void PalmyraOS::kernel::PagingManager::freePage(void* pageAddress)
{
	// Ensure a page directory is set
	if (currentPageDirectory_ == nullptr)
		kernelPanic("PagingManager: Attempted to free a page before setting a directory");

	// Free the page in the current page directory
	currentPageDirectory_->freePage(pageAddress);
}

void* PalmyraOS::kernel::PagingManager::allocatePages(size_t numPages)
{
	// Ensure a page directory is set
	if (currentPageDirectory_ == nullptr)
		kernelPanic("PagingManager: Attempted to allocate a page before setting a directory");

	// Allocate a page in the current page directory
	return currentPageDirectory_->allocatePages(numPages);
}

void PalmyraOS::kernel::PagingManager::setSecondaryPageFaultHandler(PageFaultHandler handler)
{
	secondaryHandler_ = handler;
}

uint32_t* PalmyraOS::kernel::PagingManager::handlePageFault(interrupts::CPURegisters* regs)
{

	// Original page fault handling code
	uint32_t faultingAddress;
	asm volatile("mov %%cr2, %0" : "=r" (faultingAddress));

	bool present          = regs->errorCode & 0x1;
	bool write            = regs->errorCode & 0x2;
	bool userMode         = regs->errorCode & 0x4;
	bool reserved         = regs->errorCode & 0x8;
	bool instructionFetch = regs->errorCode & 0x10;

	if (secondaryHandler_)
	{
		secondaryHandler_(regs, faultingAddress, present, write, userMode, instructionFetch);
	}
	else
	{
		// Fetch current process
		auto& currentProcess = *TaskManager::getCurrentProcess();
		auto pid           = currentProcess.getPid();
		auto userStack     = currentProcess.getUserStack();
		bool stackOverflow = currentProcess.checkStackOverflow();

		// Handle page fault by triggering a kernel panic  TODO (for example, crash current process)
		kernelPanic(
			"Page Fault (0x%X) (0x%X) at 0x%X\n"
			"System: CR3: 0x%X, Kernel CR3: 0x%X\n"
			"isPresent: %s\n"
			"isWrite: %s\n"
			"isUser: %s\n"
			"isInstructionFetch: %s\n"
			"Faulting Instruction: 0x%X\n"
			"--------------------------\n"
			"Process: %d\n"
			"User Stack: 0x%X\n"
			"Stack Overflow: %s\n",
			regs->intNo, regs->errorCode, faultingAddress,
			currentPageDirectory_->getDirectory(), kernel::kernelPagingDirectory_ptr->getDirectory(),
			(present ? "YES" : "NO"),
			(write ? "YES" : "NO"),
			(userMode ? "YES" : "NO"),
			(instructionFetch ? "YES" : "NO"),
			regs->eip,
			pid, userStack,
			(stackOverflow ? "YES" : "NO")
		);
	}

	return (uint32_t*)regs;
}

bool PalmyraOS::kernel::PagingManager::isEnabled()
{
	return is_paging_enabled();
}
///endregion


