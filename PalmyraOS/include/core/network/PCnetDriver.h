#pragma once

#include "core/definitions.h"
#include "core/network/NetworkInterface.h"

namespace PalmyraOS::kernel {

    /**
     * @brief AMD PCnet-PCI II (Am79C970A) Network Driver
     *
     * Driver for AMD PCnet Ethernet controllers commonly found in:
     * - VirtualBox (default network adapter)
     * - VMware, QEMU, VirtualPC
     * - Legacy servers and laptops (2000-2010 era)
     *
     * **Hardware Specifications:**
     * - Vendor ID: 0x1022 (AMD)
     * - Device ID: 0x2000 (PCnet-PCI II)
     * - Speed: 10/100 Mbps (auto-negotiating)
     * - Duplex: Full duplex
     * - DMA Model: Descriptor-based with ownership flags
     * - Architecture: I/O-mapped with CSR/BCR register access
     *
     * **Architecture Highlights:**
     * - Word I/O (WIO) mode for portability across PCnet variants
     * - 16-byte aligned descriptor rings (TX + RX)
     * - Dynamic MAC address reading from APROM
     * - Interrupt-driven packet reception
     * - Full statistics tracking (packets, bytes, errors)
     *
     * For complete hardware specifications, see:
     * - `PalmyraOS/include/core/network/PCNET_DRIVER_SPEC.md`
     * - AMD Am79C970A datasheet (PCnet-PCI II Technical Reference)
     *
     * @see NetworkInterface (base class)
     * @see NetworkManager (registration)
     * @see PCNET_DRIVER_SPEC.md (detailed technical documentation)
     */
    class PCnetDriver : public NetworkInterface {
    public:
        // ==================== Configuration Constants ====================

        /// @brief Number of descriptors in TX ring (affects max concurrent TX)
        static constexpr uint8_t TX_RING_SIZE                = 8;

        /// @brief Number of descriptors in RX ring (packet buffering)
        static constexpr uint8_t RX_RING_SIZE                = 8;

        /// @brief Size of each DMA buffer (MTU + headers + CRC)
        static constexpr uint16_t BUFFER_SIZE                = 1536;

        // ==================== Hardware Timing Constants ====================

        /// @brief RAP (Register Address Port) settling delay (cycles)
        /// After writing RAP, we wait this many iterations before RDP access
        static constexpr uint32_t RAP_SETTLING_DELAY         = 10;

        /// @brief Hardware reset completion delay (cycles)
        /// After reading RESET register, wait for reset to complete
        static constexpr uint32_t RESET_COMPLETION_DELAY     = 100000;

        /// @brief INIT command completion timeout (iterations)
        /// Maximum iterations waiting for IDON (Initialization Done) flag
        static constexpr uint32_t INIT_TIMEOUT_ITERATIONS    = 1000;

        /// @brief START/enable timeout (iterations)
        /// Maximum iterations waiting for TXON/RXON flags after START command
        static constexpr uint32_t START_TIMEOUT_ITERATIONS   = 1000;

        /// @brief Busy-wait delay between status polling (cycles)
        /// Inner loop delay between CSR0 reads during INIT/START
        static constexpr uint32_t STATUS_POLL_DELAY          = 10000;

        // ==================== CSR Register Constants ====================

        /// @brief CSR3 value to mask all interrupts during initialization
        static constexpr uint16_t CSR3_INIT_MASK             = 0x5F00;

        /// @brief CSR3 value to enable RX/TX interrupts for normal operation
        static constexpr uint16_t CSR3_NORMAL_MASK           = 0x0040;

        /// @brief CSR4 value for features control
        /// Enables full-duplex and auto-strip padding for clean packet reception
        static constexpr uint16_t CSR4_FEATURES              = 0x0915;

        /// @brief CSR15 value for normal mode (no special features)
        static constexpr uint16_t CSR15_NORMAL_MODE          = 0x0000;

        // ==================== BCR Register Constants ====================

        /// @brief BCR20 value to set 32-bit software style
        /// Required for 32-bit access to RDP/BDP registers
        static constexpr uint16_t BCR20_SOFTWARE_STYLE_32BIT = 0x0102;

        // ==================== Initialization Block Constants ====================

        /// @brief Initialization block mode (normal mode, not promiscuous)
        /// Receive frames matching this MAC address only
        static constexpr uint16_t INIT_MODE_NORMAL           = 0x0000;

