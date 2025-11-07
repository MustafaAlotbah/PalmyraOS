#pragma once

#include "core/definitions.h"

namespace PalmyraOS::kernel {

    /**
     * @class HPET
     * @brief High Precision Event Timer driver
     *
     * HPET provides high-resolution timing and can optionally replace legacy PIT/RTC.
     * This implementation allows using HPET alongside PIT initially, with the ability
     * to enable legacy replacement mode when ready.
     *
     * Features:
     * - Nanosecond-precision time measurement
     * - Microsecond-accurate delays
     * - Multiple independent timer channels (comparators)
     * - Optional legacy timer replacement (PIT/RTC)
     */
    class HPET {
    public:
        /**
         * @brief Event Timer Block ID bit fields
         */
        enum class Capability : uint32_t {
            HardwareRevID_Mask    = 0x000000FF,  // Bits 0-7: Hardware revision
            NumComparators_Mask   = 0x00001F00,  // Bits 8-12: Number of comparators (N-1)
            CounterSize64_Bit     = 0x00002000,  // Bit 13: 1=64-bit counter, 0=32-bit
            LegacyReplacement_Bit = 0x00008000,  // Bit 15: Legacy replacement capable
            PCIVendorID_Mask      = 0xFFFF0000,  // Bits 16-31: PCI Vendor ID

            NumComparators_Shift  = 8,
            PCIVendorID_Shift     = 16
        };

        /**
         * @brief HPET register offsets (memory-mapped)
         */
        enum class Register : uint32_t {
            GeneralCapabilities    = 0x000,  // General Capabilities and ID (read-only)
            GeneralConfiguration   = 0x010,  // General Configuration (read/write)
            GeneralInterruptStatus = 0x020,  // General Interrupt Status (read/write)
            MainCounterValue       = 0x0F0,  // Main Counter Value (read/write)
            Timer0Config           = 0x100,  // Timer 0 Configuration and Capability
            Timer0Comparator       = 0x108,  // Timer 0 Comparator Value
            Timer1Config           = 0x120,  // Timer 1 Configuration and Capability
            Timer1Comparator       = 0x128,  // Timer 1 Comparator Value
            Timer2Config           = 0x140,  // Timer 2 Configuration and Capability
            Timer2Comparator       = 0x148,  // Timer 2 Comparator Value
        };

        /**
         * @brief General Configuration register bits
         */
        enum class ConfigBit : uint64_t {
            Enable            = 0x0001,  // Bit 0: Overall enable
            LegacyReplacement = 0x0002,  // Bit 1: Legacy replacement route (IRQ0/IRQ8)
        };

        /**
         * @brief Timer Configuration register bits
         */
        enum class TimerConfigBit : uint64_t {
            InterruptEnable     = 0x0004,      // Bit 2: Timer interrupt enable
            PeriodicMode        = 0x0008,      // Bit 3: Periodic mode (vs one-shot)
            PeriodicCapable     = 0x0010,      // Bit 4: Timer supports periodic mode
            CounterSize64       = 0x0020,      // Bit 5: 64-bit mode capable
            ValueSet            = 0x0040,      // Bit 6: Set accumulator value
            Force32BitMode      = 0x0100,      // Bit 8: Force 32-bit mode
            InterruptRoute_Mask = 0x00003E00,  // Bits 9-13: Interrupt route
            FSBEnable           = 0x00004000,  // Bit 14: FSB interrupt delivery
            FSBCapable          = 0x00008000,  // Bit 15: FSB interrupt capable
        };

        /**
         * @brief Initialize HPET from ACPI table
         *
         * Maps HPET registers, reads capabilities, but does NOT enable it yet.
         * After initialization, use enable() to start the main counter.
         *
         * @return True if initialization successful, false otherwise
         */
        static bool initialize();

        /**
         * @brief Check if HPET is initialized
         */
        [[nodiscard]] static bool isInitialized() { return initialized_; }

