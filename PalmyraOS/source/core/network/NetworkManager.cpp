#include "core/network/NetworkManager.h"
#include "core/peripherals/Logger.h"
#include "libs/string.h"

namespace PalmyraOS::kernel {

    // ==================== Static Member Initialization ====================

    bool NetworkManager::initialized_ = false;
    NetworkInterface* NetworkManager::interfaces_[MAX_INTERFACES] = { nullptr };
    uint8_t NetworkManager::interfaceCount_ = 0;
    NetworkInterface* NetworkManager::defaultInterface_ = nullptr;

    // ==================== Lifecycle ====================

    bool NetworkManager::initialize() {
        if (initialized_) {
            LOG_WARN("NetworkManager: Already initialized");
            return true;
        }

        // Clear interface array
        for (uint8_t i = 0; i < MAX_INTERFACES; ++i) {
            interfaces_[i] = nullptr;
        }

        interfaceCount_ = 0;
        defaultInterface_ = nullptr;
        initialized_ = true;

        LOG_INFO("NetworkManager: Initialized (max %u interfaces)", MAX_INTERFACES);
        return true;
    }

    // ==================== Interface Registration ====================

    bool NetworkManager::registerInterface(NetworkInterface* interface) {
        if (!initialized_) {
            LOG_ERROR("NetworkManager: Not initialized");
            return false;
        }

        if (!interface) {
            LOG_ERROR("NetworkManager: Cannot register nullptr interface");
            return false;
        }

        if (interfaceCount_ >= MAX_INTERFACES) {
            LOG_ERROR("NetworkManager: Maximum interfaces (%u) reached", MAX_INTERFACES);
            return false;
        }

        // Check for duplicate name
        const char* name = interface->getName();
        for (uint8_t i = 0; i < interfaceCount_; ++i) {
            if (interfaces_[i] && strcmp(interfaces_[i]->getName(), name) == 0) {
                LOG_ERROR("NetworkManager: Interface '%s' already registered", name);
                return false;
            }
        }

        // Add to first empty slot
        interfaces_[interfaceCount_] = interface;
        interfaceCount_++;

        LOG_INFO("NetworkManager: Registered interface '%s' (%u/%u)",
                 name, interfaceCount_, MAX_INTERFACES);

        // If this is the first interface, make it default
        if (interfaceCount_ == 1) {
            defaultInterface_ = interface;
            LOG_INFO("NetworkManager: Set '%s' as default interface", name);
        }

        return true;
    }

    bool NetworkManager::unregisterInterface(NetworkInterface* interface) {
        if (!initialized_ || !interface) {
            return false;
        }

        uint8_t index = findInterfaceIndex(interface);
        if (index == INVALID_INTERFACE_INDEX) {
            LOG_WARN("NetworkManager: Interface '%s' not found", interface->getName());
            return false;
        }

        const char* name = interface->getName();

        // Clear default if this was the default interface
        if (defaultInterface_ == interface) {
            defaultInterface_ = nullptr;
            LOG_INFO("NetworkManager: Cleared default interface");
        }

        // Remove by shifting remaining interfaces
        for (uint8_t i = index; i < interfaceCount_ - 1; ++i) {
            interfaces_[i] = interfaces_[i + 1];
        }
        interfaces_[interfaceCount_ - 1] = nullptr;
        interfaceCount_--;

        LOG_INFO("NetworkManager: Unregistered interface '%s' (%u remaining)",
                 name, interfaceCount_);

        return true;
    }

    // ==================== Interface Lookup ====================

    NetworkInterface* NetworkManager::getInterface(const char* name) {
        if (!initialized_ || !name) {
            return nullptr;
        }

        for (uint8_t i = 0; i < interfaceCount_; ++i) {
            if (interfaces_[i] && strcmp(interfaces_[i]->getName(), name) == 0) {
                return interfaces_[i];
            }
        }

        return nullptr;
    }

    NetworkInterface* NetworkManager::getInterface(uint8_t index) {
        if (!initialized_ || index >= interfaceCount_) {
            return nullptr;
        }

        return interfaces_[index];
    }

    // ==================== Default Interface Management ====================

