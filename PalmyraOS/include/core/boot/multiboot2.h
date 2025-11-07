/*  multiboot2.h - Multiboot 2 header file for PalmyraOS  */
/*  Adapted from GRUB Multiboot 2 specification
 *  Copyright (C) 1999,2003,2007,2008,2009,2010 Free Software Foundation, Inc.
 *
 *  Integrated into PalmyraOS with modern C++ wrapper classes.
 */

#pragma once

#include "core/definitions.h"

// ============================================================================
// Multiboot 2 Constants
// ============================================================================

// How many bytes from the start of the file we search for the header
constexpr uint32_t MULTIBOOT2_SEARCH              = 32768;
constexpr uint32_t MULTIBOOT2_HEADER_ALIGN        = 8;

// The magic field should contain this in the header
constexpr uint32_t MULTIBOOT2_HEADER_MAGIC        = 0xE85250D6;

// This should be in %eax when bootloader calls kernel
constexpr uint32_t MULTIBOOT2_BOOTLOADER_MAGIC    = 0x36D76289;

// Alignment of multiboot modules and info structure
constexpr uint32_t MULTIBOOT2_MOD_ALIGN           = 0x00001000;
constexpr uint32_t MULTIBOOT2_INFO_ALIGN          = 0x00000008;
constexpr uint32_t MULTIBOOT2_TAG_ALIGN           = 8;

// Architecture types
constexpr uint32_t MULTIBOOT2_ARCHITECTURE_I386   = 0;
constexpr uint32_t MULTIBOOT2_ARCHITECTURE_MIPS32 = 4;

// ============================================================================
// Multiboot 2 Tag Types (Information provided by bootloader)
// ============================================================================

namespace PalmyraOS::kernel::Multiboot2 {

    // Tag types that appear in the Multiboot information structure
    enum class TagType : uint32_t {
        End              = 0,   // Terminator tag
        CmdLine          = 1,   // Kernel command line
        BootLoaderName   = 2,   // Boot loader name
        Module           = 3,   // Boot module
        BasicMemInfo     = 4,   // Basic memory information
        BootDevice       = 5,   // BIOS boot device
        MemoryMap        = 6,   // Memory map
        VBE              = 7,   // VBE video info
        Framebuffer      = 8,   // Framebuffer info
        ELFSections      = 9,   // ELF symbols
        APM              = 10,  // APM table
        EFI32            = 11,  // EFI 32-bit system table pointer
        EFI64            = 12,  // EFI 64-bit system table pointer
        SMBIOS           = 13,  // SMBIOS tables
        ACPIOld          = 14,  // ACPI 1.0 RSDP
        ACPINew          = 15,  // ACPI 2.0+ RSDP
        Network          = 16,  // Network information
        EFIMemoryMap     = 17,  // EFI memory map
        EFIBootServices  = 18,  // EFI boot services not terminated
        EFI32ImageHandle = 19,  // EFI 32-bit image handle
        EFI64ImageHandle = 20,  // EFI 64-bit image handle
        LoadBaseAddr     = 21   // Image load base physical address
    };

    // Header tag types (requests to bootloader)
    enum class HeaderTagType : uint16_t {
        End                = 0,  // Terminator
        InformationRequest = 1,  // Request specific tags
        Address            = 2,  // Override default addresses
        EntryAddress       = 3,  // Override entry point
        ConsoleFlags       = 4,  // Console flags
        Framebuffer        = 5,  // Request framebuffer
        ModuleAlign        = 6,  // Module alignment
        EFIBootServices    = 7,  // EFI boot services
        EntryAddressEFI64  = 9,  // EFI64 entry point
        Relocatable        = 10  // Kernel is relocatable
    };

    // Memory types in memory map
    enum class MemoryType : uint32_t {
        Available       = 1,  // Available RAM
        Reserved        = 2,  // Reserved - unavailable
        ACPIReclaimable = 3,  // ACPI information
        NVS             = 4,  // ACPI NVS memory
        BadRAM          = 5   // Bad memory
    };

    // Framebuffer types
    enum class FramebufferType : uint8_t {
        Indexed = 0,  // Indexed color
        RGB     = 1,  // Direct RGB color
        EGAText = 2   // EGA text mode
    };

    // ============================================================================
    // Multiboot 2 Structures (C-compatible, packed)
    // ============================================================================

    // Base tag structure - all tags start with this
    struct multiboot_tag {
        uint32_t type;  // TagType
        uint32_t size;  // Size of tag including this header
    } __attribute__((packed));

    typedef struct multiboot_tag multiboot_tag_t;

    // String tag (command line, bootloader name, etc.)
    struct multiboot_tag_string {
        uint32_t type;
        uint32_t size;
        char string[0];  // Flexible array member - null-terminated string
    } __attribute__((packed));