        /**
         * @brief Get the physical address of HPET registers
         *
         * @return Physical address of HPET MMIO registers, or 0 if not initialized
         */
        [[nodiscard]] static uintptr_t getPhysicalAddress() { return reinterpret_cast<uintptr_t>(baseAddress_); }

        /**
         * @brief Enable HPET main counter
         *
         * Starts the HPET main counter. Does NOT enable legacy replacement mode.
         * Safe to call alongside PIT.
         */
        static void enable();

        /**
         * @brief Disable HPET main counter
         */
        static void disable();

        /**
         * @brief Enable legacy replacement mode
         *
         * Replaces PIT (IRQ0) and RTC periodic interrupt (IRQ8) with HPET.
         * Only call this when ready to replace PIT completely!
         *
         * WARNING: This will affect system timing if PIT is still in use.
         */
        static void enableLegacyReplacement();

        /**
         * @brief Disable legacy replacement mode
         */
        static void disableLegacyReplacement();

        /**
         * @brief Read current HPET main counter value
         *
         * @return Counter value in femtoseconds (based on clock period)
         */
        [[nodiscard]] static uint64_t readCounter();

        /**
         * @brief Get HPET frequency in Hz
         */
        [[nodiscard]] static uint64_t getFrequency();

        /**
         * @brief Get HPET clock period in femtoseconds
         */
        [[nodiscard]] static uint32_t getClockPeriod();

        /**
         * @brief Delay for specified microseconds using HPET
         *
         * High-precision busy-wait delay.
         *
         * @param microseconds Duration to delay
         */
        static void delayMicroseconds(uint32_t microseconds);

        /**
         * @brief Get elapsed time in nanoseconds since a previous counter value
         *
         * @param previousCounter Previous counter value from readCounter()
         * @return Elapsed time in nanoseconds
         */
        [[nodiscard]] static uint64_t getElapsedNanoseconds(uint64_t previousCounter);

        /**
         * @brief Get number of comparators (timers) available
         */
        [[nodiscard]] static uint8_t getNumComparators() { return numComparators_; }

        /**
         * @brief Check if 64-bit counter is supported
         */
        [[nodiscard]] static bool is64BitCounter() { return is64Bit_; }

        /**
         * @brief Check if legacy replacement is capable
         */
        [[nodiscard]] static bool isLegacyReplacementCapable() { return legacyReplacementCapable_; }

        /**
         * @brief Measure CPU TSC frequency using HPET as precise timing reference
         *
         * Uses HPET's known frequency to accurately measure the CPU's TSC (Time Stamp Counter)
         * frequency. Much more accurate than RTC-based measurement.
         *
         * @param measurementTimeMs Time to measure in milliseconds (default 100ms for accuracy)
         * @return CPU frequency in MHz, or 0 if measurement failed
         */
        [[nodiscard]] static uint32_t measureCPUFrequency(uint32_t measurementTimeMs = 100);

    private:
        /**
         * @brief Perform a single TSC frequency measurement
         *
         * @param measurementTimeMs Measurement duration in milliseconds
         * @return CPU frequency in MHz, or 0 if failed
         */
        [[nodiscard]] static uint32_t performSingleMeasurement(uint32_t measurementTimeMs);
        // Initialization state
        static bool initialized_;
        static volatile uint64_t* baseAddress_;  // Memory-mapped base address

        // Capabilities (from General Capabilities register)
        static uint32_t clockPeriod_;           // Clock period in femtoseconds
        static uint8_t numComparators_;         // Number of timer comparators
        static uint16_t vendorID_;              // PCI Vendor ID
        static bool is64Bit_;                   // 64-bit counter support
        static bool legacyReplacementCapable_;  // Legacy replacement capability

        /**
         * @brief Read 64-bit value from HPET register
         */
        static uint64_t readRegister(Register reg);

        /**
         * @brief Write 64-bit value to HPET register
         */
        static void writeRegister(Register reg, uint64_t value);

        /**
         * @brief Parse capabilities from General Capabilities register
         */
        static void parseCapabilities();
    };

}  // namespace PalmyraOS::kernel
