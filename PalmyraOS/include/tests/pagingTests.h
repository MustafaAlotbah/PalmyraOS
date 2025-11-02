

#pragma once

#include "core/memory/paging.h"


namespace PalmyraOS::Tests::Paging {


    // Class for testing paging functionalities in the PalmyraOS kernel
    class PagingTester {
    public:
        static void setup();                                  // Set up the paging tester by setting the page fault handler
        static void reset();                                  // Reset the paging tester by clearing the page fault handler and resetting flags
        static void setFaultRecoveryAddress(volatile void*);  // Set the fault recovery address for the page fault handler
        inline static bool pageFaultOccurred() { return pageFaultOccurred_; };

    private:
        // The custom page fault handler for testing purposes
        static void testingPageFaultHandler(kernel::interrupts::CPURegisters* regs,
                                            uint32_t faultingAddress,
                                            bool isPresent,
                                            bool isWrite,
                                            bool isUserMode,
                                            bool instructionFetch);

    private:
        static bool pageFaultOccurred_;               // Flag to indicate if a page fault has occurred
        static volatile void* faultRecoveryAddress_;  // Address to jump to after a page fault for recovery
        static uint32_t faultingAddress_;             // The address where the page fault occurred
        static bool isWrite_;                         // Flag to indicate if the page fault was caused by a write operation
    };

    // Function to check paging boundaries and test page fault handling
    bool testPagingBoundaries();

    // Assures that writing to or reading from nullptr causes a page fault
    bool testNullPointerException();

    // Function to test allocation and de-allocation of page tables
    bool testPageTableAllocation();


}  // namespace PalmyraOS::Tests::Paging

namespace PalmyraOS::Tests::Heap {
    // Function to check paging boundaries and test page fault handling
    bool testHeapAllocation();

    // Assures that writing to or reading from nullptr causes a page fault
    bool testHeapCoalescence();

}  // namespace PalmyraOS::Tests::Heap