#pragma once

#include "core/definitions.h"
#include "core/network/NetworkInterface.h"

namespace PalmyraOS::kernel {

    /**
     * @brief Network Interface Manager (Singleton)
     *
     * System-wide manager for all network interfaces (eth0, wlan0, lo, etc.).
     * Provides a centralized registry for interface discovery, configuration, and packet routing.
     *
     * Responsibilities:
     * - Register/unregister hardware drivers (NetworkInterface implementations)
     * - Maintain default interface for outbound traffic
     * - Route packets to specific or default interfaces
     * - Enumerate all registered interfaces
     * - Provide global access via static methods
     *
     * Design Pattern: Singleton (single instance, thread-unsafe for OS dev)
     *
     * Usage Example:
     *   // Once at boot
     *   NetworkManager::initialize();
     *
     *   // Register hardware drivers
     *   auto eth0 = new PCnetDriver(bus, dev, fn, &heapManager);
     *   eth0->initialize();
     *   NetworkManager::registerInterface(eth0);
     *   NetworkManager::setDefaultInterface("eth0");
     *
     *   // Application code
     *   NetworkManager::sendPacket(frame_data, frame_length);  // Uses default
     *   NetworkManager::sendPacketTo("wlan0", data, len);      // Specific interface
     *
     * @note NOT thread-safe - assumes single-threaded OS or interrupt-driven architecture
     * @note Maximum interface count is MAX_INTERFACES (typically 8)
     * @note First registered interface automatically becomes default
     */
    class NetworkManager {
    public:
        // ==================== Configuration Constants ====================

        /// @brief Maximum number of network interfaces supported
        static constexpr uint8_t MAX_INTERFACES = 8;

        /// @brief Special value indicating "not found" in interface lookups
        static constexpr uint8_t INVALID_INTERFACE_INDEX = 0xFF;

        // ==================== Lifecycle ====================

        /**
         * @brief Initialize the NetworkManager
         *
         * Must be called exactly once at system startup, before registering any interfaces.
         * Subsequent calls are idempotent (safe to call multiple times).
         *
         * @return true if initialization successful (or already initialized), false otherwise
         *
         * @note Called from kernelEntry.cpp during boot sequence
         * @note Sets up internal state and prepares for interface registration
         */
        static bool initialize();

        /// @brief Query if NetworkManager is initialized
        [[nodiscard]] static bool isInitialized() { return initialized_; }

        // ==================== Interface Registration ====================

        /**
         * @brief Register a network interface
         *
         * Adds a hardware driver to the manager's interface list.
         * The interface must already be initialized by its driver.
         *
         * @param interface Pointer to initialized NetworkInterface (cannot be nullptr)
         * @return true if registration successful, false if:
         *         - Manager not initialized
         *         - interface is nullptr
         *         - Maximum interface limit reached (MAX_INTERFACES)
         *         - Interface with same name already registered
         *
         * @note First registered interface automatically becomes default
         * @note Name must be unique (checked at registration time)
         *
         * Example:
         *   auto eth0 = new PCnetDriver(...);
         *   if (eth0->initialize() && NetworkManager::registerInterface(eth0)) {
         *       LOG_INFO("eth0 registered successfully");
         *   }
         */
        static bool registerInterface(NetworkInterface* interface);

        /**
         * @brief Unregister a network interface
         *
         * Removes an interface from the manager. If this was the default interface,
         * the default is cleared (no automatic fallback to next interface).
         *
         * @param interface Pointer to interface to remove
         * @return true if unregistered successfully, false if interface not found
         *
         * @note Safe to call multiple times for same interface (idempotent)
         * @note Does NOT free memory - caller must delete interface if needed
         */
        static bool unregisterInterface(NetworkInterface* interface);

        /// @brief Get number of currently registered interfaces
        [[nodiscard]] static uint8_t getInterfaceCount() { return interfaceCount_; }

        // ==================== Interface Lookup ====================

        /**
         * @brief Get interface by name
         *
         * @param name Interface name (e.g., "eth0", "wlan0", "lo")
         * @return Pointer to interface, or nullptr if not found
         *
         * Example:
         *   auto eth0 = NetworkManager::getInterface("eth0");
         *   if (eth0) eth0->enable();
         */
        [[nodiscard]] static NetworkInterface* getInterface(const char* name);

        /**
         * @brief Get interface by index
         *
         * @param index Interface index (0 to interfaceCount - 1)
         * @return Pointer to interface, or nullptr if index out of range
         *
         * @note Useful for enumerating all interfaces
         * Example:
         *   for (uint8_t i = 0; i < NetworkManager::getInterfaceCount(); ++i) {
         *       auto iface = NetworkManager::getInterface(i);
         *       LOG_INFO("Interface %u: %s", i, iface->getName());
         *   }
         */
        [[nodiscard]] static NetworkInterface* getInterface(uint8_t index);

        // ==================== Default Interface Management ====================