    bool NetworkManager::setDefaultInterface(NetworkInterface* interface) {
        if (!initialized_ || !interface) {
            return false;
        }

        // Verify interface is registered
        if (findInterfaceIndex(interface) == INVALID_INTERFACE_INDEX) {
            LOG_ERROR("NetworkManager: Cannot set unregistered interface as default");
            return false;
        }

        defaultInterface_ = interface;
        LOG_INFO("NetworkManager: Set '%s' as default interface", interface->getName());
        return true;
    }

    bool NetworkManager::setDefaultInterface(const char* name) {
        NetworkInterface* interface = getInterface(name);
        if (!interface) {
            LOG_ERROR("NetworkManager: Interface '%s' not found", name ? name : "null");
            return false;
        }

        return setDefaultInterface(interface);
    }

    // ==================== Packet Routing ====================

    bool NetworkManager::sendPacket(const uint8_t* data, uint32_t length) {
        if (!initialized_) {
            LOG_ERROR("NetworkManager: Not initialized");
            return false;
        }

        if (!defaultInterface_) {
            LOG_ERROR("NetworkManager: No default interface set");
            return false;
        }

        if (!defaultInterface_->isUp()) {
            LOG_ERROR("NetworkManager: Default interface '%s' is down",
                     defaultInterface_->getName());
            return false;
        }

        return defaultInterface_->sendPacket(data, length);
    }

    bool NetworkManager::sendPacketTo(const char* interfaceName, const uint8_t* data,
                                      uint32_t length) {
        NetworkInterface* interface = getInterface(interfaceName);
        if (!interface) {
            LOG_ERROR("NetworkManager: Interface '%s' not found", interfaceName);
            return false;
        }

        if (!interface->isUp()) {
            LOG_ERROR("NetworkManager: Interface '%s' is down", interfaceName);
            return false;
        }

        return interface->sendPacket(data, length);
    }

    // ==================== Debug & Enumeration ====================

    void NetworkManager::listInterfaces() {
        if (!initialized_) {
            LOG_WARN("NetworkManager: Not initialized");
            return;
        }

        LOG_INFO("========================================");
        LOG_INFO("Network Interfaces (%u registered):", interfaceCount_);
        LOG_INFO("========================================");

        if (interfaceCount_ == 0) {
            LOG_INFO("  No interfaces registered");
            LOG_INFO("========================================");
            return;
        }

        for (uint8_t i = 0; i < interfaceCount_; ++i) {
            NetworkInterface* iface = interfaces_[i];
            if (!iface) continue;

            const char* name = iface->getName();
            const uint8_t* mac = iface->getMACAddress();
            uint32_t ip = iface->getIPAddress();
            bool isDefault = (iface == defaultInterface_);

            const char* state_str = (iface->getState() == NetworkInterface::State::Up) ? "UP" :
                                   (iface->getState() == NetworkInterface::State::Down) ? "DOWN" : "ERROR";

            LOG_INFO("");
            LOG_INFO("  %s%s:", name, isDefault ? " (default)" : "");
            LOG_INFO("    MAC:   %02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

            // Format and display IP address
            if (ip != 0) {
                uint8_t ip_bytes[4] = {
                    static_cast<uint8_t>((ip >> 24) & 0xFF),
                    static_cast<uint8_t>((ip >> 16) & 0xFF),
                    static_cast<uint8_t>((ip >> 8) & 0xFF),
                    static_cast<uint8_t>(ip & 0xFF)
                };
                LOG_INFO("    IP:    %u.%u.%u.%u",
                         ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
            } else {
                LOG_INFO("    IP:    Not configured");
            }

            LOG_INFO("    State: %s", state_str);
            LOG_INFO("    MTU:   %u", iface->getMTU());
            LOG_INFO("    TX:    %llu packets, %llu bytes, %u errors",
                     iface->getTxPackets(), iface->getTxBytes(), iface->getTxErrors());
            LOG_INFO("    RX:    %llu packets, %llu bytes, %u errors, %u dropped",
                     iface->getRxPackets(), iface->getRxBytes(),
                     iface->getRxErrors(), iface->getRxDropped());
        }

        LOG_INFO("========================================");
    }

    // ==================== Helper Methods ====================

    uint8_t NetworkManager::findInterfaceIndex(const NetworkInterface* interface) {
        for (uint8_t i = 0; i < interfaceCount_; ++i) {
            if (interfaces_[i] == interface) {
                return i;
            }
        }
        return INVALID_INTERFACE_INDEX;
    }

}  // namespace PalmyraOS::kernel
