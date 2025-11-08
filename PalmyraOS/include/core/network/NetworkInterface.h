#pragma once

#include "core/definitions.h"

namespace PalmyraOS::types {
    class HeapManagerBase;  // Forward declaration
}

namespace PalmyraOS::kernel {

    /**
     * @brief Abstract Base Network Interface
     *
     * Represents a single network adapter (eth0, wlan0, lo, etc.) in the system.
     * Provides a uniform interface for all network hardware drivers to implement.
     *
     * Hardware-specific drivers inherit from this class and implement:
     * - initialize()  : Initialize hardware and allocate DMA buffers
     * - sendPacket()  : Transmit Ethernet frames
     * - enable()      : Start TX/RX (hardware-specific)
     * - disable()     : Stop TX/RX (hardware-specific)
     * - handleInterrupt() : Process hardware interrupts
     *
     * The base class handles:
     * - Network configuration (IP, subnet, gateway)
     * - Interface state management (UP/DOWN/ERROR)
     * - Statistics collection (packets, bytes, errors)
     * - MAC address storage
     *
     * Usage Example:
     *   auto eth0 = new PCnetDriver(bus, dev, fn, &heapManager);
     *   eth0->initialize();
     *   NetworkManager::registerInterface(eth0);
     *   eth0->setIPAddress(0xC0A80101);  // 192.168.1.1
     *   eth0->enable();
     *   NetworkManager::sendPacket(frame_data, frame_length);
     */
    class NetworkInterface {
    public:
        // ==================== Interface State Enumeration ====================

        /**
         * @brief Network interface operational state
         */
        enum class State : uint8_t {
            Down  = 0,  ///< Interface is inactive (not ready for TX/RX)
            Up    = 1,  ///< Interface is active and ready for traffic
            Error = 2   ///< Hardware error state (requires recovery/reinitialization)
        };

        // ==================== Configuration Constants ====================

        /// @brief Maximum interface name length (e.g., "eth0", "wlan0")
        static constexpr size_t MAX_NAME_LENGTH  = 15;

        /// @brief MAC address size in bytes (IEEE 802.3 standard)
        static constexpr size_t MAC_ADDRESS_SIZE = 6;

        /// @brief Standard Ethernet Maximum Transmission Unit
        static constexpr uint16_t STANDARD_MTU   = 1500;

        /// @brief Minimum valid Ethernet frame size (header + CRC)
        static constexpr uint32_t MIN_FRAME_SIZE = 60;

        /// @brief Maximum valid Ethernet frame size (jumbo frames)
        static constexpr uint32_t MAX_FRAME_SIZE = 1518;

        // ==================== Lifecycle ====================

        /**
         * @brief Constructor
         *
         * Initializes the network interface with basic configuration.
         * The interface starts in DOWN state and must be enabled explicitly.
         *
         * @param name Interface name (e.g., "eth0", max 15 chars)
         * @param macAddress MAC address bytes (nullptr allowed - driver reads from hardware)
         * @param heapManager Heap allocator (dependency injection for DMA buffers)
         */
        NetworkInterface(const char* name, const uint8_t macAddress[MAC_ADDRESS_SIZE], types::HeapManagerBase* heapManager);

        /// @brief Virtual destructor for proper cleanup of derived classes
        virtual ~NetworkInterface() = default;

        // ==================== Memory Management (Freestanding C++) ====================

        /// @brief Custom operator new for freestanding environment (global heap allocation)
        static void* operator new(size_t size);

        /// @brief Placement new operator (used by createInstance in HeapManager)
        static void* operator new(size_t size, void* ptr) noexcept;

        /// @brief Custom operator delete for unsized deallocation (virtual destructors)
        static void operator delete(void* ptr) noexcept;

        /// @brief Custom operator delete for sized deallocation (compiler optimization)
        static void operator delete(void* ptr, size_t size) noexcept;

        // ==================== Pure Virtual Interface (MUST Implement) ====================

        /**
         * @brief Initialize network hardware
         *
         * Called once during driver initialization. Responsible for:
         * - Reading hardware configuration (MAC, EEPROM, etc.)
         * - Allocating DMA buffers for descriptors and packet data
         * - Setting up hardware registers and initialization blocks
         * - Preparing for enable() call
         *
         * @return true if initialization succeeded, false on hardware failure
         *
         * @note Must NOT enable TX/RX - that's done by enable()
         * @note Must be idempotent - safe to call multiple times
         */
        virtual bool initialize()                                     = 0;

