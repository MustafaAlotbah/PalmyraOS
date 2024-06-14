
#pragma once

#include "core/definitions.h"
#include "core/Interrupts.h"
#include "PhysicalMemory.h"


namespace PalmyraOS::kernel
{
  // types
  /**
   * @brief Type definition for a Page Fault Handler function.
   *
   * A Page Fault Handler function is called when a page fault occurs.
   *
   * @param regs Pointer to the CPU registers at the time of the fault.
   * @param faultingAddress The address that caused the fault.
   * @param isPresent Whether the page is present in memory.
   * @param isWrite Whether the fault was caused by a write operation.
   * @param isUserMode Whether the fault occurred in user mode.
   * @param instructionFetch Whether the fault was caused by an instruction fetch.
   */
  typedef void
  (* PageFaultHandler)(
	  interrupts::CPURegisters* regs,
	  uint32_t faultingAddress,
	  bool isPresent,
	  bool isWrite,
	  bool isUserMode,
	  bool instructionFetch
  );

/**
 * @brief Structure representing a Page Directory Entry
 *
 * A Page Directory Entry (PDE) points to a Page Table.
 */
  struct PageDirectoryEntry
  {
	  uint32_t present: 1;         ///< Page present in memory
	  uint32_t rw: 1;              ///< Read/Write permission
	  uint32_t user: 1;            ///< User/Supervisor level
	  uint32_t writeThrough: 1;    ///< Write-through caching
	  uint32_t cacheDisabled: 1;   ///< Cache disable
	  uint32_t accessed: 1;        ///< Accessed
	  uint32_t reserved: 1;        ///< Reserved
	  uint32_t pageSize: 1;        ///< Page size (0 for 4KB)
	  uint32_t ignored: 1;         ///< Ignored
	  uint32_t available: 3;       ///< Available for system programmer
	  uint32_t tableAddress: 20;   ///< Physical address of the page table (aligned)
  } __attribute__((packed));

/**
 * @brief Structure representing a Page Table Entry
 *
 * A Page Table Entry (PTE) maps a virtual address to a physical address.
 */
  struct PageTableEntry
  {
	  uint32_t present: 1;         ///< Page present in memory
	  uint32_t rw: 1;              ///< Read/Write permission
	  uint32_t user: 1;            ///< User/Supervisor level
	  uint32_t writeThrough: 1;    ///< Write-through caching
	  uint32_t cacheDisabled: 1;   ///< Cache disable
	  uint32_t accessed: 1;        ///< Accessed
	  uint32_t dirty: 1;           ///< Dirty
	  uint32_t attributeIndex: 1;  ///< Attribute index
	  uint32_t global: 1;          ///< Global page
	  uint32_t available: 3;       ///< Available for system programmer
	  uint32_t physicalAddress: 20;///< Physical address of the frame (aligned)
  } __attribute__((packed));

/**
 * @brief Class representing a Paging Directory
 *
 * This class manages the paging mechanism, including setting up page tables and mapping pages.
 */
  class PagingDirectory
  {
   public:
	  /**
	   * @brief Constructs a PagingDirectory object
	   *
	   * Initializes page tables and sets up the page directory.
	   */
	  PagingDirectory();

	  /**
	   * @brief Destructs the PagingDirectory object
	   *
	   * Frees allocated page tables.
	   */
	  void destruct();

	  /**
	   * @brief Allocates a page and returns its virtual address
	   * @return void* Pointer to the allocated page
	   */
	  void* allocatePage();                // returns virtual address

	  /**
	   * @brief Allocates multiple pages and returns the starting virtual address
	   * @param numPages Number of pages to allocate
	   * @return void* Pointer to the starting virtual address of the allocated pages
	   */
	  void* allocatePages(size_t numPages);                // returns virtual address

	  /**
	   * @brief Frees a page given its virtual address
	   * @param pageAddress Virtual address of the page to free
	   */
	  void freePage(void* pageAddress);     // give it virtual address

	  /**
	   * @brief Returns the page directory
	   * @return uint32_t* Pointer to the page directory
	   */
	  [[nodiscard]] uint32_t* getDirectory() const;

