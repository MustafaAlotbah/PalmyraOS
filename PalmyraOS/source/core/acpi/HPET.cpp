#include "core/acpi/HPET.h"
#include "core/acpi/ACPI.h"
#include "core/peripherals/Logger.h"

namespace PalmyraOS::kernel {

    // Static member initialization
    bool HPET::initialized_               = false;
    volatile uint64_t* HPET::baseAddress_ = nullptr;
    uint32_t HPET::clockPeriod_           = 0;
    uint8_t HPET::numComparators_         = 0;
    uint16_t HPET::vendorID_              = 0;
    bool HPET::is64Bit_                   = false;
    bool HPET::legacyReplacementCapable_  = false;

    bool HPET::initialize() {
        if (initialized_) {
            LOG_WARN("HPET: Already initialized");
            return true;
        }

        if (!ACPI::isInitialized()) {
            LOG_ERROR("HPET: ACPI not initialized");
            return false;
        }

        // Get HPET table from ACPI
        const auto* hpetTable = ACPI::getHPET();
        if (!hpetTable) {
            LOG_ERROR("HPET: HPET table not found in ACPI");
            return false;
        }

        // Validate address space (must be system memory)
        if (hpetTable->addressSpaceID != 0) {
            LOG_ERROR("HPET: Unsupported address space %u (expected 0=System Memory)", hpetTable->addressSpaceID);
            return false;
        }

        // Map HPET registers (will be identity-mapped during virtual memory initialization)
        LOG_INFO("HPET: Initializing at physical address 0x%llX", hpetTable->address);
        baseAddress_ = reinterpret_cast<volatile uint64_t*>(static_cast<uintptr_t>(hpetTable->address));

        // Parse capabilities
        parseCapabilities();

        // Log initialization info
        LOG_INFO("HPET: Clock period: %u femtoseconds", clockPeriod_);
        LOG_INFO("HPET: Frequency: ~%llu Hz", getFrequency());
        LOG_INFO("HPET: Comparators: %u", numComparators_);
        LOG_INFO("HPET: Counter size: %s", is64Bit_ ? "64-bit" : "32-bit");
        LOG_INFO("HPET: Vendor ID: 0x%04X", vendorID_);
        LOG_INFO("HPET: Legacy replacement: %s", legacyReplacementCapable_ ? "Capable" : "Not capable");

        initialized_ = true;
        return true;
    }

    void HPET::parseCapabilities() {
        // Read General Capabilities and ID register
        uint64_t capabilities     = readRegister(Register::GeneralCapabilities);

        // Extract clock period (bits 32-63, in femtoseconds)
        clockPeriod_              = static_cast<uint32_t>(capabilities >> 32);

        // Extract capabilities from low 32 bits
        uint32_t capLow           = static_cast<uint32_t>(capabilities & 0xFFFFFFFF);

        // Vendor ID (bits 16-31)
        vendorID_                 = static_cast<uint16_t>((capLow & static_cast<uint32_t>(Capability::PCIVendorID_Mask)) >> static_cast<uint32_t>(Capability::PCIVendorID_Shift));

        // Legacy replacement capable (bit 15)
        legacyReplacementCapable_ = (capLow & static_cast<uint32_t>(Capability::LegacyReplacement_Bit)) != 0;

        // Counter size (bit 13)
        is64Bit_                  = (capLow & static_cast<uint32_t>(Capability::CounterSize64_Bit)) != 0;

        // Number of comparators (bits 8-12, value is N-1)
        uint8_t numTimers = static_cast<uint8_t>((capLow & static_cast<uint32_t>(Capability::NumComparators_Mask)) >> static_cast<uint32_t>(Capability::NumComparators_Shift));
        numComparators_   = numTimers + 1;
    }

    void HPET::enable() {
        if (!initialized_) {
            LOG_ERROR("HPET: Not initialized");
            return;
        }

        // Read current configuration
        uint64_t config = readRegister(Register::GeneralConfiguration);

        // Set enable bit (bit 0)
        config |= static_cast<uint64_t>(ConfigBit::Enable);

        // Write back
        writeRegister(Register::GeneralConfiguration, config);

        LOG_INFO("HPET: Main counter enabled");
    }

    void HPET::disable() {
        if (!initialized_) {
            LOG_ERROR("HPET: Not initialized");
            return;
        }

        // Read current configuration
        uint64_t config = readRegister(Register::GeneralConfiguration);

        // Clear enable bit (bit 0)
        config &= ~static_cast<uint64_t>(ConfigBit::Enable);

        // Write back
        writeRegister(Register::GeneralConfiguration, config);

        LOG_INFO("HPET: Main counter disabled");
    }

