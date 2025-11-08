#include "core/network/PCnetDriver.h"
#include "core/kernel.h"
#include "core/network/ARP.h"
#include "core/network/Ethernet.h"
#include "core/network/IPv4.h"
#include "core/pcie/PCIe.h"
#include "core/peripherals/Logger.h"
#include "core/port.h"
#include "libs/memory.h"
#include "palmyraOS/shared/memory/Heap.h"

namespace PalmyraOS::kernel {

    // ==================== Constructor / Destructor ====================

    PCnetDriver::PCnetDriver(uint8_t bus, uint8_t device, uint8_t function, types::HeapManagerBase* heapManager)
        : NetworkInterface("eth0", nullptr, heapManager), bus_(bus), device_(device), function_(function), ioBase_(0), initBlock_(nullptr), initBlockRaw_(nullptr),
          txRing_(nullptr), rxRing_(nullptr), txRingRaw_(nullptr), rxRingRaw_(nullptr), currentTx_(0), currentRx_(0) {

        // Initialize buffer pointers to safe default (nullptr)
        for (uint8_t i = 0; i < TX_RING_SIZE; ++i) txBuffers_[i] = nullptr;
        for (uint8_t i = 0; i < RX_RING_SIZE; ++i) rxBuffers_[i] = nullptr;
    }

    PCnetDriver::~PCnetDriver() {
        disable();
        freeBuffers();
    }

    // ==================== Hardware Initialization ====================

