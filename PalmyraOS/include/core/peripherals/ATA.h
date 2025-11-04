/* file: PalmyraOS/core/peripherals/ATA.h
 * Advanced Technology Attachment (ATA) Driver
 *
 * Purpose: Provides a robust ATA interface for managing hard drive operations
 * including device identification, sector reading, and writing.
 *
 * This implementation supports both master and slave devices, and handles
 * LBA28 addressing mode. It's designed to be extensible for future
 * improvements like LBA48 support.
 *
 * References:
 * - ATA/ATAPI specification: http://www.t13.org/
 * - OSDev Wiki: https://wiki.osdev.org/ATA_PIO_Mode
 * - ATA Command: https://wiki.osdev.org/ATA_Command_Matrix
 */

#pragma once

#include "core/port.h"


namespace PalmyraOS::kernel {
    // Transfer Modes:
    // DMA (Direct Memory Access)
    // PIO (Programmed I/O)

    class ATA {
    public:
        /**
         * @brief Enum class to specify the device type (Master or Slave)
         */
        enum class Type { Master, Slave };

        /**
         * @brief Converts DeviceType enum to a const char* string representation
         * @param type The DeviceType enum value
         * @return Corresponding const char* string representation
         */
        static const char* toString(Type type);

        /**
         * @brief Enum class for ATA commands
         */
        enum class Command { Identify = 0xEC, ReadSectors = 0x20, WriteSectors = 0x30 };

        /**
         * @brief Constructor to initialize ATA device with given port base and device type
         * @param portBase Base I/O port address for the ATA device
         * @param deviceType Specifies whether the device is master or slave
         */
        ATA(uint16_t portBase, Type deviceType);

        /**
         * @brief Identify the ATA device and extract its information
         */
        bool identify(uint32_t timeout);

        /**
         * @brief Read a sector from the ATA device
         * @param logicalBlockAddress Logical Block Address (LBA) of the sector to read
         * @param buffer Pointer to the buffer where the read data will be stored
         * @return true if the read operation was successful, false otherwise
         */
        bool readSector(uint32_t logicalBlockAddress, uint8_t* buffer, uint32_t timeout);  // TODO return KVector<uint8_t>

        /**
         * @brief Write a sector to the ATA device
         * @param logicalBlockAddress Logical Block Address of the sector to write
         * @param buffer Pointer to the buffer containing the data to write
         * @return true if the write operation was successful, false otherwise
         */
        bool writeSector(uint32_t logicalBlockAddress, const uint8_t* buffer,
                         uint32_t timeout);  // TODO take KVector<uint8_t>

        /**
         * @brief Check if the ATA device is present
         * @return true if the device is present, false otherwise
         */
        bool isDevicePresent();

        // Getters
        [[nodiscard]] const char* getSerialNumber() const;
        [[nodiscard]] const char* getFirmwareVersion() const;
        [[nodiscard]] const char* getModelNumber() const;
        [[nodiscard]] uint64_t getStorageSize() const;
        [[nodiscard]] uint32_t getSectors28Bit() const;
        [[nodiscard]] uint32_t getSectors48Bit() const;
        [[nodiscard]] bool supportsLBA48() const;

    protected:
        /**
         * @brief Wait for the ATA device to clear its busy flag
         * @param timeout Timeout in milliseconds
         * @return true if the device is no longer busy, false if the operation times out
         */
        bool waitForBusy(uint32_t timeout);

        bool waitForNotBusy(uint32_t timeout);

        /**
         * @brief Wait for the ATA device to be ready for data transfer
         * @param timeout Timeout in milliseconds
         * @return true if the device is ready for data transfer, false if the operation times out
         */
        bool waitForReady(uint32_t timeout);

        /**
         * @brief Select the device based on the LBA and master/slave setting
         * @param logicalBlockAddress Logical Block Address for the upcoming operation
         */
        void selectDevice(uint32_t logicalBlockAddress);

        /**
         * @brief Set the LBA for the next operation
         * @param logicalBlockAddress Logical Block Address to be set in the device registers
         */
        void setLBA(uint32_t logicalBlockAddress);

        /**
         * @brief Helper function to extract a string from the identity data
         * @param source Pointer to the source data buffer
         * @param dest Pointer to the destination character array
         * @param start Starting index in the source buffer
         * @param length Length of the string to extract (in bytes)
         */
        static void extractString(const uint16_t* source, char* dest, int start, int length);

        /**
         * @brief Execute an ATA command
         * @param command The ATA command to execute
         * @param logicalBlockAddress Logical Block Address for the command
         * @param sectorCount Number of sectors for the command
         * @return true if the command execution is successful, false otherwise
         */
        bool executeCommand(Command command, uint32_t logicalBlockAddress, uint8_t sectorCount, uint8_t* buffer, uint32_t timeout);

        /**
         * @brief Check the status register for errors or other flags
         * @return true if no errors are present, false otherwise
         */
        bool checkStatus();

        /**
         * @brief Clear the error on the ATA device.
         * @return true if the error was successfully cleared, false otherwise.
         */
        bool clearError();

    protected:
        // I/O ports for ATA communication
        uint32_t basePort_;                // keep the base port for logging
        ports::WordPort dataPort_;         // Data transfer
        ports::BytePort errorPort_;        // Error information
        ports::BytePort sectorCountPort_;  // Number of sectors to transfer
        ports::BytePort lbaLowPort_;       // LBA bits 0-7
        ports::BytePort lbaMidPort_;       // LBA bits 8-15
        ports::BytePort lbaHighPort_;      // LBA bits 16-23
        ports::BytePort devicePort_;       // Device selection
        ports::BytePort commandPort_;      // Command and status
        ports::BytePort controlPort_;      // Alternate status and device control

        Type deviceType_;  // Indicates if this is the master or slave device

        // Device information (populated by identify())
        char serialNumber_[21]{};    // +1 for null termination
        char firmwareVersion_[9]{};  // +1 for null termination
        char modelNumber_[41]{};     // +1 for null termination
        uint64_t storageSize_{};     // Total storage size in bytes
        uint32_t sectors28Bit_{};    // Number of 28-bit addressable sectors
        uint32_t sectors48Bit_{};    // Number of 48-bit addressable sectors
        bool supports48Bit_{};       // Indicates LBA48 support
    };

}  // namespace PalmyraOS::kernel