        /// @brief Multicast filter - reject all multicast initially
        static constexpr uint32_t LADRF_NO_MULTICAST         = 0x00000000;

        // ==================== Lifecycle ====================

        /**
         * @brief Constructor
         *
         * Creates a PCnet driver instance for a specific PCI device.
         * Does NOT initialize hardware - that's done in initialize().
         *
         * @param bus PCI bus number (0-255)
         * @param device PCI device number (0-31)
         * @param function PCI function number (0-7)
         * @param heapManager Heap allocator for DMA buffers (dependency injection)
         *
         * @note PCI location must be valid - no validation done here
         * @note Constructor initializes member variables to safe defaults
         */
        PCnetDriver(uint8_t bus, uint8_t device, uint8_t function, types::HeapManagerBase* heapManager);

        /// @brief Destructor - disables and frees hardware resources
        ~PCnetDriver() override;

        // ==================== NetworkInterface Implementation ====================

        /**
         * @brief Initialize PCnet hardware
         *
         * **Initialization Sequence:**
         * 1. Read BAR0 from PCI config space (I/O base address)
         * 2. Enable bus mastering and I/O space in PCI command register
         * 3. Perform hardware reset (read RESET register)
         * 4. Read MAC address from APROM (BIOS extension ROM)
         * 5. Switch to 32-bit mode via BCR20 (Software Style = 2)
         * 6. Allocate DMA buffers (init block, TX/RX rings, packet buffers)
         * 7. Initialize descriptor rings with correct ownership flags
         * 8. Configure CSR3/CSR4/CSR15 registers
         * 9. Write initialization block address to CSR1/CSR2
         * 10. Send INIT command and wait for IDON flag
         * 11. Configure CSR3 for normal operation
         *
         * @return true if successful, false if:
         *         - BAR0 is not I/O space (bit 0 = 0)
         *         - DMA buffer allocation fails
         *         - IDON flag not set after timeout
         *
         * @note Takes ~1-2 ms to complete
         * @note Does NOT enable TX/RX - call enable() for that
         * @note Idempotent - safe to call multiple times (second call is no-op)
         */
        bool initialize() override;

        /**
         * @brief Transmit Ethernet packet
         *
         * **Packet Transmission Process:**
         * 1. Check interface is UP and frame length valid
         * 2. Obtain next TX descriptor (currentTx_)
         * 3. Verify descriptor not owned by NIC (DESC_OWN = 0)
         * 4. Copy packet to TX buffer
         * 5. Setup descriptor (length in 2's complement, set STP/ENP)
         * 6. Give descriptor to NIC (set DESC_OWN = 1)
         * 7. Signal NIC via TDMD bit in CSR0
         * 8. Move to next descriptor (currentTx_ = (currentTx_ + 1) % 8)
         *
         * @param data Ethernet frame (must be valid 60-1518 byte frame)
         * @param length Frame length in bytes
         * @return true if queued for transmission, false if:
         *         - Interface is DOWN
         *         - Frame too large (> BUFFER_SIZE)
         *         - TX ring full (all descriptors owned by NIC)
         *
         * @note Fire-and-forget - completion signaled via TINT interrupt
         * @note Statistics updated: txPackets_, txBytes_, or txErrors_
         *
         * **Timing:** Typically queues in <10µs
         * **NIC Handling:** Takes 10-100µs to transmit on 100Mbps link
         */
        bool sendPacket(const uint8_t* data, uint32_t length) override;

        /**
         * @brief Enable TX/RX on hardware
         *
         * **Startup Sequence:**
         * 1. Read current CSR0 status
         * 2. Verify IDON flag is set (init completed)
         * 3. Clear error flags (ERR, TINT, RINT)
         * 4. Set START and INEA (Interrupt Enable) bits
         * 5. Poll CSR0 until TXON and RXON both set
         * 6. Change state to UP on success
         *
         * @return true if TX/RX enabled successfully, false if:
         *         - Not initialized (initBlock_ == nullptr)
         *         - IDON flag not set (init failed)
         *         - TXON/RXON don't set after timeout
         *
         * @note Takes ~100-200µs typically, up to 10ms at timeout
         * @note Enables interrupt generation for RX packets (RINT)
         * @note Must be called before sendPacket() has effect
         */
        bool enable() override;