	  /**
	   * @brief Returns the number of allocated pages
	   * @return uint32_t Number of allocated pages
	   */
	  [[nodiscard]] inline uint32_t getNumAllocatedPages() const
	  { return pagesCount_; };
   public:
	  /**
	   * @brief Maps a physical address to a virtual address with given flags
	   * @param physicalAddr Physical address
	   * @param virtualAddr Virtual address
	   * @param flags Flags for page table entry
	   */
	  void mapPage(void* physicalAddr, void* virtualAddr, uint32_t flags);

	  /**
	   * @brief Unmaps a virtual address
	   * @param virtualAddr Virtual address to unmap
	   */
	  void unmapPage(void* virtualAddr);

	  REMOVE_COPY(PagingDirectory);
   private:
	  /**
	   * @brief Sets a page table in the directory
	   * @param tableIndex Index of the table
	   * @param tableAddress Address of the table
	   * @param flags Flags for the table entry
	   */
	  void setTable(uint32_t tableIndex, uint32_t tableAddress, uint32_t flags);

	  /**
	   * @brief Sets a page in a table
	   * @param table Pointer to the table
	   * @param pageIndex Index of the page
	   * @param physicalAddr Physical address of the page
	   * @param flags Flags for the page entry
	   */
	  void setPage(uint32_t* table, uint32_t pageIndex, uint32_t physicalAddr, uint32_t flags);

	  /**
	   * @brief Gets a page table by index
	   * @param tableIndex Index of the table
	   * @return uint32_t* Pointer to the page table
	   */
	  uint32_t* getTable(uint32_t tableIndex);


   private:
	  PageTableEntry* pageTables_[NUM_ENTRIES]{};          ///< Array of pointers to page tables
	  PageDirectoryEntry pageDirectory_[NUM_ENTRIES]{};    ///< Page directory
	  uint32_t           physicalAddress_{ 0 };            ///< Physical address of the directory
	  uint32_t           pagesCount_{ 0 };                 ///< Number of pages allocated
  };

/**
 * @brief Class for managing paging
 *
 * Handles initialization, switching page directories, and handling page faults.
 */
  class PagingManager
  {
   public:
	  /**
	   * @brief Initializes the paging system
	   */
	  static void initialize();

	  /**
	   * @brief Switches to a new page directory
	   * @param newPageDirectory Pointer to the new page directory
	   */
	  static void switchPageDirectory(PagingDirectory* newPageDirectory);

	  /**
	   * @brief Allocates a page
	   * @return void* Pointer to the allocated page
	   */
	  static void* allocatePage();

	  /**
	   * @brief Allocates multiple pages
	   * @param numPages Number of pages to allocate
	   * @return void* Pointer to the starting virtual address of the allocated pages
	   */
	  static void* allocatePages(size_t numPages);

	  /**
	   * @brief Frees a page given its virtual address
	   * @param pageAddress Virtual address of the page to free
	   */
	  static void freePage(void* pageAddress);

	  /**
	   * @brief Creates a new page directory
	   * @return PagingDirectory* Pointer to the created page directory
	   */
	  static PagingDirectory* createPageDirectory();

	  /**
	   * @brief Returns the current page directory
	   * @return PagingDirectory* Pointer to the current page directory
	   */
	  [[nodiscard]] static PagingDirectory* getCurrentPageDirectory();
   public:

	  /**
	   * @brief Handles page fault interrupts
	   * @param regs Pointer to CPU registers at the time of the fault
	   */
	  static uint32_t* handlePageFault(interrupts::CPURegisters* regs);

	  /**
	   * @brief Sets a secondary page fault handler
	   * @param handler Pointer to the secondary page fault handler function
	   */
	  static void setSecondaryPageFaultHandler(PageFaultHandler handler);

   private:
	  static PagingDirectory* currentPageDirectory_;    ///< Pointer to the current page directory
	  static PageFaultHandler secondaryHandler_;        ///< Pointer to the secondary page fault handler
  };

}





















