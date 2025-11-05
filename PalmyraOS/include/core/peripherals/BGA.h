/* file: PalmyraOS/core/peripherals/BGA.h
 * Bochs Graphics Adapter (BGA) Driver
 *
 * Purpose: Provides support for BGA graphics adapter detection and initialization
 * in virtualized environments (QEMU, Bochs, VirtualBox).
 *
 * This implementation allows checking if a BGA graphics adapter is available
 * on the system and provides access to VBE registers through I/O ports.
 *
 * References:
 * - Bochs VBE Extensions: http://www.bochs.org/
 * - OSDev Wiki: https://wiki.osdev.org/Bochs_Virtual_Display
 * - VBE Specification: http://www.vbe.org/
 */

#pragma once

#include "core/definitions.h"
#include "core/port.h"


namespace PalmyraOS::kernel {

    /**
     * @class BGA
     * @brief Bochs Graphics Adapter driver for virtualized graphics output.
     *
     * This class provides static methods to detect and initialize a BGA graphics
     * adapter. BGA is a paravirtualized graphics device available in QEMU, Bochs,
     * and VirtualBox, allowing graphics mode switching at runtime without BIOS
     * intervention (unlike VBE which requires bootloader setup).
     */
    class BGA {
    public:
        /**
         * @brief Check if a BGA graphics adapter is available
         *
         * Reads the BGA device ID register and compares it against known
         * valid BGA ID values (VBE_DISPI_ID0 through VBE_DISPI_ID5).
         *
         * @return true if BGA is detected and responding, false otherwise
         */
        static bool isAvailable();

        /**
         * @brief Initialize BGA driver and set graphics resolution
         *
         * This method checks if BGA is available and if so, configures the
         * graphics adapter with the specified resolution and color depth.
         *
         * @param width The desired horizontal resolution in pixels
         * @param height The desired vertical resolution in pixels
         * @param bpp Bits per pixel (typically 32 for ARGB)
         * @return true if BGA initialization was successful, false if BGA not available
         *
         * @note Should be called after checking isAvailable()
         */
        static bool initialize(uint16_t width, uint16_t height, uint16_t bpp = 32);

        /**
         * @brief Get the current graphics resolution width
         *
         * @return Current width in pixels, or 0 if not initialized
         */
        [[nodiscard]] static uint16_t getWidth();

        /**
         * @brief Get the current graphics resolution height
         *
         * @return Current height in pixels, or 0 if not initialized
         */
        [[nodiscard]] static uint16_t getHeight();

        /**
         * @brief Get the current bits per pixel setting
         *
         * @return Current BPP value, or 0 if not initialized
         */
        [[nodiscard]] static uint16_t getBpp();

        /**
         * @brief Get the default BGA framebuffer address
         *
         * For BGA in QEMU/VirtualBox, the framebuffer is typically at a specific
         * memory address. This is determined by the virtual machine configuration.
         *
         * @return The physical framebuffer address (typically 0xE0000000 or similar)
         */
        [[nodiscard]] static uint32_t getFramebufferAddress();

    private:
        // ====================================================================
        // BGA I/O Port Addresses
        // ====================================================================

        /// Index register port: Write register index here
        static constexpr uint16_t VBE_DISPI_IOPORT_INDEX      = 0x01CE;

        /// Data register port: Read/write register data here
        static constexpr uint16_t VBE_DISPI_IOPORT_DATA       = 0x01CF;

        // ====================================================================
        // BGA Framebuffer Address
        // ====================================================================

        /// Default linear framebuffer base address for BGA
        /// Commonly used by QEMU and VirtualBox
        static constexpr uint32_t BGA_FRAMEBUFFER_ADDRESS     = 0xE0000000;

        // ====================================================================
        // VBE Register Indices (used with IOPORT_INDEX)
        // ====================================================================

        /// Register 0: Device ID - identifies BGA hardware version
        static constexpr uint16_t VBE_DISPI_INDEX_ID          = 0;

        /// Register 1: X Resolution in pixels
        static constexpr uint16_t VBE_DISPI_INDEX_XRES        = 1;

        /// Register 2: Y Resolution in pixels
        static constexpr uint16_t VBE_DISPI_INDEX_YRES        = 2;

        /// Register 3: Bits per pixel (8, 15, 16, 24, 32)
        static constexpr uint16_t VBE_DISPI_INDEX_BPP         = 3;

