
// Implementations of the PagingTester functions

#include "tests/pagingTests.h"
#include "core/panic.h"

///region PagingTester
// Initialize static members of PagingTester
bool     PalmyraOS::Tests::Paging::PagingTester::pageFaultOccurred_ = false;
uint32_t PalmyraOS::Tests::Paging::PagingTester::faultingAddress_   = 0;
bool     PalmyraOS::Tests::Paging::PagingTester::isWrite_           = false;
volatile void* PalmyraOS::Tests::Paging::PagingTester::faultRecoveryAddress_ = nullptr;

void PalmyraOS::Tests::Paging::PagingTester::setup()
{
	kernel::PagingManager::setSecondaryPageFaultHandler(PagingTester::testingPageFaultHandler);
}

void PalmyraOS::Tests::Paging::PagingTester::reset()
{
	kernel::PagingManager::setSecondaryPageFaultHandler(nullptr);
	pageFaultOccurred_    = false;
	faultingAddress_      = 0;
	isWrite_              = false;
	faultRecoveryAddress_ = nullptr;
}

void PalmyraOS::Tests::Paging::PagingTester::testingPageFaultHandler(
	PalmyraOS::kernel::interrupts::CPURegisters* regs,
	uint32_t faultingAddress,
	bool isPresent,
	bool isWrite,
	bool isUserMode,
	bool instructionFetch
)
{
	// Set the page fault occurred flag and store faulting address and write status
	pageFaultOccurred_ = true;
	faultingAddress_   = faultingAddress;
	isWrite_           = isWrite;

	// If a fault recovery address is set, update EIP to that address
	if (faultRecoveryAddress_) regs->eip = (uint32_t)faultRecoveryAddress_;

		// If no recovery address is set, panic the kernel
	else kernel::kernelPanic("PagingTester: Page Fault occurred while no faultRecoveryAddress was set!");
}

void PalmyraOS::Tests::Paging::PagingTester::setFaultRecoveryAddress(volatile void* address)
{
	faultRecoveryAddress_ = address;
}
///endregion

///region Paging Tests

bool PalmyraOS::Tests::Paging::testPagingBoundaries()
{
	bool result    = false;
	bool pageFreed = false;

	// Set up the paging tester
	PagingTester::setup();
	// Set the fault recovery address to the clean label
	PagingTester::setFaultRecoveryAddress( && clean);

	// Allocate a page and check if allocation was successful
	int* pointer = (int*)kernel::PagingManager::allocatePage();
	if (!pointer) goto clean;

	// Write to the allocated page, which should not cause a page fault
	*pointer = 8;

	// Free the page and then try writing to it again
	kernel::PagingManager::freePage(pointer);
	pageFreed = true;

	// Set the fault recovery address to the continue_execution label
	PagingTester::setFaultRecoveryAddress( && continue_execution);
	*pointer = 9; // This should cause a page fault

continue_execution:
	// If we reach here, the page fault handling was successful
	result = true;

clean:
	// Free the page only if it hasn't been freed yet
	if (!pageFreed) kernel::PagingManager::freePage(pointer);

	// Reset the paging tester
	PagingTester::reset();
	return result;
}

bool PalmyraOS::Tests::Paging::testNullPointerException()
{
	bool result = false;

	// Set up the paging tester
	PagingTester::setup();

	// Set the fault recovery address to the clean label
	PagingTester::setFaultRecoveryAddress( && clean);

	// Attempt to write to nullptr, which should cause a page fault
	*(volatile int*)nullptr = 8;

	// If we reach here, the page fault did not occur as expected
	goto clean;

clean:
	// Check if the page fault occurred
	if (PagingTester::pageFaultOccurred()) result = true;

	// Reset the paging tester
	PagingTester::reset();
	return result;
}