        /**
         * @brief Disable TX/RX on hardware
         *
         * **Shutdown Sequence:**
         * 1. Write STOP bit to CSR0
         * 2. Change state to DOWN
         * 3. Wait for hardware to acknowledge (optional, not done here)
         *
         * @return true if disabled successfully, false if not initialized
         *
         * @note Fast operation (<100µs)
         * @note Clears DESC_OWN on remaining descriptors so OS can reclaim buffers
         * @note After disable(), sendPacket() will fail (interface DOWN)
         */
        bool disable() override;

        /**
         * @brief Process hardware interrupt
         *
         * **Interrupt Handling:**
         * 1. Read CSR0 to get interrupt status
         * 2. Check RINT (Receive Interrupt) - process received packets
         * 3. Check TINT (Transmit Interrupt) - update TX statistics
         * 4. Check ERR (Error Interrupt) - log error conditions
         * 5. Clear interrupt flags by writing them back to CSR0
         *
         * **Processing Details:**
         * - RINT calls processReceivedPackets() to walk RX ring
         * - TINT just clears flag (no TX buffer cleanup yet)
         * - ERR logs error for debugging
         *
         * @note Called from interrupt handler (ISR context)
         * @note Must be fast (<100µs typical with 8 packets)
         * @note Statistics updated automatically
         *
         * @see handleInterrupt() in NetworkInterface (base class)
         * @see processReceivedPackets() (private - processes RX ring)
         */
        void handleInterrupt() override;

    private:
        // ==================== Hardware Register Definitions ====================

        /**
         * @brief I/O Port Offsets (WIO Mode - Word I/O)
         *
         * PCnet supports two I/O access modes:
         * - **WIO (Word I/O)**: RAP=0x12, RDP=0x10, BDP=0x16 (16-bit access)
         * - **DWIO (DWord I/O)**: RAP=0x14, RDP=0x10, BDP=0x1C (32-bit access)
         *
         * We use WIO universally for compatibility across all PCnet variants.
         * RAP must be written with CSR/BCR number before each RDP/BDP access.
         */
        enum IOPort : uint16_t {
            APROM0 = 0x00,  ///< BIOS extension ROM - MAC bytes 0-3 (byte I/O)
            APROM4 = 0x04,  ///< BIOS extension ROM - MAC bytes 4-5 (byte I/O)
            RDP    = 0x10,  ///< Register Data Port (read/write data for selected CSR)
            RAP    = 0x12,  ///< Register Address Port (select CSR/BCR number)
            RESET  = 0x18,  ///< Hardware Reset (reading triggers reset)
            BDP    = 0x16,  ///< Bus Configuration Register Data Port (BCR access)
        };

        /**
         * @brief Control Status Registers (CSRs)
         *
         * All CSR access is via RAP/RDP protocol:
         * 1. Write CSR number to RAP
         * 2. Wait RAP_SETTLING_DELAY cycles
         * 3. Read/write data from/to RDP
         */
        enum CSR : uint16_t {
            CSR0  = 0,   ///< Status and Control (interrupts, init, start/stop)
            CSR1  = 1,   ///< Init Block Address Low (16-bit)
            CSR2  = 2,   ///< Init Block Address High (16-bit)
            CSR3  = 3,   ///< Interrupt Masks and Deferral Control
            CSR4  = 4,   ///< Test and Features Control (duplex, padding, etc.)
            CSR5  = 5,   ///< Extended Control and Interrupt
            CSR15 = 15,  ///< Mode Register
        };

