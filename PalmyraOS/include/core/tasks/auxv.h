/**
 * @file auxv.h
 * @brief Auxiliary vector definitions for ELF process initialization
 *
 * The auxiliary vector (auxv) is a mechanism for the kernel to pass information
 * to user-space programs at process startup. It provides essential metadata about
 * the system and the loaded ELF binary that dynamic linkers and C runtime libraries
 * need to function correctly.
 *
 * This implementation follows the Linux i386 ABI specification for compatibility
 * with standard toolchains and dynamically-linked executables.
 *
 * NOTE: The AT_* constants (AT_NULL, AT_PHDR, AT_ENTRY, etc.) are defined in <elf.h>
 * and are automatically available when elf.h is included. We don't redefine them here
 * to avoid conflicts with the system definitions.
 *
 * Common AT_* constants available from <elf.h>:
 *   - AT_NULL    (0)  : End of auxiliary vector
 *   - AT_PHDR    (3)  : Program headers address
 *   - AT_PHENT   (4)  : Size of program header entry
 *   - AT_PHNUM   (5)  : Number of program headers
 *   - AT_PAGESZ  (6)  : System page size
 *   - AT_BASE    (7)  : Interpreter base address
 *   - AT_ENTRY   (9)  : Entry point address
 *   - AT_UID     (11) : Real user ID
 *   - AT_EUID    (12) : Effective user ID
 *   - AT_GID     (13) : Real group ID
 *   - AT_EGID    (14) : Effective group ID
 *   - AT_CLKTCK  (17) : Clock tick frequency
 *   - AT_SECURE  (23) : Secure mode flag
 *   - AT_RANDOM  (25) : Random bytes address
 *   - AT_EXECFN  (31) : Executable filename
 */

#pragma once

#include <cstdint>

namespace PalmyraOS::kernel {


    /**
     * @struct AuxiliaryVectorEntry
     * @brief Single entry in the auxiliary vector
     *
     * Each entry is a key-value pair where:
     * - type: One of the AT_* constants above
     * - value: The corresponding value (address, size, flag, etc.)
     *
     * The auxiliary vector is terminated by an entry with type=AT_NULL and value=0.
     */
    struct AuxiliaryVectorEntry {
        uint32_t type;   ///< AT_* constant identifying the entry type
        uint32_t value;  ///< The value associated with this entry
    };


}  // namespace PalmyraOS::kernel