bool PalmyraOS::Tests::Paging::testPageTableAllocation()
{
	bool result = true;

	// Set up the paging tester
	PagingTester::setup();

	// Set the fault recovery address to the end of the function
	PagingTester::setFaultRecoveryAddress( && clean);

	constexpr uint32_t numPages = 1025;
	int* addresses[numPages] = { nullptr };

	uint32_t allocatedPages = kernel::PagingManager::getCurrentPageDirectory()->getNumAllocatedPages();
	uint32_t allocatedPages_1;

	// Allocate enough pages to create a new page table and write to each page
	for (auto& address : addresses)
	{
		address = (int*)kernel::PagingManager::allocatePage();
		if (!address)
		{
			result = false;
			goto clean;
		}
		*address = 8; // This should not cause a page fault
	}

	allocatedPages_1 = kernel::PagingManager::getCurrentPageDirectory()->getNumAllocatedPages();
	if (allocatedPages_1 - allocatedPages < 1025) result = false;

clean:
	// Free all allocated pages
	for (auto& address : addresses)
	{
		if (address) kernel::PagingManager::freePage(address);
	}
	allocatedPages_1 = kernel::PagingManager::getCurrentPageDirectory()->getNumAllocatedPages();
	if (allocatedPages_1 - allocatedPages > 1)
		result       = false; // maximum 1 for the new table (which is not de-allocated)

	// Reset the paging tester
	PagingTester::reset();
	return result;
}

///endregion


///region Heap Tests


bool PalmyraOS::Tests::Heap::testHeapAllocation()
{
	bool result = false;

	// Set up the paging tester
	Paging::PagingTester::setup();
	// Set the fault recovery address to the clean label
	Paging::PagingTester::setFaultRecoveryAddress( && clean);

	uint32_t* pointer;
	{
		// initialize a heap instant
		kernel::HeapManager heap;

		constexpr uint32_t arraySize = 100;
		uint32_t* addresses[arraySize] = { nullptr };
		auto    * array                = (uint32_t*)heap.alloc(arraySize * sizeof(uint32_t));

		// initialize variables the array
		for (uint32_t i = 0; i < arraySize; ++i)
		{
			addresses[i] = (uint32_t*)heap.alloc(sizeof(uint32_t));
			if (!addresses[i])
			{
				result = false;
				goto clean;
			}
			*addresses[i] = i; // This should not cause a page fault
		}

		// now read and make sure they are correct
		for (uint32_t i = 0; i < arraySize; ++i)
		{
			if (*addresses[i] != i)
			{
				result = false;
				goto clean;
			}
		}

		// set pointer and make sure it works before destruction
		pointer = addresses[0];

		// destructor is called, pages are freed
	}

	// Set the fault recovery address to the continue_execution label
	Paging::PagingTester::setFaultRecoveryAddress( && continue_execution);
	*pointer = 9; // This should cause a page fault (hence heap destructor works fine)

continue_execution:
	// If we reach here, the page fault handling was successful
	result = true;

clean:
	// Reset the paging tester
	Paging::PagingTester::reset();
	return result;
}

bool PalmyraOS::Tests::Heap::testHeapCoalescence()
{
	bool result = false;

	// Set up the paging tester
	Paging::PagingTester::setup();
	// Set the fault recovery address to the clean label
	Paging::PagingTester::setFaultRecoveryAddress( && clean);

	{
		// Initialize a heap instance
		kernel::HeapManager heap;

		// Allocate multiple blocks of memory
		constexpr uint32_t blockSize = 128;
		constexpr uint32_t numBlocks = 10;
		void* addresses[numBlocks] = { nullptr };

		for (auto& address : addresses)
		{
			address = heap.alloc(blockSize);
			if (!address)
			{
				result = false;
				goto clean;
			}
		}

		// Free some blocks to create gaps
		for (uint32_t i = 1; i < numBlocks; i += 2)
		{
			heap.free(addresses[i]);
			addresses[i] = nullptr;
		}

		// Check heap status before coalescence
		uint32_t freeMemoryBefore = heap.getTotalFreeMemory();

		// Coalesce free blocks
		heap.coalesceFreeBlocks();

		// Check heap status after coalescence
		uint32_t freeMemoryAfter = heap.getTotalFreeMemory();

		// If the free memory increased, coalescence was successful
		if (freeMemoryAfter > freeMemoryBefore)
		{
			result = true;
		}
	}

clean:
	// Reset the paging tester
	Paging::PagingTester::reset();
	return result;
}


///endregion