        /**
         * @brief Get the default network interface
         *
         * The default interface is used for outbound traffic when no specific
         * interface is specified in sendPacket() calls.
         *
         * @return Pointer to default interface, or nullptr if none set
         *
         * @note First registered interface automatically becomes default
         * @note User can override with setDefaultInterface()
         */
        [[nodiscard]] static NetworkInterface* getDefaultInterface() { return defaultInterface_; }

        /**
         * @brief Set default interface by pointer
         *
         * @param interface Pointer to interface (must be registered)
         * @return true if set successfully, false if:
         *         - interface is nullptr
         *         - interface not found in registry
         *
         * Example:
         *   auto wlan0 = NetworkManager::getInterface("wlan0");
         *   if (wlan0) NetworkManager::setDefaultInterface(wlan0);
         */
        static bool setDefaultInterface(NetworkInterface* interface);

        /**
         * @brief Set default interface by name
         *
         * @param name Interface name (e.g., "eth0")
         * @return true if set successfully, false if interface not found
         *
         * @note Convenience wrapper around setDefaultInterface(NetworkInterface*)
         * Example:
         *   NetworkManager::setDefaultInterface("eth0");
         */
        static bool setDefaultInterface(const char* name);

        // ==================== Packet Routing ====================

        /**
         * @brief Send packet using default interface
         *
         * Transmits a raw Ethernet frame via the default interface.
         *
         * @param data Complete Ethernet frame (MAC header + payload + FCS)
         * @param length Frame length in bytes
         * @return true if packet queued for transmission, false if:
         *         - No default interface set
         *         - Default interface is DOWN
         *         - Hardware TX error (ring full, invalid length, etc.)
         *
         * @note Calls NetworkInterface::sendPacket() on default interface
         * @note Statistics updated automatically by underlying driver
         *
         * Example:
         *   uint8_t arp_frame[60] = { ... };
         *   if (!NetworkManager::sendPacket(arp_frame, 60)) {
         *       LOG_ERROR("Failed to send ARP request");
         *   }
         */
        static bool sendPacket(const uint8_t* data, uint32_t length);

        /**
         * @brief Send packet to specific interface by name
         *
         * Transmits a raw Ethernet frame via a specific named interface.
         *
         * @param interfaceName Interface name (e.g., "eth0", "wlan0")
         * @param data Complete Ethernet frame
         * @param length Frame length in bytes
         * @return true if packet queued for transmission, false if:
         *         - Interface name not found
         *         - Interface is DOWN
         *         - Hardware TX error
         *
         * @note Useful when multiple interfaces are available
         * @note More overhead than sendPacket() due to name lookup
         *
         * Example:
         *   NetworkManager::sendPacketTo("wlan0", data, len);
         */
        static bool sendPacketTo(const char* interfaceName, const uint8_t* data, uint32_t length);

        // ==================== Debug & Enumeration ====================

        /**
         * @brief Log all registered interfaces with detailed statistics
         *
         * Displays formatted information for each registered interface:
         * - Name (with default marker if applicable)
         * - MAC address (XX:XX:XX:XX:XX:XX format)
         * - IP address (W.X.Y.Z format, or "Not configured")
         * - Operational state (UP/DOWN/ERROR)
         * - MTU value
         * - Traffic statistics (packets, bytes, errors, dropped)
         *
         * @note Called by NetworkManager during boot and by diagnostics tools
         * @note Output goes to Logger (typically kernel log buffer)
         *
         * Example Output:
         *   ========================================
         *   Network Interfaces (2 registered):
         *   ========================================
         *
         *   eth0 (default):
         *     MAC:   08:00:27:40:D1:9D
         *     IP:    192.168.1.101
         *     State: UP
         *     MTU:   1500
         *     TX:    10 packets, 1024 bytes, 0 errors
         *     RX:    15 packets, 2048 bytes, 0 errors, 0 dropped
         *
         *   wlan0:
         *     MAC:   00:1A:2B:3C:4D:5E
         *     IP:    Not configured
         *     State: DOWN
         *     ...
         *   ========================================
         */
        static void listInterfaces();

    private:
        // ==================== Singleton Pattern ====================
        /// @brief Prevent instantiation
        NetworkManager() = delete;
        ~NetworkManager() = delete;
        NetworkManager(const NetworkManager&) = delete;
        NetworkManager& operator=(const NetworkManager&) = delete;

        // ==================== Helper Methods ====================

        /**
         * @brief Find interface index by pointer
         *
         * @param interface Interface to locate
         * @return Index if found, or INVALID_INTERFACE_INDEX if not found
         *
         * @note Used internally by unregisterInterface() and setDefaultInterface()
         */
        [[nodiscard]] static uint8_t findInterfaceIndex(const NetworkInterface* interface);

        // ==================== Static Data ====================

        /// @brief Initialization state (true after initialize() succeeds)
        static bool initialized_;

        /// @brief Array of registered interface pointers (nullptr = empty slot)
        static NetworkInterface* interfaces_[MAX_INTERFACES];

        /// @brief Number of currently registered interfaces (0 to MAX_INTERFACES)
        static uint8_t interfaceCount_;

        /// @brief Currently selected default interface (nullptr if none set)
        static NetworkInterface* defaultInterface_;
    };

}  // namespace PalmyraOS::kernel
