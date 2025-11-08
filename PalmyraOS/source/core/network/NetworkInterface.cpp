#include "core/network/NetworkInterface.h"
#include "core/kernel.h"
#include "libs/memory.h"
#include "libs/string.h"

namespace PalmyraOS::kernel {

    // ==================== Custom Memory Management (Freestanding C++) ====================

    void* NetworkInterface::operator new(size_t size) { return kernel::kmalloc(size); }

    void* NetworkInterface::operator new(size_t size, void* ptr) noexcept {
        return ptr;  // Placement new - memory already allocated
    }

    void NetworkInterface::operator delete(void* ptr) noexcept {
        if (ptr) kernel::heapManager.free(ptr);
    }

    void NetworkInterface::operator delete(void* ptr, size_t size) noexcept {
        if (ptr) kernel::heapManager.free(ptr);
    }

    // ==================== Constructor ====================

    NetworkInterface::NetworkInterface(const char* name, const uint8_t macAddress[MAC_ADDRESS_SIZE], types::HeapManagerBase* heapManager)
        : heapManager_(heapManager), ipAddress_(0), subnetMask_(0), gateway_(0), state_(State::Down), mtu_(STANDARD_MTU), promiscuousMode_(false), txPackets_(0), rxPackets_(0),
          txBytes_(0), rxBytes_(0), txErrors_(0), rxErrors_(0), rxDropped_(0) {

        // Copy interface name (max 15 chars + null terminator)
        memset(name_, 0, sizeof(name_));
        if (name) { strncpy(name_, name, MAX_NAME_LENGTH); }

        // Copy or zero MAC address
        if (macAddress) { memcpy(macAddress_, macAddress, MAC_ADDRESS_SIZE); }
        else {
            memset(macAddress_, 0, MAC_ADDRESS_SIZE);  // Driver will populate from hardware
        }
    }

    // ==================== Virtual Methods ====================

    bool NetworkInterface::enable() {
        if (state_ == State::Error) {
            return false;  // Cannot enable error state
        }
        state_ = State::Up;
        return true;
    }

    bool NetworkInterface::disable() {
        state_ = State::Down;
        return true;
    }

    void NetworkInterface::handleInterrupt() {
        // Default: polling mode (do nothing)
    }

    // ==================== Configuration Methods ====================

    void NetworkInterface::setIPAddress(uint32_t ip) { ipAddress_ = ip; }

    void NetworkInterface::setSubnetMask(uint32_t mask) { subnetMask_ = mask; }

    void NetworkInterface::setGateway(uint32_t gateway) { gateway_ = gateway; }

    void NetworkInterface::setMTU(uint16_t mtu) { mtu_ = mtu; }

    void NetworkInterface::setPromiscuousMode(bool enabled) { promiscuousMode_ = enabled; }

    // ==================== Statistics Management ====================

    void NetworkInterface::updateStatistics(uint32_t bytes, bool isTx, bool isError) {
        if (isTx) {
            if (isError) { txErrors_++; }
            else {
                txPackets_++;
                txBytes_ += bytes;
            }
        }
        else {
            if (isError) { rxErrors_++; }
            else {
                rxPackets_++;
                rxBytes_ += bytes;
            }
        }
    }

    void NetworkInterface::resetStatistics() {
        txPackets_ = 0;
        rxPackets_ = 0;
        txBytes_   = 0;
        rxBytes_   = 0;
        txErrors_  = 0;
        rxErrors_  = 0;
        rxDropped_ = 0;
    }

}  // namespace PalmyraOS::kernel