        /**
         * @brief CSR0 Status and Control Bits
         *
         * Layout (32-bit register, but only lower 16 bits used):
         * - Bit 0: INIT - Initialize (write 1, hardware clears on IDON)
         * - Bit 1: STRT - Start (TX/RX) - write 1 to enable
         * - Bit 2: STOP - Stop - write 1 to halt
         * - Bit 3: TDMD - Transmit Demand - write 1 to signal pending TX
         * - Bit 4: TXON - TX Online (read-only) - 1 = transmitting
         * - Bit 5: RXON - RX Online (read-only) - 1 = receiving
         * - Bit 6: INEA - Interrupt Enable - write 1 to enable interrupts
         * - Bit 7: INTR - Interrupt Flag (read-only) - 1 = interrupt pending
         * - Bit 8: IDON - Initialization Done (read-only) - hardware sets after init
         * - Bit 9: TINT - Transmit Interrupt - write 1 to clear TX interrupt
         * - Bit 10: RINT - Receive Interrupt - write 1 to clear RX interrupt
         * - Bit 15: ERR - Error - write 1 to clear error condition
         */
        enum CSR0Bits : uint32_t {
            INIT = (1 << 0),   ///< Initialize (write 1, hw clears on IDON)
            STRT = (1 << 1),   ///< Start TX/RX (write 1)
            STOP = (1 << 2),   ///< Stop TX/RX (write 1)
            TDMD = (1 << 3),   ///< Transmit Demand (write 1 for pending TX)
            TXON = (1 << 4),   ///< Transmitter On (read-only, hw sets)
            RXON = (1 << 5),   ///< Receiver On (read-only, hw sets)
            INEA = (1 << 6),   ///< Interrupt Enable (write 1)
            INTR = (1 << 7),   ///< Interrupt Flag (read-only)
            IDON = (1 << 8),   ///< Initialization Done (read-only, hw sets)
            TINT = (1 << 9),   ///< Transmit Interrupt (write 1 to clear)
            RINT = (1 << 10),  ///< Receive Interrupt (write 1 to clear, was bit 8 in old HW)
            ERR  = (1 << 15),  ///< Error (write 1 to clear)
        };

        /**
         * @brief Bus Configuration Registers (BCRs)
         *
         * BCRs control hardware behavior and are accessed via RAP/BDP protocol.
         * Only BCR20 (Software Style) is commonly used for 32-bit access setup.
         */
        enum BCR : uint16_t {
            BCR20 = 20,  ///< Software Style (2 = 32-bit mode)
        };

        // ==================== Descriptor Structures ====================

        /**
         * @brief Transmit Descriptor (16 bytes, 32-bit DMA mode)
         *
         * One descriptor per TX packet. Arrays of 8 form the TX ring.
         * TX ring MUST be 16-byte aligned for DMA!
         *
         * Layout (little-endian):
         * - [0:3]: Buffer address (32-bit physical)
         * - [4:5]: Buffer length in 2's complement form
         *          Example: 60 bytes -> -60 = 0xFFC4
         * - [6:7]: Status flags (see enum DescriptorStatus)
         * - [8:11]: Miscellaneous flags and error info
         * - [12:15]: Reserved
         */
        struct TxDescriptor {
            uint32_t address;   ///< Physical address of TX buffer
            uint16_t length;    ///< Buffer length (2's complement form)
            uint16_t status;    ///< Status flags (OWN, ERR, STP, ENP, etc.)
            uint32_t misc;      ///< Miscellaneous flags and error counters
            uint32_t reserved;  ///< Reserved for future use
        } __attribute__((packed));

        /**
         * @brief Receive Descriptor (16 bytes, 32-bit DMA mode)
         *
         * One descriptor per RX packet. Arrays of 8 form the RX ring.
         * RX ring MUST be 16-byte aligned for DMA!
         *
         * Layout (little-endian):
         * - [0:3]: Buffer address (32-bit physical)
         * - [4:5]: Buffer length in 2's complement (typically -1536)
         * - [6:7]: Status flags with ownership bit
         * - [8:11]: Miscellaneous (contains received message length on RX)
         * - [12:15]: Reserved
         */
        struct RxDescriptor {
            uint32_t address;   ///< Physical address of RX buffer
            uint16_t length;    ///< Buffer length (2's complement, e.g., -1536)
            uint16_t status;    ///< Status flags (OWN, ERR, STP, ENP, etc.)
            uint32_t misc;      ///< Message length received (bits 0-11) and error flags
            uint32_t reserved;  ///< Reserved for future use
        } __attribute__((packed));

        /**
         * @brief Descriptor Status Bits (applies to both TX and RX)
         *
         * Used in the 16-bit status field of descriptors.
         * Ownership bit (DESC_OWN) is fundamental to DMA synchronization:
         * - DESC_OWN = 0: CPU owns descriptor (can read/write)
         * - DESC_OWN = 1: NIC owns descriptor (CPU must not touch)
         */
        enum DescriptorStatus : uint16_t {
            DESC_OWN = (1 << 15),  ///< Ownership: 0=CPU, 1=NIC (CRITICAL!)
            DESC_ERR = (1 << 14),  ///< Error flag
            DESC_STP = (1 << 9),   ///< Start of Packet
            DESC_ENP = (1 << 8),   ///< End of Packet
        };