    typedef struct multiboot_tag_string multiboot_tag_string_t;

    // Boot module tag
    struct multiboot_tag_module {
        uint32_t type;
        uint32_t size;
        uint32_t mod_start;  // Physical start address
        uint32_t mod_end;    // Physical end address
        char cmdline[0];     // Module command line
    } __attribute__((packed));

    typedef struct multiboot_tag_module multiboot_tag_module_t;

    // Basic memory information tag
    struct multiboot_tag_basic_meminfo {
        uint32_t type;
        uint32_t size;
        uint32_t mem_lower;  // Amount of lower memory in kilobytes
        uint32_t mem_upper;  // Amount of upper memory in kilobytes
    } __attribute__((packed));

    typedef struct multiboot_tag_basic_meminfo multiboot_tag_basic_meminfo_t;

    // Boot device tag
    struct multiboot_tag_bootdev {
        uint32_t type;
        uint32_t size;
        uint32_t biosdev;  // BIOS drive number
        uint32_t slice;    // Top-level partition number
        uint32_t part;     // Sub-partition number
    } __attribute__((packed));

    typedef struct multiboot_tag_bootdev multiboot_tag_bootdev_t;

    // Memory map entry
    struct multiboot_mmap_entry {
        uint64_t addr;  // Starting address
        uint64_t len;   // Size of memory region
        uint32_t type;  // MemoryType
        uint32_t zero;  // Reserved (must be zero)
    } __attribute__((packed));

    typedef struct multiboot_mmap_entry multiboot_mmap_entry_t;

    // Memory map tag
    struct multiboot_tag_mmap {
        uint32_t type;
        uint32_t size;
        uint32_t entry_size;              // Size of each entry
        uint32_t entry_version;           // Version of entry structure
        multiboot_mmap_entry entries[0];  // Flexible array of entries
    } __attribute__((packed));

    typedef struct multiboot_tag_mmap multiboot_tag_mmap_t;

    // VBE info blocks (opaque external specification)
    struct multiboot_vbe_info_block {
        uint8_t external_specification[512];
    } __attribute__((packed));

    typedef struct multiboot_vbe_info_block multiboot_vbe_info_block_t;

    struct multiboot_vbe_mode_info_block {
        uint8_t external_specification[256];
    } __attribute__((packed));

    typedef struct multiboot_vbe_mode_info_block multiboot_vbe_mode_info_block_t;

    // VBE tag
    struct multiboot_tag_vbe {
        uint32_t type;
        uint32_t size;
        uint16_t vbe_mode;
        uint16_t vbe_interface_seg;
        uint16_t vbe_interface_off;
        uint16_t vbe_interface_len;
        multiboot_vbe_info_block vbe_control_info;
        multiboot_vbe_mode_info_block vbe_mode_info;
    } __attribute__((packed));

    typedef struct multiboot_tag_vbe multiboot_tag_vbe_t;

    // Color structure for indexed color modes
    struct multiboot_color {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } __attribute__((packed));

    typedef struct multiboot_color multiboot_color_t;

    // Framebuffer tag - common header
    struct multiboot_tag_framebuffer_common {
        uint32_t type;
        uint32_t size;
        uint64_t framebuffer_addr;    // Physical address of framebuffer
        uint32_t framebuffer_pitch;   // Bytes per scanline
        uint32_t framebuffer_width;   // Width in pixels
        uint32_t framebuffer_height;  // Height in pixels
        uint8_t framebuffer_bpp;      // Bits per pixel
        uint8_t framebuffer_type;     // FramebufferType
        uint16_t reserved;
    } __attribute__((packed));

    typedef struct multiboot_tag_framebuffer_common multiboot_tag_framebuffer_common_t;

    // Framebuffer tag with color info
    struct multiboot_tag_framebuffer {
        multiboot_tag_framebuffer_common common;
        union {
            struct {
                uint16_t framebuffer_palette_num_colors;
                multiboot_color framebuffer_palette[0];
            };
            struct {
                uint8_t framebuffer_red_field_position;
                uint8_t framebuffer_red_mask_size;
                uint8_t framebuffer_green_field_position;
                uint8_t framebuffer_green_mask_size;
                uint8_t framebuffer_blue_field_position;
                uint8_t framebuffer_blue_mask_size;
            };
        };
    } __attribute__((packed));

    typedef struct multiboot_tag_framebuffer multiboot_tag_framebuffer_t;

    // ELF sections tag
    struct multiboot_tag_elf_sections {
        uint32_t type;
        uint32_t size;
        uint32_t num;      // Number of section headers
        uint32_t entsize;  // Size of each section header
        uint32_t shndx;    // String table section index
        char sections[0];  // Section headers
    } __attribute__((packed));

    typedef struct multiboot_tag_elf_sections multiboot_tag_elf_sections_t;