    void HPET::enableLegacyReplacement() {
        if (!initialized_) {
            LOG_ERROR("HPET: Not initialized");
            return;
        }

        if (!legacyReplacementCapable_) {
            LOG_ERROR("HPET: Legacy replacement not supported by hardware");
            return;
        }

        // Read current configuration
        uint64_t config = readRegister(Register::GeneralConfiguration);

        // Set legacy replacement bit (bit 1)
        config |= static_cast<uint64_t>(ConfigBit::LegacyReplacement);

        // Write back
        writeRegister(Register::GeneralConfiguration, config);

        LOG_WARN("HPET: Legacy replacement mode ENABLED (PIT/RTC replaced)");
    }

    void HPET::disableLegacyReplacement() {
        if (!initialized_) {
            LOG_ERROR("HPET: Not initialized");
            return;
        }

        // Read current configuration
        uint64_t config = readRegister(Register::GeneralConfiguration);

        // Clear legacy replacement bit (bit 1)
        config &= ~static_cast<uint64_t>(ConfigBit::LegacyReplacement);

        // Write back
        writeRegister(Register::GeneralConfiguration, config);

        LOG_INFO("HPET: Legacy replacement mode disabled");
    }

    uint64_t HPET::readCounter() {
        if (!initialized_) { return 0; }

        // Read main counter value register
        return readRegister(Register::MainCounterValue);
    }

    uint64_t HPET::getFrequency() {
        if (clockPeriod_ == 0) { return 0; }

        // Frequency (Hz) = 10^15 / clock_period_femtoseconds
        // Using integer math: (1000000000000000ULL / clockPeriod_)
        return 1000000000000000ULL / clockPeriod_;
    }

    uint32_t HPET::getClockPeriod() { return clockPeriod_; }

    uint32_t HPET::measureCPUFrequency(uint32_t measurementTimeMs) {
        if (!initialized_) {
            LOG_ERROR("HPET: Not initialized");
            return 0;
        }

        LOG_INFO("HPET: TSC frequency measurement (%u ms)...", measurementTimeMs);

        // Perform multiple measurements for accuracy and consistency
        uint32_t results[3];
        uint32_t validResults = 0;

        for (int attempt = 0; attempt < 3; ++attempt) {
            uint32_t result = performSingleMeasurement(measurementTimeMs);
            if (result > 0) {
                results[validResults++] = result;
                LOG_DEBUG("HPET: Attempt %d: %u MHz", attempt + 1, result);
            }
            else { LOG_WARN("HPET: Attempt %d failed", attempt + 1); }
        }

        if (validResults == 0) {
            LOG_ERROR("HPET: All measurement attempts failed");
            return 0;
        }

        // Calculate median for robustness
        uint32_t finalResult;
        if (validResults == 1) { finalResult = results[0]; }
        else if (validResults == 2) { finalResult = (results[0] + results[1]) / 2; }
        else {
            // Sort and take median
            if (results[0] > results[1]) {
                uint32_t temp = results[0];
                results[0]    = results[1];
                results[1]    = temp;
            }
            if (results[1] > results[2]) {
                uint32_t temp = results[1];
                results[1]    = results[2];
                results[2]    = temp;
            }
            if (results[0] > results[1]) {
                uint32_t temp = results[0];
                results[0]    = results[1];
                results[1]    = temp;
            }
            finalResult = results[1];  // Median
        }

        LOG_INFO("HPET: TSC frequency measurement: %u MHz (median of %u samples)", finalResult, validResults);
        return finalResult;
    }