    bool PCnetDriver::initialize() {
        LOG_INFO("PCnet: Initializing AMD PCnet driver");
        LOG_INFO("PCnet: PCI location [%02X:%02X.%u]", bus_, device_, function_);

        // Read BAR0 from PCI config space (I/O base address)
        LOG_DEBUG("PCnet: Reading BAR0 from PCI config space...");
        uint32_t bar0 = PCIe::readConfig32(bus_, device_, function_, 0x10);
        LOG_DEBUG("PCnet: BAR0 = 0x%08X", bar0);

        // Verify I/O space bit (bit 0 must be 1)
        if ((bar0 & 0x1) == 0) {
            LOG_ERROR("PCnet: BAR0 is not I/O space (0x%X)", bar0);
            return false;
        }

        ioBase_ = static_cast<uint16_t>(bar0 & 0xFFF0);
        LOG_INFO("PCnet: I/O Base Address: 0x%04X", ioBase_);

        // Enable PCI bus mastering and I/O space access
        LOG_DEBUG("PCnet: Enabling bus mastering and I/O space...");
        uint16_t command = PCIe::readConfig16(bus_, device_, function_, 0x04);
        LOG_DEBUG("PCnet: Current command register: 0x%04X", command);
        command |= 0x05;  // Bit 0: I/O Space, Bit 2: Bus Master
        PCIe::writeConfig16(bus_, device_, function_, 0x04, command);
        LOG_DEBUG("PCnet: Command register updated to: 0x%04X", command);

        // Perform hardware reset
        LOG_DEBUG("PCnet: Resetting controller...");
        reset();
        LOG_DEBUG("PCnet: Reset complete");

        // Read MAC address from APROM
        LOG_DEBUG("PCnet: Reading MAC address from APROM...");
        readMACAddress();
        LOG_DEBUG("PCnet: MAC address read complete");

        const uint8_t* mac = getMACAddress();
        LOG_INFO("PCnet: MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        // Switch to 32-bit software style
        LOG_DEBUG("PCnet: Switching to 32-bit mode...");
        writeBCR(BCR20, BCR20_SOFTWARE_STYLE_32BIT);
        LOG_DEBUG("PCnet: Switched to 32-bit mode");

        // Allocate DMA buffers (init block, rings, packet buffers)
        LOG_DEBUG("PCnet: Allocating DMA buffers...");
        if (!allocateBuffers()) {
            LOG_ERROR("PCnet: Failed to allocate DMA buffers");
            return false;
        }
        LOG_DEBUG("PCnet: DMA buffers allocated");

        // Initialize descriptor rings
        LOG_DEBUG("PCnet: Initializing descriptors...");
        initializeDescriptors();
        LOG_DEBUG("PCnet: Descriptors initialized");

        // Write initialization block address to CSR1/CSR2
        LOG_DEBUG("PCnet: Writing initialization block address (0x%p) to CSR1/CSR2...", initBlock_);
        uintptr_t initBlockAddr = reinterpret_cast<uintptr_t>(initBlock_);
        writeCSR(CSR1, static_cast<uint32_t>(initBlockAddr & 0xFFFF));
        writeCSR(CSR2, static_cast<uint32_t>((initBlockAddr >> 16) & 0xFFFF));
        LOG_DEBUG("PCnet: Init block address written");

        // Configure CSR3 (Interrupt Masks) - mask all interrupts during init
        LOG_DEBUG("PCnet: Configuring CSR3 (interrupt mask)...");
        writeCSR(CSR3, CSR3_INIT_MASK);

        // Configure CSR4 (Features Control)
        LOG_DEBUG("PCnet: Configuring CSR4 (features)...");
        writeCSR(CSR4, CSR4_FEATURES);

        // Configure CSR15 (Mode Register)
        LOG_DEBUG("PCnet: Configuring CSR15 (mode register)...");
        writeCSR(CSR15, CSR15_NORMAL_MODE);

        // Send INIT command to hardware
        LOG_DEBUG("PCnet: Sending INIT command to controller...");
        writeCSR(CSR0, INIT);
        LOG_DEBUG("PCnet: INIT command sent");

        // Wait for initialization to complete (IDON flag)
        LOG_DEBUG("PCnet: Waiting for IDON flag...");
        for (uint32_t i = 0; i < INIT_TIMEOUT_ITERATIONS; ++i) {
            uint32_t csr0 = readCSR(CSR0);
            if (csr0 & IDON) {
                LOG_INFO("PCnet: Initialization complete (CSR0=0x%X)", csr0);

                // Diagnostic: check descriptor state after IDON
                LOG_DEBUG("PCnet: RX desc[0] AFTER IDON: addr=0x%08X, length=0x%04X, status=0x%04X", rxRing_[0].address, rxRing_[0].length, rxRing_[0].status);
                LOG_DEBUG("PCnet: TX desc[0] AFTER IDON: addr=0x%08X, length=0x%04X, status=0x%04X", txRing_[0].address, txRing_[0].length, txRing_[0].status);

                // Configure CSR3 for normal operation (enable RX/TX interrupts)
                writeCSR(CSR3, CSR3_NORMAL_MASK);
                LOG_DEBUG("PCnet: CSR3 configured for normal operation");

                // Read back CSR values for diagnostic
                uint32_t csr3  = readCSR(CSR3);
                uint32_t csr4  = readCSR(CSR4);
                uint32_t csr15 = readCSR(CSR15);
                LOG_DEBUG("PCnet: Final CSR values - CSR3=0x%04X, CSR4=0x%04X, CSR15=0x%04X", csr3, csr4, csr15);

                setState(State::Down);  // Initialized but not enabled
                return true;
            }

            // Busy-wait delay between status polls
            for (volatile uint32_t j = 0; j < STATUS_POLL_DELAY; ++j) {}
        }

        LOG_ERROR("PCnet: Initialization timeout (IDON flag never set)");
        return false;
    }

    // ==================== MAC Address & Reset ====================

    void PCnetDriver::readMACAddress() {
        uint8_t mac[MAC_ADDRESS_SIZE];

        // Read MAC address from APROM (I/O ports 0x00-0x05)
        ports::BytePort port0(ioBase_ + APROM0);
        ports::BytePort port1(ioBase_ + APROM0 + 1);
        ports::BytePort port2(ioBase_ + APROM0 + 2);
        ports::BytePort port3(ioBase_ + APROM0 + 3);
        ports::BytePort port4(ioBase_ + APROM4);
        ports::BytePort port5(ioBase_ + APROM4 + 1);

        mac[0] = port0.read();
        mac[1] = port1.read();
        mac[2] = port2.read();
        mac[3] = port3.read();
        mac[4] = port4.read();
        mac[5] = port5.read();

        // Update base class MAC address
        memcpy(const_cast<uint8_t*>(getMACAddress()), mac, MAC_ADDRESS_SIZE);
    }

    void PCnetDriver::reset() {
        ports::WordPort resetPort(ioBase_ + RESET);
        (void) resetPort.read();  // Read RESET port to trigger hardware reset

        // Wait for reset to complete
        for (volatile uint32_t i = 0; i < RESET_COMPLETION_DELAY; ++i) {}

        LOG_DEBUG("PCnet: Hardware reset complete");
    }

    // ==================== Register Access (RAP/RDP Protocol) ====================

    uint32_t PCnetDriver::readCSR(uint16_t csr) {
        ports::WordPort rap(ioBase_ + RAP);
        ports::WordPort rdp(ioBase_ + RDP);

        rap.write(csr);
        for (volatile uint32_t i = 0; i < RAP_SETTLING_DELAY; ++i) {}

        uint16_t value = static_cast<uint16_t>(rdp.read());
        return value;
    }

    void PCnetDriver::writeCSR(uint16_t csr, uint32_t value) {
        ports::WordPort rap(ioBase_ + RAP);
        ports::WordPort rdp(ioBase_ + RDP);

        rap.write(csr);
        for (volatile uint32_t i = 0; i < RAP_SETTLING_DELAY; ++i) {}
        rdp.write(static_cast<uint16_t>(value));
    }

    uint32_t PCnetDriver::readBCR(uint16_t bcr) {
        ports::WordPort rap(ioBase_ + RAP);
        ports::WordPort bdp(ioBase_ + BDP);

        rap.write(bcr);
        for (volatile uint32_t i = 0; i < RAP_SETTLING_DELAY; ++i) {}
        return static_cast<uint16_t>(bdp.read());
    }

    void PCnetDriver::writeBCR(uint16_t bcr, uint32_t value) {
        ports::WordPort rap(ioBase_ + RAP);
        ports::WordPort bdp(ioBase_ + BDP);

        rap.write(bcr);
        for (volatile uint32_t i = 0; i < RAP_SETTLING_DELAY; ++i) {}
        bdp.write(static_cast<uint16_t>(value));
    }

    // ==================== DMA Buffer Management ====================

    bool PCnetDriver::allocateBuffers() {
        // Helper: identity-mapped DMA allocation (returns physical==virtual)
        auto allocPages = [](size_t bytes) -> void* {
            uint32_t pages = static_cast<uint32_t>((bytes + 4095) / 4096);
            return PalmyraOS::kernel::kernelPagingDirectory_ptr ? PalmyraOS::kernel::kernelPagingDirectory_ptr->allocatePages(pages) : nullptr;
        };
        // Helper: align pointer to 16-byte boundary (required for DMA)
        auto alignTo16 = [](void* ptr) -> void* {
            uintptr_t addr    = reinterpret_cast<uintptr_t>(ptr);
            uintptr_t aligned = (addr + 15) & ~0xF;  // Round up to 16-byte boundary
            return reinterpret_cast<void*>(aligned);
        };

        // Allocate Initialization Block (4-byte aligned minimum)
        initBlockRaw_ = allocPages(sizeof(InitBlock));
        if (!initBlockRaw_) {
            LOG_ERROR("PCnet: Failed to allocate initialization block");
            return false;
        }
        initBlock_ = static_cast<InitBlock*>(initBlockRaw_);
        memset(initBlock_, 0, sizeof(InitBlock));

        // Allocate TX ring with alignment (16-byte aligned REQUIRED)
        txRingRaw_ = allocPages(sizeof(TxDescriptor) * TX_RING_SIZE + 16);
        if (!txRingRaw_) {
            LOG_ERROR("PCnet: Failed to allocate TX ring");
            return false;
        }
        txRing_ = static_cast<TxDescriptor*>(alignTo16(txRingRaw_));
        memset(txRing_, 0, sizeof(TxDescriptor) * TX_RING_SIZE);

        // Allocate RX ring with alignment (16-byte aligned REQUIRED)
        rxRingRaw_ = allocPages(sizeof(RxDescriptor) * RX_RING_SIZE + 16);
        if (!rxRingRaw_) {
            LOG_ERROR("PCnet: Failed to allocate RX ring");
            return false;
        }
        rxRing_ = static_cast<RxDescriptor*>(alignTo16(rxRingRaw_));
        memset(rxRing_, 0, sizeof(RxDescriptor) * RX_RING_SIZE);

        // Verify alignment (critical for DMA!)
        if (reinterpret_cast<uintptr_t>(txRing_) % 16 != 0 || reinterpret_cast<uintptr_t>(rxRing_) % 16 != 0) {
            LOG_ERROR("PCnet: Descriptor ring alignment check FAILED!");
            return false;
        }

        // Allocate TX packet buffers
        for (uint8_t i = 0; i < TX_RING_SIZE; ++i) {
            txBuffers_[i] = static_cast<uint8_t*>(allocPages(BUFFER_SIZE));
            if (!txBuffers_[i]) {
                LOG_ERROR("PCnet: Failed to allocate TX buffer %u", i);
                return false;
            }
            memset(txBuffers_[i], 0, BUFFER_SIZE);
        }

        // Allocate RX packet buffers
        for (uint8_t i = 0; i < RX_RING_SIZE; ++i) {
            rxBuffers_[i] = static_cast<uint8_t*>(allocPages(BUFFER_SIZE));
            if (!rxBuffers_[i]) {
                LOG_ERROR("PCnet: Failed to allocate RX buffer %u", i);
                return false;
            }
            memset(rxBuffers_[i], 0, BUFFER_SIZE);
        }

        LOG_DEBUG("PCnet: DMA buffers allocated successfully "
                  "(TX ring: 0x%p, RX ring: 0x%p)",
                  txRing_,
                  rxRing_);
        return true;
    }

    void PCnetDriver::freeBuffers() {
        // DMA buffers were allocated via identity-mapped page allocations.
        // For now, skip freeing (persist for driver lifetime) and just clear pointers.
        for (uint8_t i = 0; i < TX_RING_SIZE; ++i) { txBuffers_[i] = nullptr; }
        for (uint8_t i = 0; i < RX_RING_SIZE; ++i) { rxBuffers_[i] = nullptr; }
        txRing_       = nullptr;
        rxRing_       = nullptr;
        txRingRaw_    = nullptr;
        rxRingRaw_    = nullptr;
        initBlock_    = nullptr;
        initBlockRaw_ = nullptr;
    }

    // ==================== Descriptor Ring Setup ====================

    void PCnetDriver::initializeDescriptors() {
        // Setup Initialization Block
        initBlock_->mode   = INIT_MODE_NORMAL;  // Normal mode (not promiscuous)
        initBlock_->rlen   = (3 << 4);          // RX ring log2(8) in upper nibble
        initBlock_->tlen   = (3 << 4);          // TX ring log2(8) in upper nibble

        // Copy MAC address to init block
        const uint8_t* mac = getMACAddress();
        memcpy(initBlock_->mac, mac, MAC_ADDRESS_SIZE);

        initBlock_->reserved   = 0;
        initBlock_->ladrf[0]   = LADRF_NO_MULTICAST;  // Reject all multicast
        initBlock_->ladrf[1]   = LADRF_NO_MULTICAST;  // Reject all multicast
        initBlock_->rxRingAddr = reinterpret_cast<uintptr_t>(rxRing_);
        initBlock_->txRingAddr = reinterpret_cast<uintptr_t>(txRing_);

        // Log init block configuration
        LOG_DEBUG("PCnet: Init Block @ 0x%p:", initBlock_);
        LOG_DEBUG("  mode=0x%04X, rlen=0x%02X, tlen=0x%02X", initBlock_->mode, initBlock_->rlen, initBlock_->tlen);
        LOG_DEBUG("  mac=%02X:%02X:%02X:%02X:%02X:%02X", initBlock_->mac[0], initBlock_->mac[1], initBlock_->mac[2], initBlock_->mac[3], initBlock_->mac[4], initBlock_->mac[5]);
        LOG_DEBUG("  rxRingAddr=0x%08X (alignment: %u), txRingAddr=0x%08X (alignment: %u)",
                  initBlock_->rxRingAddr,
                  initBlock_->rxRingAddr % 16,
                  initBlock_->txRingAddr,
                  initBlock_->txRingAddr % 16);

        // Initialize TX descriptors (CPU owns initially, DESC_OWN=0)
        for (uint8_t i = 0; i < TX_RING_SIZE; ++i) {
            txRing_[i].address  = reinterpret_cast<uintptr_t>(txBuffers_[i]);
            txRing_[i].length   = 0;  // Set when sending
            txRing_[i].status   = 0;  // CPU owns (DESC_OWN=0)
            txRing_[i].misc     = 0;
            txRing_[i].reserved = 0;
        }

        // Initialize RX descriptors (NIC owns initially, DESC_OWN=1)
        for (uint8_t i = 0; i < RX_RING_SIZE; ++i) {
            rxRing_[i].address  = reinterpret_cast<uintptr_t>(rxBuffers_[i]);
            rxRing_[i].length   = static_cast<uint16_t>(-BUFFER_SIZE);  // 2's complement
            rxRing_[i].status   = DESC_OWN;                             // NIC owns initially
            rxRing_[i].misc     = 0;
            rxRing_[i].reserved = 0;
        }

        currentTx_ = 0;
        currentRx_ = 0;

        LOG_DEBUG("PCnet: Descriptor rings initialized (TX ring @ 0x%p, RX ring @ 0x%p)", txRing_, rxRing_);
        LOG_DEBUG("PCnet: RX buffer[0] @ 0x%p, TX buffer[0] @ 0x%p", rxBuffers_[0], txBuffers_[0]);
        LOG_DEBUG("PCnet: RX desc[0] AFTER INIT: addr=0x%08X, length=0x%04X, status=0x%04X", rxRing_[0].address, rxRing_[0].length, rxRing_[0].status);
        LOG_DEBUG("PCnet: TX desc[0] AFTER INIT: addr=0x%08X, length=0x%04X, status=0x%04X", txRing_[0].address, txRing_[0].length, txRing_[0].status);
    }

    // ==================== Packet Transmission ====================

    bool PCnetDriver::sendPacket(const uint8_t* data, uint32_t length) {
        if (!isUp()) {
            return false;  // Interface must be UP
        }

        if (length > BUFFER_SIZE) {
            LOG_ERROR("PCnet: Packet too large (%u bytes, max %u)", length, BUFFER_SIZE);
            updateStatistics(length, true, true);
            return false;
        }

        // Enforce Ethernet minimum frame size (driver-side padding)
        uint32_t txLen = length;
        if (txLen < ethernet::MIN_FRAME_SIZE) { txLen = ethernet::MIN_FRAME_SIZE; }

        // Check if current TX descriptor is available (CPU must own it)
        if (txRing_[currentTx_].status & DESC_OWN) {
            LOG_WARN("PCnet: TX ring full");
            updateStatistics(length, true, true);
            return false;
        }

        // Copy packet to TX buffer
        memcpy(txBuffers_[currentTx_], data, length);
        if (txLen > length) {
            // Zero-pad remainder to meet minimum Ethernet frame size
            memset(txBuffers_[currentTx_] + length, 0, txLen - length);
        }

        // Setup TX descriptor
        txRing_[currentTx_].length = static_cast<uint16_t>(-static_cast<int16_t>(txLen));  // 2's complement of actual TX length
        txRing_[currentTx_].misc   = 0;
        txRing_[currentTx_].status = DESC_OWN | DESC_STP | DESC_ENP;  // Give to NIC

        // Signal NIC: Transmit Demand
        writeCSR(CSR0, readCSR(CSR0) | TDMD);

        // Optional: brief poll for TX descriptor ownership to clear (TX complete)
        // This is diagnostic and should be removed in high-throughput paths
        {
            const uint8_t descIndex = currentTx_;
            for (uint32_t i = 0; i < 10000; ++i) {
                if ((txRing_[descIndex].status & DESC_OWN) == 0) {
                    break;  // NIC returned ownership
                }
            }
        }

        // Update statistics (success) with actual transmitted length
        updateStatistics(txLen, true, false);

        // Advance to next descriptor (round-robin)
        currentTx_ = (currentTx_ + 1) % TX_RING_SIZE;

        return true;
    }

    // ==================== Interface Control ====================

    bool PCnetDriver::enable() {
        if (!initBlock_) {
            LOG_ERROR("PCnet: Cannot enable - not initialized");
            return false;
        }

        // Read current CSR0
        uint32_t csr0 = readCSR(CSR0);
        LOG_DEBUG("PCnet: Current CSR0=0x%X before START", csr0);

        // Verify initialization completed (IDON bit must be set)
        if (!(csr0 & IDON)) {
            LOG_ERROR("PCnet: Cannot start - IDON not set (CSR0=0x%X)", csr0);
            return false;
        }

        // Prepare START command: clear errors, set START + INEA
        csr0 &= ~(ERR | TINT | RINT);  // Clear error and interrupt flags
        csr0 |= (STRT | INEA);         // Set START and enable interrupts

        LOG_DEBUG("PCnet: Sending START command (CSR0=0x%X)...", csr0);
        writeCSR(CSR0, csr0);
        LOG_DEBUG("PCnet: START command sent");

        // Wait for TX/RX to come online
        LOG_DEBUG("PCnet: Waiting for TXON/RXON flags...");
        for (uint32_t i = 0; i < START_TIMEOUT_ITERATIONS; ++i) {
            csr0 = readCSR(CSR0);
            if ((csr0 & (TXON | RXON)) == (TXON | RXON)) {
                LOG_INFO("PCnet: Interface enabled (TX/RX online, CSR0=0x%X)", csr0);
                setState(State::Up);
                return true;
            }

            // Log progress at key iterations
            if (i == 0 || i == 10 || i == 50 || i == 100 || i == 500 || i == START_TIMEOUT_ITERATIONS - 1) {
                LOG_DEBUG("PCnet: CSR0=0x%X (iteration %u, TXON=%d, RXON=%d)", csr0, i, !!(csr0 & TXON), !!(csr0 & RXON));
            }

            // Busy-wait delay
            for (volatile uint32_t j = 0; j < STATUS_POLL_DELAY; ++j) {}
        }

        uint32_t finalCSR0 = readCSR(CSR0);
        LOG_ERROR("PCnet: Failed to bring interface up (final CSR0=0x%X)", finalCSR0);

        // Diagnostic info
        LOG_DEBUG("PCnet: First RX descriptor: addr=0x%08X, length=0x%04X, status=0x%04X", rxRing_[0].address, rxRing_[0].length, rxRing_[0].status);
        LOG_DEBUG("PCnet: First TX descriptor: addr=0x%08X, length=0x%04X, status=0x%04X", txRing_[0].address, txRing_[0].length, txRing_[0].status);

        return false;
    }

    bool PCnetDriver::disable() {
        if (!initBlock_) {
            return true;  // Already disabled
        }

        // Send STOP command
        writeCSR(CSR0, STOP);

        LOG_INFO("PCnet: Interface disabled");
        setState(State::Down);
        return true;
    }

    // ==================== Interrupt Handling ====================

    void PCnetDriver::handleInterrupt() {
        // Read interrupt status
        uint32_t csr0 = readCSR(CSR0);

        // ALWAYS poll for received packets (for ARP polling)
        // In a real OS, this would only be called on actual interrupts
        processReceivedPackets();

        // Handle RX interrupt (clear flag if set)
        if (csr0 & RINT) {
            writeCSR(CSR0, csr0 | RINT);  // Clear RX interrupt
        }

        // Handle TX interrupt
        if (csr0 & TINT) {
            // TX complete - nothing to do yet
            writeCSR(CSR0, csr0 | TINT);  // Clear TX interrupt
        }

        // Handle error condition
        if (csr0 & ERR) {
            LOG_ERROR("PCnet: Error interrupt (CSR0=0x%X)", csr0);
            writeCSR(CSR0, csr0 | ERR);  // Clear error flag
        }
    }

    // ==================== Received Packet Processing ====================

    void PCnetDriver::processReceivedPackets() {
        uint32_t packetsProcessedCount = 0;

        // Process all RX descriptors owned by CPU (hardware has released ownership)
        while (!(rxRing_[currentRx_].status & DESC_OWN)) {
            const uint16_t descriptorStatus = rxRing_[currentRx_].status;
            const uint32_t frameLength      = rxRing_[currentRx_].misc & 0xFFF;  // Extract 12-bit length

            // Check for hardware-reported errors
            if (descriptorStatus & DESC_ERR) {
                LOG_WARN("PCnet: Receive error on descriptor %u (status=0x%04X)", currentRx_, descriptorStatus);
                updateStatistics(0, false, true);
            }
            // Process complete packet (both STP and ENP flags set)
            else if ((descriptorStatus & (DESC_STP | DESC_ENP)) == (DESC_STP | DESC_ENP)) {
                // Validate Ethernet frame length (14-byte header min, 1518-byte max)
                constexpr uint32_t ETHERNET_MIN_FRAME_LENGTH = 14;
                constexpr uint32_t ETHERNET_MAX_FRAME_LENGTH = 1518;

                if (frameLength >= ETHERNET_MIN_FRAME_LENGTH && frameLength <= ETHERNET_MAX_FRAME_LENGTH) {
                    const uint8_t* receivedFrameBuffer = rxBuffers_[currentRx_];

                    if (receivedFrameBuffer != nullptr) {
                        // Extract EtherType from Ethernet header (bytes 12-13, network byte order)
                        const uint16_t etherType = static_cast<uint16_t>((receivedFrameBuffer[12] << 8) | receivedFrameBuffer[13]);

                        // Dispatch to protocol handler based on EtherType
                        if (etherType == ethernet::ETHERTYPE_ARP) { ARP::handleARPPacket(receivedFrameBuffer, frameLength); }
                        else if (etherType == ethernet::ETHERTYPE_IPV4) { IPv4::handleIPv4Packet(receivedFrameBuffer, frameLength); }

                        // Update interface statistics (successful packet reception)
                        updateStatistics(frameLength, false, false);
                        packetsProcessedCount++;
                    }
                }
                else {
                    LOG_WARN("PCnet: Invalid frame length (%u bytes, expected %u-%u)", frameLength, ETHERNET_MIN_FRAME_LENGTH, ETHERNET_MAX_FRAME_LENGTH);
                    updateStatistics(0, false, true);
                }
            }

            // Return descriptor ownership to NIC for buffer reuse
            rxRing_[currentRx_].status = DESC_OWN;
            rxRing_[currentRx_].misc   = 0;

            // Advance to next descriptor (circular buffer with wraparound)
            currentRx_                 = (currentRx_ + 1) % RX_RING_SIZE;
        }
    }

}  // namespace PalmyraOS::kernel