        /// Register 4: Enable flags (see VBE_DISPI_* flags below)
        static constexpr uint16_t VBE_DISPI_INDEX_ENABLE      = 4;

        /// Register 5: Bank number (for windowed mode)
        static constexpr uint16_t VBE_DISPI_INDEX_BANK        = 5;

        /// Register 6: Virtual width in pixels
        static constexpr uint16_t VBE_DISPI_INDEX_VIRT_WIDTH  = 6;

        /// Register 7: Virtual height in pixels
        static constexpr uint16_t VBE_DISPI_INDEX_VIRT_HEIGHT = 7;

        /// Register 8: X offset for panning
        static constexpr uint16_t VBE_DISPI_INDEX_X_OFFSET    = 8;

        /// Register 9: Y offset for panning
        static constexpr uint16_t VBE_DISPI_INDEX_Y_OFFSET    = 9;

        // ====================================================================
        // BGA Device ID Values (read from VBE_DISPI_INDEX_ID register)
        // ====================================================================

        /// BGA version 0 identifier
        static constexpr uint16_t VBE_DISPI_ID0               = 0xB0C0;

        /// BGA version 1 identifier
        static constexpr uint16_t VBE_DISPI_ID1               = 0xB0C1;

        /// BGA version 2 identifier
        static constexpr uint16_t VBE_DISPI_ID2               = 0xB0C2;

        /// BGA version 3 identifier
        static constexpr uint16_t VBE_DISPI_ID3               = 0xB0C3;

        /// BGA version 4 identifier
        static constexpr uint16_t VBE_DISPI_ID4               = 0xB0C4;

        /// BGA version 5 identifier
        static constexpr uint16_t VBE_DISPI_ID5               = 0xB0C5;

        // ====================================================================
        // BGA Enable Register Flags (for VBE_DISPI_INDEX_ENABLE)
        // ====================================================================

        /// Disable BGA adapter
        static constexpr uint16_t VBE_DISPI_DISABLED          = 0x00;

        /// Enable BGA adapter
        static constexpr uint16_t VBE_DISPI_ENABLED           = 0x01;

        /// Enable linear framebuffer mode
        static constexpr uint16_t VBE_DISPI_LFB_ENABLED       = 0x40;

        // ====================================================================
        // Hardware Port Abstractions
        // ====================================================================

        /// Index register port object for selecting VBE registers
        static ports::WordPort indexPort_;

        /// Data register port object for reading/writing VBE register values
        static ports::WordPort dataPort_;

        // ====================================================================
        // Helper Methods (private)
        // ====================================================================

        /**
         * @brief Write a value to a BGA register
         *
         * @param index The VBE register index (0-9, see VBE_DISPI_INDEX_* constants)
         * @param value The 16-bit value to write to the register
         *
         * Implementation:
         * 1. Write index to IOPORT_INDEX
         * 2. Write value to IOPORT_DATA
         */
        static void writeRegister(uint16_t index, uint16_t value);

        /**
         * @brief Read a value from a BGA register
         *
         * @param index The VBE register index (0-9, see VBE_DISPI_INDEX_* constants)
         * @return The 16-bit value read from the register
         *
         * Implementation:
         * 1. Write index to IOPORT_INDEX
         * 2. Read value from IOPORT_DATA
         */
        static uint16_t readRegister(uint16_t index);

        /**
         * @brief Set the graphics resolution and color depth
         *
         * Configures the BGA adapter with the specified resolution.
         * Sequence:
         * 1. Disable the adapter
         * 2. Set X resolution
         * 3. Set Y resolution
         * 4. Set bits per pixel
         * 5. Enable with linear framebuffer
         *
         * @param width Horizontal resolution in pixels
         * @param height Vertical resolution in pixels
         * @param bpp Bits per pixel (typically 32)
         * @return true if successful, false otherwise
         */
        static bool setResolution(uint16_t width, uint16_t height, uint16_t bpp);

        // ====================================================================
        // State Variables (private)
        // ====================================================================

        /// Tracks if driver has been initialized
        static bool isInitialized_;

        /// Cached result: true if BGA is available on this system
        static bool isAvailable_;

        /// Current width in pixels
        static uint16_t currentWidth_;

        /// Current height in pixels
        static uint16_t currentHeight_;

        /// Current bits per pixel
        static uint16_t currentBpp_;
    };

}  // namespace PalmyraOS::kernel