    uint32_t HPET::performSingleMeasurement(uint32_t measurementTimeMs) {
        // Quick verification that HPET is counting
        uint64_t test1 = readCounter();
        for (volatile int i = 0; i < 50000; ++i) {}
        uint64_t test2 = readCounter();

        if (test2 <= test1) {
            LOG_ERROR("HPET: Counter not incrementing");
            return 0;
        }

        // Read TSC helper function (RDTSC instruction)
        auto readTSC = []() -> uint64_t {
            uint32_t low, high;
            asm volatile("rdtsc" : "=a"(low), "=d"(high));
            return (static_cast<uint64_t>(high) << 32) | low;
        };

        // Serialize execution with CPUID to prevent out-of-order execution
        LOG_DEBUG("HPET: Serializing with CPUID...");
        asm volatile("cpuid" : : "a"(0) : "ebx", "ecx", "edx");

        // Read starting values
        LOG_DEBUG("HPET: Reading start counters...");
        uint64_t hpetStart = readCounter();
        uint64_t tscStart  = readTSC();
        LOG_DEBUG("HPET: Start - HPET=%llu, TSC=%llu", hpetStart, tscStart);

        // Calculate target HPET count
        // Convert milliseconds to HPET ticks:
        // ticks = (milliseconds * 10^12 femtoseconds/ms) / clock_period_femtoseconds
        uint64_t targetTicks = (static_cast<uint64_t>(measurementTimeMs) * 1000000000000ULL) / clockPeriod_;
        uint64_t hpetTarget  = hpetStart + targetTicks;
        LOG_DEBUG("HPET: Target ticks=%llu, target=%llu (clock_period=%u fs)", targetTicks, hpetTarget, clockPeriod_);

        // Busy wait until target reached (interrupts should be enabled by caller)
        // NOTE: We batch-check the HPET counter because MMIO reads are VERY slow (~1000+ CPU cycles)
        // Reading every iteration would kill performance and skew measurements
        LOG_DEBUG("HPET: Starting busy-wait loop...");
        uint32_t loopIterations = 0;
        uint64_t current        = hpetStart;
        while (current < hpetTarget) {
            // Do many iterations before checking HPET again (amortize MMIO cost)
            for (volatile int i = 0; i < 10000; ++i) {
                asm volatile("pause");
                loopIterations++;
            }

            // Now read HPET counter (expensive MMIO read)
            current = readCounter();

            // Debug every 10 million iterations to avoid log spam
            if ((loopIterations % 10000000) == 0) { LOG_DEBUG("HPET: Loop iteration %u, current=%llu, target=%llu", loopIterations, current, hpetTarget); }
        }
        LOG_DEBUG("HPET: Busy-wait completed after %u iterations", loopIterations);

        // Read ending values (serialize again)
        LOG_DEBUG("HPET: Reading end counters...");
        asm volatile("cpuid" : : "a"(0) : "ebx", "ecx", "edx");
        uint64_t tscEnd  = readTSC();
        uint64_t hpetEnd = readCounter();
        LOG_DEBUG("HPET: End - HPET=%llu, TSC=%llu", hpetEnd, tscEnd);

        // Calculate elapsed time
        uint64_t hpetElapsed        = hpetEnd - hpetStart;
        uint64_t tscElapsed         = tscEnd - tscStart;

        // Convert HPET ticks to nanoseconds
        // nanoseconds = (hpet_ticks * clock_period_femtoseconds) / 10^6
        uint64_t elapsedNanoseconds = (hpetElapsed * clockPeriod_) / 1000000ULL;

        // Convert nanoseconds to milliseconds for logging
        uint64_t elapsedMs          = elapsedNanoseconds / 1000000ULL;

        // Calculate CPU frequency in Hz
        // TSC frequency (Hz) = (TSC_ticks * 10^9) / elapsed_nanoseconds
        uint64_t cpuFrequencyHz     = (tscElapsed * 1000000000ULL) / elapsedNanoseconds;

        // Calculate MHz for return value
        uint32_t cpuFrequencyMHz    = cpuFrequencyHz / 1000000;

        LOG_INFO("HPET: CPU TSC frequency measured: %u MHz", cpuFrequencyMHz);
        LOG_DEBUG("HPET: Measurement duration: %llu ms (target: %u ms)", elapsedMs, measurementTimeMs);
        LOG_DEBUG("HPET: TSC ticks elapsed: %llu", tscElapsed);
        LOG_DEBUG("HPET: HPET ticks elapsed: %llu", hpetElapsed);

        return cpuFrequencyMHz;
    }

    void HPET::delayMicroseconds(uint32_t microseconds) {
        if (!initialized_ || microseconds == 0) { return; }

        // Convert microseconds to HPET ticks
        // ticks = (microseconds * 10^9) / clock_period_femtoseconds
        // Avoid overflow: ticks = (microseconds * 1000000000ULL) / clockPeriod_
        uint64_t ticks  = (static_cast<uint64_t>(microseconds) * 1000000000ULL) / clockPeriod_;

        // Read start counter value
        uint64_t start  = readCounter();
        uint64_t target = start + ticks;

        // Busy wait until target reached
        while (readCounter() < target) {
            // Busy loop - consider yielding CPU in a real implementation
            asm volatile("pause");
        }
    }

    uint64_t HPET::getElapsedNanoseconds(uint64_t previousCounter) {
        if (!initialized_) { return 0; }

        uint64_t current = readCounter();
        uint64_t elapsed = current - previousCounter;

        // Convert ticks to nanoseconds
        // nanoseconds = (elapsed_ticks * clock_period_femtoseconds) / 10^6
        // Avoid overflow by rearranging: (elapsed * clockPeriod_) / 1000000
        return (elapsed * clockPeriod_) / 1000000ULL;
    }

    uint64_t HPET::readRegister(Register reg) {
        if (!baseAddress_) { return 0; }

        // Calculate offset in 64-bit words
        uintptr_t offset = static_cast<uintptr_t>(reg) / sizeof(uint64_t);

        // Read and return
        return baseAddress_[offset];
    }

    void HPET::writeRegister(Register reg, uint64_t value) {
        if (!baseAddress_) { return; }

        // Calculate offset in 64-bit words
        uintptr_t offset     = static_cast<uintptr_t>(reg) / sizeof(uint64_t);

        // Write value
        baseAddress_[offset] = value;
    }

}  // namespace PalmyraOS::kernel
