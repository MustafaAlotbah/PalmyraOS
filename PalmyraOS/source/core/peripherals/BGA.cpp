#include "core/peripherals/BGA.h"
#include "core/peripherals/Logger.h"

// ============================================================================
// Static Member Variable Definitions
// ============================================================================

PalmyraOS::kernel::ports::WordPort PalmyraOS::kernel::BGA::indexPort_(0x01CE);
PalmyraOS::kernel::ports::WordPort PalmyraOS::kernel::BGA::dataPort_(0x01CF);
bool PalmyraOS::kernel::BGA::isInitialized_     = false;
bool PalmyraOS::kernel::BGA::isAvailable_       = false;
uint16_t PalmyraOS::kernel::BGA::currentWidth_  = 0;
uint16_t PalmyraOS::kernel::BGA::currentHeight_ = 0;
uint16_t PalmyraOS::kernel::BGA::currentBpp_    = 0;

// ============================================================================
// Public Methods
// ============================================================================

bool PalmyraOS::kernel::BGA::isAvailable() {
    // Read the BGA device ID register
    uint16_t id = readRegister(VBE_DISPI_INDEX_ID);

    // Check if the ID matches any known BGA version
    if (id == VBE_DISPI_ID0 || id == VBE_DISPI_ID1 || id == VBE_DISPI_ID2 || id == VBE_DISPI_ID3 || id == VBE_DISPI_ID4 || id == VBE_DISPI_ID5) {
        LOG_DEBUG("BGA ID detected: 0x%X", id);
        return true;
    }

    LOG_DEBUG("BGA not detected. ID read: 0x%X", id);
    return false;
}

bool PalmyraOS::kernel::BGA::initialize(uint16_t width, uint16_t height, uint16_t bpp) {
    // Check if BGA is available
    if (!isAvailable()) {
        LOG_INFO("BGA Graphics Adapter not available.");
        return false;
    }

    // Mark as initialized
    isInitialized_ = true;

    // Attempt to set the resolution
    if (setResolution(width, height, bpp)) {
        LOG_INFO("BGA Graphics Adapter initialized: %dx%d @ %d bpp", width, height, bpp);
        return true;
    }
    else {
        LOG_ERROR("BGA failed to set resolution %dx%d @ %d bpp", width, height, bpp);
        return false;
    }
}

// ============================================================================
// Private Helper Methods
// ============================================================================

void PalmyraOS::kernel::BGA::writeRegister(uint16_t index, uint16_t value) {
    // Write the register index to the index port
    indexPort_.write(index);

    // Write the value to the data port
    dataPort_.write(value);
}

uint16_t PalmyraOS::kernel::BGA::readRegister(uint16_t index) {
    // Write the register index to the index port
    indexPort_.write(index);

    // Read the value from the data port
    return dataPort_.read();
}

bool PalmyraOS::kernel::BGA::setResolution(uint16_t width, uint16_t height, uint16_t bpp) {
    // Step 1: Disable the BGA adapter (prevent glitching during mode change)
    writeRegister(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);

    // Step 2: Set the X resolution (width)
    writeRegister(VBE_DISPI_INDEX_XRES, width);

    // Step 3: Set the Y resolution (height)
    writeRegister(VBE_DISPI_INDEX_YRES, height);

    // Step 4: Set the bits per pixel (color depth)
    writeRegister(VBE_DISPI_INDEX_BPP, bpp);

    // Step 5: Enable the adapter with linear framebuffer mode
    writeRegister(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    // Store the current resolution
    currentWidth_  = width;
    currentHeight_ = height;
    currentBpp_    = bpp;

    LOG_DEBUG("BGA resolution set: %dx%d @ %d bpp", width, height, bpp);

    return true;
}

// ============================================================================
// Getter Methods
// ============================================================================

uint16_t PalmyraOS::kernel::BGA::getWidth() { return currentWidth_; }

uint16_t PalmyraOS::kernel::BGA::getHeight() { return currentHeight_; }

uint16_t PalmyraOS::kernel::BGA::getBpp() { return currentBpp_; }

uint32_t PalmyraOS::kernel::BGA::getFramebufferAddress() { return BGA_FRAMEBUFFER_ADDRESS; }