    // APM (Advanced Power Management) tag
    struct multiboot_tag_apm {
        uint32_t type;
        uint32_t size;
        uint16_t version;
        uint16_t cseg;
        uint32_t offset;
        uint16_t cseg_16;
        uint16_t dseg;
        uint16_t flags;
        uint16_t cseg_len;
        uint16_t cseg_16_len;
        uint16_t dseg_len;
    } __attribute__((packed));

    typedef struct multiboot_tag_apm multiboot_tag_apm_t;

    // EFI 32-bit system table pointer tag
    struct multiboot_tag_efi32 {
        uint32_t type;
        uint32_t size;
        uint32_t pointer;  // Physical address of EFI system table
    } __attribute__((packed));

    typedef struct multiboot_tag_efi32 multiboot_tag_efi32_t;

    // EFI 64-bit system table pointer tag
    struct multiboot_tag_efi64 {
        uint32_t type;
        uint32_t size;
        uint64_t pointer;  // Physical address of EFI system table
    } __attribute__((packed));

    typedef struct multiboot_tag_efi64 multiboot_tag_efi64_t;

    // SMBIOS tables tag
    struct multiboot_tag_smbios {
        uint32_t type;
        uint32_t size;
        uint8_t major;  // SMBIOS major version
        uint8_t minor;  // SMBIOS minor version
        uint8_t reserved[6];
        uint8_t tables[0];  // SMBIOS tables
    } __attribute__((packed));

    typedef struct multiboot_tag_smbios multiboot_tag_smbios_t;

    // ACPI Old (1.0) RSDP tag
    struct multiboot_tag_old_acpi {
        uint32_t type;
        uint32_t size;
        uint8_t rsdp[0];  // ACPI 1.0 RSDP structure (20 bytes)
    } __attribute__((packed));

    typedef struct multiboot_tag_old_acpi multiboot_tag_old_acpi_t;

    // ACPI New (2.0+) RSDP tag
    struct multiboot_tag_new_acpi {
        uint32_t type;
        uint32_t size;
        uint8_t rsdp[0];  // ACPI 2.0+ RSDP structure (variable size)
    } __attribute__((packed));

    typedef struct multiboot_tag_new_acpi multiboot_tag_new_acpi_t;

    // Network information tag
    struct multiboot_tag_network {
        uint32_t type;
        uint32_t size;
        uint8_t dhcpack[0];  // DHCP ACK packet
    } __attribute__((packed));

    typedef struct multiboot_tag_network multiboot_tag_network_t;

    // EFI memory map tag
    struct multiboot_tag_efi_mmap {
        uint32_t type;
        uint32_t size;
        uint32_t descr_size;  // Size of each descriptor
        uint32_t descr_vers;  // Descriptor version
        uint8_t efi_mmap[0];  // EFI memory descriptors
    } __attribute__((packed));

    typedef struct multiboot_tag_efi_mmap multiboot_tag_efi_mmap_t;

    // EFI 32-bit image handle tag
    struct multiboot_tag_efi32_ih {
        uint32_t type;
        uint32_t size;
        uint32_t pointer;  // EFI image handle
    } __attribute__((packed));

    typedef struct multiboot_tag_efi32_ih multiboot_tag_efi32_ih_t;

    // EFI 64-bit image handle tag
    struct multiboot_tag_efi64_ih {
        uint32_t type;
        uint32_t size;
        uint64_t pointer;  // EFI image handle
    } __attribute__((packed));

    typedef struct multiboot_tag_efi64_ih multiboot_tag_efi64_ih_t;

    // Load base address tag
    struct multiboot_tag_load_base_addr {
        uint32_t type;
        uint32_t size;
        uint32_t load_base_addr;  // Physical load address of kernel
    } __attribute__((packed));

    typedef struct multiboot_tag_load_base_addr multiboot_tag_load_base_addr_t;

    // ============================================================================
    // Modern C++ Wrapper Classes
    // ============================================================================

    /**
     * @brief Multiboot 2 Information Structure Parser
     *
     * Provides a modern C++ interface to parse and access Multiboot 2 boot information
     * provided by the bootloader. Uses a tag-based approach for extensibility.
     */
    class MultibootInfo {
    public:
        /**
         * @brief Construct a MultibootInfo parser from the bootloader-provided address
         * @param addr Physical address of Multiboot 2 info structure
         */
        explicit MultibootInfo(uint32_t addr);

        /**
         * @brief Check if Multiboot 2 info is valid
         * @return True if the info structure is properly initialized
         */
        [[nodiscard]] bool isValid() const { return addr_ != 0 && totalSize_ >= 8; }

        /**
         * @brief Get total size of Multiboot info structure
         * @return Size in bytes
         */
        [[nodiscard]] uint32_t getTotalSize() const { return totalSize_; }

