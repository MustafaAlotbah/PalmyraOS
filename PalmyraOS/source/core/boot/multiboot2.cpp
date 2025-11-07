
#include "core/boot/multiboot2.h"
#include "core/peripherals/Logger.h"
#include <cstring>

using namespace PalmyraOS::kernel::Multiboot2;

// ============================================================================
// MultibootInfo Implementation
// ============================================================================

MultibootInfo::MultibootInfo(uint32_t addr) : addr_(addr), totalSize_(0), reserved_(0) {
    if (addr == 0) {
        LOG_ERROR("Multiboot2: Invalid address (nullptr)");
        return;
    }

    // Read total size and reserved fields from beginning of structure
    // Structure layout: [uint32_t total_size][uint32_t reserved][tags...]
    totalSize_ = *reinterpret_cast<uint32_t*>(addr);
    reserved_  = *reinterpret_cast<uint32_t*>(addr + 4);

    if (totalSize_ < 8) {
        LOG_ERROR("Multiboot2: Invalid total size %u (must be >= 8)", totalSize_);
        totalSize_ = 0;
        return;
    }

    if (reserved_ != 0) { LOG_WARN("Multiboot2: Reserved field is non-zero (%u), should be 0", reserved_); }

    LOG_INFO("Multiboot2: Info structure at 0x%X, size=%u bytes", addr_, totalSize_);
}

const multiboot_tag* MultibootInfo::getFirstTag() const {
    if (!isValid()) { return nullptr; }

    // First tag starts after the 8-byte header (totalSize + reserved)
    return reinterpret_cast<const multiboot_tag*>(addr_ + 8);
}

const multiboot_tag* MultibootInfo::getNextTag(const multiboot_tag* tag) {
    if (!tag) { return nullptr; }

    // Tags must be 8-byte aligned
    uint32_t nextAddr = reinterpret_cast<uint32_t>(tag) + tag->size;
    nextAddr          = alignUp8(nextAddr);

    return reinterpret_cast<const multiboot_tag*>(nextAddr);
}

const multiboot_tag* MultibootInfo::findTag(TagType type) const {
    if (!isValid()) { return nullptr; }

    const multiboot_tag* tag = getFirstTag();
    const uint32_t endAddr   = addr_ + totalSize_;

    while (tag) {
        // Safety check: ensure tag is within bounds
        uint32_t tagAddr = reinterpret_cast<uint32_t>(tag);
        if (tagAddr >= endAddr || tagAddr + sizeof(multiboot_tag) > endAddr) {
            LOG_ERROR("Multiboot2: Tag at 0x%X exceeds info structure bounds", tagAddr);
            break;
        }

        // Check for end tag
        if (static_cast<TagType>(tag->type) == TagType::End) { break; }

        // Check if this is the tag we're looking for
        if (static_cast<TagType>(tag->type) == type) { return tag; }

        // Move to next tag
        tag = getNextTag(tag);
    }

    return nullptr;
}

const uint8_t* MultibootInfo::getACPIRSDP() const {
    // Try ACPI 2.0+ first (preferred)
    const auto* acpiNew = findTagTyped<multiboot_tag_new_acpi>(TagType::ACPINew);
    if (acpiNew) {
        LOG_DEBUG("Multiboot2: Found ACPI 2.0+ RSDP");
        return acpiNew->rsdp;
    }

    // Fall back to ACPI 1.0
    const auto* acpiOld = findTagTyped<multiboot_tag_old_acpi>(TagType::ACPIOld);
    if (acpiOld) {
        LOG_DEBUG("Multiboot2: Found ACPI 1.0 RSDP");
        return acpiOld->rsdp;
    }

    LOG_DEBUG("Multiboot2: No ACPI RSDP found");
    return nullptr;
}

// ============================================================================
// Logging Utility
// ============================================================================