        /**
         * @brief Transmit Ethernet packet
         *
         * Queues a complete Ethernet frame (including headers and FCS) for transmission.
         * The frame must be a valid Ethernet packet; no additional framing is applied.
         *
         * @param data Complete Ethernet frame (MAC header + payload)
         * @param length Frame length in bytes (MIN_FRAME_SIZE to MAX_FRAME_SIZE)
         * @return true if queued for transmission, false if:
         *         - Interface is DOWN
         *         - TX ring is full (all descriptors owned by NIC)
         *         - Frame exceeds maximum size
         *
         * @note This is a fire-and-forget operation; completion is signaled via TINT interrupt
         * @note Driver updates statistics automatically (TX count/bytes or TX errors)
         */
        virtual bool sendPacket(const uint8_t* data, uint32_t length) = 0;

        // ==================== Virtual Interface (CAN Override) ====================

        /**
         * @brief Bring interface UP (enable TX/RX)
         *
         * Default implementation just changes state. Hardware drivers MUST override
         * to actually enable transmitter and receiver in hardware registers.
         *
         * @return true if successfully enabled, false on hardware error
         *
         * @note Override must check if interface is in ERROR state
         * @note Should enable interrupts (INTR, TINT, RINT flags)
         * @note Should wait for TX/RX to actually start (poll status bits)
         */
        virtual bool enable();

        /**
         * @brief Bring interface DOWN (disable TX/RX)
         *
         * Default implementation just changes state. Hardware drivers MUST override
         * to actually disable hardware and drain queues.
         *
         * @return true if successfully disabled, false on hardware error
         *
         * @note Override should flush any pending TX frames
         * @note Override should clean up DMA descriptors
         */
        virtual bool disable();

        /**
         * @brief Process hardware interrupt
         *
         * Called from interrupt handler when NIC generates an interrupt.
         * Default implementation does nothing (polling mode).
         *
         * Hardware drivers MUST override to:
         * - Read interrupt status register (CSR0, status port, etc.)
         * - Process RX packets (RINT)
         * - Complete TX frames (TINT)
         * - Handle error conditions (ERR)
         * - Clear interrupt flags (write-to-clear bits)
         *
         * @note Must be fast - called from ISR context
         * @note Must NOT acquire locks or perform blocking operations
         * @note Should disable interrupts while processing to prevent recursion
         */
        virtual void handleInterrupt();

        // ==================== Network Configuration ====================

        /**
         * @brief Set IPv4 address
         *
         * @param ip IPv4 address in host byte order
         *           Example: 0xC0A80101 = 192.168.1.1
         */
        void setIPAddress(uint32_t ip);

        /**
         * @brief Set IPv4 subnet mask
         *
         * @param mask Subnet mask in host byte order
         *             Example: 0xFFFFFF00 = 255.255.255.0 (/24)
         */
        void setSubnetMask(uint32_t mask);

        /**
         * @brief Set default gateway
         *
         * @param gateway Gateway IP in host byte order
         *                Example: 0xC0A80101 = 192.168.1.1
         */
        void setGateway(uint32_t gateway);

        /**
         * @brief Set Maximum Transmission Unit
         *
         * @param mtu MTU in bytes (typically STANDARD_MTU = 1500)
         *            Jumbo frames: 9000 bytes
         */
        void setMTU(uint16_t mtu);

        /**
         * @brief Set promiscuous mode
         *
         * In promiscuous mode, the interface receives ALL frames,
         * not just frames destined for this MAC address.
         *
         * @param enabled true = receive all packets, false = filter by MAC
         *
         * @note Requires hardware support and may need driver override
         * @note Useful for packet capture and network analysis tools
         */
        void setPromiscuousMode(bool enabled);

        // ==================== Information Accessors ====================

        /// @brief Get interface name (e.g., "eth0")
        [[nodiscard]] const char* getName() const { return name_; }

        /// @brief Get MAC address (6 bytes)
        [[nodiscard]] const uint8_t* getMACAddress() const { return macAddress_; }

        /// @brief Get IPv4 address in host byte order
        [[nodiscard]] uint32_t getIPAddress() const { return ipAddress_; }