        /**
         * @brief Find a specific tag by type
         * @param type Tag type to search for
         * @return Pointer to tag if found, nullptr otherwise
         */
        [[nodiscard]] const multiboot_tag* findTag(TagType type) const;

        /**
         * @brief Find a typed tag (convenience template)
         * @tparam T Tag structure type
         * @param type Tag type to search for
         * @return Pointer to typed tag if found, nullptr otherwise
         */
        template<typename T>
        [[nodiscard]] const T* findTagTyped(TagType type) const {
            return reinterpret_cast<const T*>(findTag(type));
        }

        /**
         * @brief Iterate over all tags
         * @tparam Func Functor type: void(const multiboot_tag*)
         * @param func Function to call for each tag
         */
        template<typename Func>
        void forEachTag(Func func) const {
            const multiboot_tag* tag = getFirstTag();
            while (tag && static_cast<TagType>(tag->type) != TagType::End) {
                func(tag);
                tag = getNextTag(tag);
            }
        }

        // ========== Convenience Accessors ==========

        /**
         * @brief Get basic memory information
         * @return Pointer to memory info tag, or nullptr if not present
         */
        [[nodiscard]] const multiboot_tag_basic_meminfo* getBasicMemInfo() const { return findTagTyped<multiboot_tag_basic_meminfo>(TagType::BasicMemInfo); }

        /**
         * @brief Get framebuffer information
         * @return Pointer to framebuffer tag, or nullptr if not present
         */
        [[nodiscard]] const multiboot_tag_framebuffer* getFramebuffer() const { return findTagTyped<multiboot_tag_framebuffer>(TagType::Framebuffer); }

        /**
         * @brief Get VBE information
         * @return Pointer to VBE tag, or nullptr if not present
         */
        [[nodiscard]] const multiboot_tag_vbe* getVBE() const { return findTagTyped<multiboot_tag_vbe>(TagType::VBE); }

        /**
         * @brief Get memory map
         * @return Pointer to memory map tag, or nullptr if not present
         */
        [[nodiscard]] const multiboot_tag_mmap* getMemoryMap() const { return findTagTyped<multiboot_tag_mmap>(TagType::MemoryMap); }

        /**
         * @brief Get ACPI RSDP (tries new version first, falls back to old)
         * @return Pointer to RSDP structure, or nullptr if not present
         */
        [[nodiscard]] const uint8_t* getACPIRSDP() const;

        /**
         * @brief Get command line string
         * @return Command line string, or nullptr if not present
         */
        [[nodiscard]] const char* getCommandLine() const {
            const auto* tag = findTagTyped<multiboot_tag_string>(TagType::CmdLine);
            return tag ? tag->string : nullptr;
        }

        /**
         * @brief Get bootloader name string
         * @return Bootloader name, or nullptr if not present
         */
        [[nodiscard]] const char* getBootLoaderName() const {
            const auto* tag = findTagTyped<multiboot_tag_string>(TagType::BootLoaderName);
            return tag ? tag->string : nullptr;
        }

        /**
         * @brief Get load base address
         * @return Physical load address, or 0 if not present
         */
        [[nodiscard]] uint32_t getLoadBaseAddr() const {
            const auto* tag = findTagTyped<multiboot_tag_load_base_addr>(TagType::LoadBaseAddr);
            return tag ? tag->load_base_addr : 0;
        }

    private:
        /**
         * @brief Get pointer to first tag in the structure
         * @return Pointer to first tag after header
         */
        [[nodiscard]] const multiboot_tag* getFirstTag() const;

        /**
         * @brief Get next tag in the sequence
         * @param tag Current tag
         * @return Pointer to next tag (8-byte aligned)
         */
        [[nodiscard]] static const multiboot_tag* getNextTag(const multiboot_tag* tag);

        /**
         * @brief Align address to 8-byte boundary
         * @param addr Address to align
         * @return Aligned address
         */
        [[nodiscard]] static constexpr uint32_t alignUp8(uint32_t addr) { return (addr + 7) & ~7u; }

        uint32_t addr_;       // Physical address of Multiboot info structure
        uint32_t totalSize_;  // Total size of structure
        uint32_t reserved_;   // Reserved field (must be 0)
    };

    /**
     * @brief Check if magic number indicates Multiboot 2 bootloader
     * @param magic Magic number from EAX register
     * @return True if valid Multiboot 2 magic
     */
    [[nodiscard]] inline bool isMultiboot2(uint32_t magic) { return magic == MULTIBOOT2_BOOTLOADER_MAGIC; }

    /**
     * @brief Log detailed Multiboot 2 information
     * @param info Multiboot 2 information structure
     */
    void logMultiboot2Info(const MultibootInfo& info);

}  // namespace PalmyraOS::kernel::Multiboot2
