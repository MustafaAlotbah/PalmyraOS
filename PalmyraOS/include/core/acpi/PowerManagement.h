#pragma once

#include "core/definitions.h"

namespace PalmyraOS::kernel {

    /**
     * @class PowerManagement
     * @brief ACPI-based power management for shutdown, reboot, and sleep states
     *
     * This class provides system power control using ACPI (if available)
     * with fallbacks to legacy methods.
     *
     * Features:
     * - ACPI-compliant shutdown (SLP_TYPa + SLP_EN)
     * - ACPI-compliant reboot (reset register)
     * - Legacy fallback methods (keyboard controller, triple fault)
     */
    class PowerManagement {
    public:
        /**
         * @brief Initialize power management subsystem
         *
         * Parses FADT to extract reset register, PM1a control block,
         * and DSDT pointer for _S5 sleep state package.
         *
         * @return True if initialized successfully, false otherwise
         */
        static bool initialize();

        /**
         * @brief Check if power management is initialized
         * @return True if initialized, false otherwise
         */
        [[nodiscard]] static bool isInitialized() { return initialized_; }

        /**
         * @brief Reboot the system
         *
         * Attempts reboot in this order:
         * 1. ACPI reset register (if available)
         * 2. Keyboard controller reset (legacy)
         * 3. Triple fault (last resort)
         *
         * This function does not return.
         */
        [[noreturn]] static void reboot();

        /**
         * @brief Shutdown the system (power off)
         *
         * Attempts shutdown in this order:
         * 1. ACPI S5 sleep state (if available)
         * 2. APM shutdown (legacy)
         * 3. Halt CPU (if all else fails)
         *
         * This function does not return.
         */
        [[noreturn]] static void shutdown();

        /**
         * @brief Enter ACPI sleep state (suspend)
         *
         * @param sleepState Sleep state to enter (1=S1, 3=S3, etc.)
         * @return True if sleep succeeded, false otherwise
         *
         * Note: S3 (suspend to RAM) is most common for suspend
         */
        static bool sleep(uint8_t sleepState);

    private:
        static bool initialized_;

        // ACPI reset register information (from FADT)
        static bool hasResetReg_;
        static uint8_t resetRegAddressSpace_;  // 0=Memory, 1=I/O
        static uint8_t resetRegBitWidth_;
        static uint8_t resetRegBitOffset_;
        static uint64_t resetRegAddress_;
        static uint8_t resetValue_;

        // PM1a control block (from FADT)
        static uint32_t pm1aControlBlock_;

        // Sleep type values (from DSDT _S5 package)
        static uint16_t slp5TypeA_;  // SLP_TYPa value for S5
        static uint16_t slp5TypeB_;  // SLP_TYPb value for S5
        static bool hasS5_;

        /**
         * @brief Parse FADT for power management information
         */
        static void parseFADT();

        /**
         * @brief Parse DSDT for _S5 sleep package
         *
         * Searches DSDT for "_S5_" package and extracts SLP_TYPa/SLP_TYPb values
         */
        static void parseS5FromDSDT();

        /**
         * @brief Perform ACPI reset using reset register
         */
        [[noreturn]] static void acpiReset();

        /**
         * @brief Perform ACPI shutdown using S5 sleep state
         */
        [[noreturn]] static void acpiShutdown();

        /**
         * @brief Legacy keyboard controller reset
         */
        [[noreturn]] static void keyboardReset();

        /**
         * @brief Legacy APM shutdown
         */
        [[noreturn]] static void apmShutdown();

        /**
         * @brief Triple fault to force reboot
         */
        [[noreturn]] static void tripleFault();
    };

}  // namespace PalmyraOS::kernel
