#pragma once

#include "core/definitions.h"

namespace PalmyraOS::kernel {

    /**
     * @brief PCI Express Configuration Space Manager
     *
     * Provides access to PCI Express devices through Memory-Mapped Configuration Space (ECAM).
     * Uses ACPI MCFG table to locate configuration space base addresses.
     */
    class PCIe {
    public:
        /**
         * @brief PCI Configuration Space Header (Type 0)
         */
        struct ConfigHeader {
            uint16_t vendorID;
            uint16_t deviceID;
            uint16_t command;
            uint16_t status;
            uint8_t revisionID;
            uint8_t progIF;
            uint8_t subclass;
            uint8_t classCode;
            uint8_t cacheLineSize;
            uint8_t latencyTimer;
            uint8_t headerType;
            uint8_t BIST;
            uint32_t BAR[6];
            uint32_t cardbusCISPointer;
            uint16_t subsystemVendorID;
            uint16_t subsystemID;
            uint32_t expansionROMBaseAddress;
            uint8_t capabilitiesPointer;
            uint8_t reserved[7];
            uint8_t interruptLine;
            uint8_t interruptPIN;
            uint8_t minGrant;
            uint8_t maxLatency;
        } __attribute__((packed));

        /**
         * @brief Initialize PCIe subsystem
         *
         * Reads MCFG table from ACPI and sets up configuration space access.
         *
         * @return True if initialization successful, false otherwise
         */
        static bool initialize();

        /**
         * @brief Check if PCIe is initialized
         */
        [[nodiscard]] static bool isInitialized() { return initialized_; }

        /**
         * @brief Read a 32-bit value from PCI configuration space
         *
         * @param bus Bus number
         * @param device Device number (0-31)
         * @param function Function number (0-7)
         * @param offset Register offset (must be 4-byte aligned)
         * @return 32-bit configuration value, or 0xFFFFFFFF if device doesn't exist
         */
        [[nodiscard]] static uint32_t readConfig32(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset);

        /**
         * @brief Write a 32-bit value to PCI configuration space
         *
         * @param bus Bus number
         * @param device Device number (0-31)
         * @param function Function number (0-7)
         * @param offset Register offset (must be 4-byte aligned)
         * @param value Value to write
         */
        static void writeConfig32(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint32_t value);

        /**
         * @brief Read a 16-bit value from PCI configuration space
         */
        [[nodiscard]] static uint16_t readConfig16(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset);

        /**
         * @brief Write a 16-bit value to PCI configuration space
         */
        static void writeConfig16(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint16_t value);

        /**
         * @brief Read an 8-bit value from PCI configuration space
         */
        [[nodiscard]] static uint8_t readConfig8(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset);

        /**
         * @brief Write an 8-bit value to PCI configuration space
         */
        static void writeConfig8(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint8_t value);

        /**
         * @brief Check if a device exists
         *
         * @param bus Bus number
         * @param device Device number (0-31)
         * @param function Function number (0-7)
         * @return True if device exists (vendor ID != 0xFFFF), false otherwise
         */
        [[nodiscard]] static bool deviceExists(uint8_t bus, uint8_t device, uint8_t function);

        /**
         * @brief Enumerate all PCI Express devices
         *
         * Scans all buses, devices, and functions, logging found devices.
         */
        static void enumerateDevices();

        /**
         * @brief Get number of discovered devices
         */
        [[nodiscard]] static uint32_t getDeviceCount() { return deviceCount_; }

    private:
        /**
         * @brief Calculate configuration space address for a device
         *
         * @param bus Bus number
         * @param device Device number
         * @param function Function number
         * @param offset Register offset
         * @return Pointer to configuration space, or nullptr if invalid
         */
        [[nodiscard]] static volatile uint32_t* getConfigAddress(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset);

        /**
         * @brief Get device class name from class code
         */
        [[nodiscard]] static const char* getClassName(uint8_t classCode);

        // Initialization state
        static bool initialized_;

        // MCFG allocation information
        static uintptr_t baseAddress_;
        static uint16_t segmentGroup_;
        static uint8_t startBus_;
        static uint8_t endBus_;

        // Device enumeration
        static uint32_t deviceCount_;
    };

}  // namespace PalmyraOS::kernel