void PalmyraOS::kernel::Multiboot2::logMultiboot2Info(const MultibootInfo& info) {
    LOG_INFO("================================================");
    LOG_INFO("Multiboot 2 Information");
    LOG_INFO("================================================");
    LOG_INFO("Total Size: %u bytes", info.getTotalSize());

    // Basic Memory Info
    const auto* memInfo = info.getBasicMemInfo();
    if (memInfo) {
        LOG_INFO("Memory:");
        LOG_INFO("  Lower: %u KB", memInfo->mem_lower);
        LOG_INFO("  Upper: %u KB (%u MB)", memInfo->mem_upper, memInfo->mem_upper / 1024);
    }

    // Boot Command Line
    const char* cmdline = info.getCommandLine();
    if (cmdline) { LOG_INFO("Command Line: %s", cmdline); }

    // Bootloader Name
    const char* blName = info.getBootLoaderName();
    if (blName) { LOG_INFO("Bootloader: %s", blName); }

    // Framebuffer Info
    const auto* fbInfo = info.getFramebuffer();
    if (fbInfo) {
        LOG_INFO("Framebuffer:");
        LOG_INFO("  Resolution: %ux%u @ %u bpp", fbInfo->common.framebuffer_width, fbInfo->common.framebuffer_height, fbInfo->common.framebuffer_bpp);
        LOG_INFO("  Address: 0x%llX", fbInfo->common.framebuffer_addr);
        LOG_INFO("  Pitch: %u bytes", fbInfo->common.framebuffer_pitch);
        LOG_INFO("  Type: %s",
                 fbInfo->common.framebuffer_type == static_cast<uint8_t>(FramebufferType::Indexed)   ? "Indexed"
                 : fbInfo->common.framebuffer_type == static_cast<uint8_t>(FramebufferType::RGB)     ? "RGB"
                 : fbInfo->common.framebuffer_type == static_cast<uint8_t>(FramebufferType::EGAText) ? "EGA Text"
                                                                                                     : "Unknown");
    }

    // VBE Info
    const auto* vbeInfo = info.getVBE();
    if (vbeInfo) {
        LOG_INFO("VBE:");
        LOG_INFO("  Mode: 0x%X", vbeInfo->vbe_mode);
        LOG_INFO("  Interface: Seg=0x%X Off=0x%X Len=%u", vbeInfo->vbe_interface_seg, vbeInfo->vbe_interface_off, vbeInfo->vbe_interface_len);
    }

    // Memory Map
    const auto* mmapTag = info.getMemoryMap();
    if (mmapTag) {
        uint32_t entryCount = (mmapTag->size - sizeof(multiboot_tag_mmap)) / mmapTag->entry_size;
        LOG_INFO("Memory Map: %u entries (entry_size=%u)", entryCount, mmapTag->entry_size);

        for (uint32_t i = 0; i < entryCount && i < 5; ++i) {
            const auto& entry = mmapTag->entries[i];
            LOG_INFO("  [%u] Base: 0x%016llX Length: 0x%016llX Type: %s",
                     i,
                     entry.addr,
                     entry.len,
                     entry.type == static_cast<uint32_t>(MemoryType::Available)         ? "Available"
                     : entry.type == static_cast<uint32_t>(MemoryType::Reserved)        ? "Reserved"
                     : entry.type == static_cast<uint32_t>(MemoryType::ACPIReclaimable) ? "ACPI Reclaimable"
                     : entry.type == static_cast<uint32_t>(MemoryType::NVS)             ? "NVS"
                     : entry.type == static_cast<uint32_t>(MemoryType::BadRAM)          ? "Bad RAM"
                                                                                        : "Unknown");
        }
        if (entryCount > 5) { LOG_INFO("  ... (%u more entries)", entryCount - 5); }
    }

    // ACPI RSDP
    const uint8_t* acpiRSDP = info.getACPIRSDP();
    if (acpiRSDP) { LOG_INFO("ACPI RSDP: 0x%p", acpiRSDP); }

    // EFI System Table
    const auto* efi32 = info.findTagTyped<multiboot_tag_efi32>(TagType::EFI32);
    const auto* efi64 = info.findTagTyped<multiboot_tag_efi64>(TagType::EFI64);
    if (efi32) { LOG_INFO("EFI32 System Table: 0x%X", efi32->pointer); }
    if (efi64) { LOG_INFO("EFI64 System Table: 0x%llX", efi64->pointer); }

    LOG_INFO("================================================");
}