        /// @brief Get subnet mask in host byte order
        [[nodiscard]] uint32_t getSubnetMask() const { return subnetMask_; }

        /// @brief Get default gateway in host byte order
        [[nodiscard]] uint32_t getGateway() const { return gateway_; }

        /// @brief Get Maximum Transmission Unit
        [[nodiscard]] uint16_t getMTU() const { return mtu_; }

        /// @brief Get current interface state
        [[nodiscard]] State getState() const { return state_; }

        /// @brief Query if interface is operational (State::Up)
        [[nodiscard]] bool isUp() const { return state_ == State::Up; }

        /// @brief Query promiscuous mode status
        [[nodiscard]] bool isPromiscuous() const { return promiscuousMode_; }

        // ==================== Statistics Management ====================

        /**
         * @brief Update TX/RX statistics
         *
         * Called by drivers after each TX/RX operation (usually in interrupt handler).
         * Maintains counters for packets, bytes, and errors.
         *
         * @param bytes Number of bytes transmitted/received
         * @param isTx true for TX stat, false for RX stat
         * @param isError true if operation failed, false if successful
         */
        void updateStatistics(uint32_t bytes, bool isTx, bool isError);

        /// @brief Get total transmitted packets (never rolls over to 64-bit)
        [[nodiscard]] uint64_t getTxPackets() const { return txPackets_; }

        /// @brief Get total received packets
        [[nodiscard]] uint64_t getRxPackets() const { return rxPackets_; }

        /// @brief Get total transmitted bytes
        [[nodiscard]] uint64_t getTxBytes() const { return txBytes_; }

        /// @brief Get total received bytes
        [[nodiscard]] uint64_t getRxBytes() const { return rxBytes_; }

        /// @brief Get TX error count (ring full, invalid length, etc.)
        [[nodiscard]] uint32_t getTxErrors() const { return txErrors_; }

        /// @brief Get RX error count (CRC errors, frame errors, etc.)
        [[nodiscard]] uint32_t getRxErrors() const { return rxErrors_; }

        /// @brief Get dropped RX packets (buffer full, DMA errors, etc.)
        [[nodiscard]] uint32_t getRxDropped() const { return rxDropped_; }

        /// @brief Reset all statistics to zero
        void resetStatistics();

    protected:
        /**
         * @brief Set interface state (protected - only drivers can change)
         *
         * @param state New state (UP, DOWN, or ERROR)
         */
        void setState(State state) { state_ = state; }

        /**
         * @brief Get heap manager for memory allocation
         *
         * @return Heap manager passed in constructor (dependency injection)
         *
         * @note Drivers use this to allocate DMA buffers
         */
        [[nodiscard]] types::HeapManagerBase* getHeapManager() const { return heapManager_; }

    private:
        // ==================== Dependencies ====================
        types::HeapManagerBase* heapManager_;  ///< Memory allocator for DMA buffers

        // ==================== Interface Identity ====================
        char name_[MAX_NAME_LENGTH + 1];        ///< Interface name (null-terminated, e.g., "eth0")
        uint8_t macAddress_[MAC_ADDRESS_SIZE];  ///< MAC address (6 bytes)

        // ==================== Network Configuration ====================
        uint32_t ipAddress_;   ///< IPv4 address (host byte order)
        uint32_t subnetMask_;  ///< IPv4 subnet mask (host byte order)
        uint32_t gateway_;     ///< Default gateway IP (host byte order)

        // ==================== Interface State ====================
        State state_;  ///< Current operational state (UP/DOWN/ERROR)

        // ==================== Interface Properties ====================
        uint16_t mtu_;          ///< Maximum Transmission Unit (bytes)
        bool promiscuousMode_;  ///< Receive all packets regardless of MAC

        // ==================== Traffic Statistics ====================
        uint64_t txPackets_;  ///< Total packets transmitted
        uint64_t rxPackets_;  ///< Total packets received
        uint64_t txBytes_;    ///< Total bytes transmitted
        uint64_t rxBytes_;    ///< Total bytes received
        uint32_t txErrors_;   ///< TX errors (ring full, invalid, etc.)
        uint32_t rxErrors_;   ///< RX errors (CRC, frame, etc.)
        uint32_t rxDropped_;  ///< Dropped RX packets (buffer full, DMA errors, etc.)
    };

}  // namespace PalmyraOS::kernel