        /**
         * @brief Initialization Block (28 bytes, 32-bit DMA mode)
         *
         * Shared control structure passed to NIC during initialization.
         * Must be accessible by hardware (identity-mapped physical memory).
         * Must be at least 4-byte aligned; 16-byte alignment recommended.
         *
         * Layout (little-endian):
         * - [0:1]: Mode register
         * - [2]: RX ring length (log2, upper nibble of rlen field)
         * - [3]: TX ring length (log2, upper nibble of tlen field)
         * - [4:9]: MAC address (6 bytes)
         * - [10:11]: Reserved
         * - [12:19]: Logical Address Filter (multicast) - 8 bytes
         * - [20:23]: RX ring base address (32-bit physical)
         * - [24:27]: TX ring base address (32-bit physical)
         */
        struct InitBlock {
            uint16_t mode;        ///< Operating mode (promiscuous, etc.)
            uint8_t rlen;         ///< RX ring length: log2(count) << 4
            uint8_t tlen;         ///< TX ring length: log2(count) << 4
            uint8_t mac[6];       ///< MAC address from APROM
            uint16_t reserved;    ///< Reserved
            uint32_t ladrf[2];    ///< Multicast filter (64-bit bitmap)
            uint32_t rxRingAddr;  ///< RX ring physical address (must be 16-byte aligned)
            uint32_t txRingAddr;  ///< TX ring physical address (must be 16-byte aligned)
        } __attribute__((packed));

        /**
         * @brief Initialization Block Mode Bits
         *
         * Used in InitBlock.mode field to control hardware behavior.
         */
        enum InitBlockMode : uint16_t {
            MODE_PROM   = (1 << 15),  ///< Promiscuous mode (receive all frames)
            MODE_DRCVBC = (1 << 14),  ///< Disable Receive Broadcast
            MODE_DRCVPA = (1 << 13),  ///< Disable Receive Physical Address
            MODE_LOOP   = (1 << 2),   ///< Loopback mode
        };

        // ==================== Private Methods ====================

        /// @brief Read MAC address from APROM (I/O 0x00-0x05)
        void readMACAddress();

        /// @brief Perform hardware reset (read RESET register at offset 0x18)
        void reset();

        /// @brief Read a CSR register (via RAP/RDP protocol)
        uint32_t readCSR(uint16_t csr);

        /// @brief Write a CSR register (via RAP/RDP protocol)
        void writeCSR(uint16_t csr, uint32_t value);

        /// @brief Read a BCR register (via RAP/BDP protocol)
        uint32_t readBCR(uint16_t bcr);

        /// @brief Write a BCR register (via RAP/BDP protocol)
        void writeBCR(uint16_t bcr, uint32_t value);

        /// @brief Allocate and align all DMA buffers (init block, rings, packet buffers)
        bool allocateBuffers();

        /// @brief Free all DMA buffers
        void freeBuffers();

        /// @brief Initialize descriptor rings with correct values and ownership flags
        void initializeDescriptors();

        /// @brief Process received packets from RX ring (called from ISR)
        void processReceivedPackets();

        // ==================== Member Variables ====================

        // **PCI Location**
        uint8_t bus_;       ///< PCI bus number
        uint8_t device_;    ///< PCI device number
        uint8_t function_;  ///< PCI function number

        // **I/O Base Address**
        uint16_t ioBase_;  ///< I/O base address from BAR0

        // **Initialization Block** (4-byte aligned)
        InitBlock* initBlock_;  ///< Initialization block pointer (used by CPU)
        void* initBlockRaw_;    ///< Raw pointer for cleanup (tracks allocated memory)

        // **Descriptor Rings** (16-byte aligned REQUIRED for DMA!)
        TxDescriptor* txRing_;  ///< TX ring pointer (used by CPU)
        RxDescriptor* rxRing_;  ///< RX ring pointer (used by CPU)
        void* txRingRaw_;       ///< Raw TX ring pointer (tracks allocated memory)
        void* rxRingRaw_;       ///< Raw RX ring pointer (tracks allocated memory)

        // **Packet Buffers** (DMA accessible)
        uint8_t* txBuffers_[TX_RING_SIZE];  ///< TX packet buffer array
        uint8_t* rxBuffers_[RX_RING_SIZE];  ///< RX packet buffer array

        // **Ring Management**
        uint8_t currentTx_;  ///< Next TX descriptor to use (round-robin, 0-7)
        uint8_t currentRx_;  ///< Next RX descriptor to process (round-robin, 0-7)
    };

}  // namespace PalmyraOS::kernel
