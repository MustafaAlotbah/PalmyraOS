
#pragma once

#include "core/Interrupts.h"
#include "core/memory/KernelHeapAllocator.h"


namespace PalmyraOS::kernel {

    class SystemCallsManager {
        using SystemCallHandler = void (*)(interrupts::CPURegisters* regs);

    public:
        static void initialize();

        static uint32_t* handleInterrupt(interrupts::CPURegisters* regs);

    private:
        static bool isValidAddress(void* addr);

        /* POSIX Interrupts */
        static void handleExit(interrupts::CPURegisters* regs);
        static void handleGetPid(interrupts::CPURegisters* regs);
        static void handleYield(interrupts::CPURegisters* regs);
        static void handleMmap(interrupts::CPURegisters* regs);
        static void handleGetTime(interrupts::CPURegisters* regs);
        static void handleClockNanoSleep64(interrupts::CPURegisters* regs);

        static void handleOpen(interrupts::CPURegisters* regs);
        static void handleClose(interrupts::CPURegisters* regs);
        static void handleWrite(interrupts::CPURegisters* regs);
        static void handleRead(interrupts::CPURegisters* regs);
        static void handleLongSeek(interrupts::CPURegisters* regs);
        static void handleIoctl(interrupts::CPURegisters* regs);
        static void handleMkdir(interrupts::CPURegisters* regs);
        static void handleRmdir(interrupts::CPURegisters* regs);
        static void handleUnlink(interrupts::CPURegisters* regs);

        /* From Linux */
        static void handleGetdents(interrupts::CPURegisters* regs);
        static void handleArchPrctl(interrupts::CPURegisters* regs);

        /* PalmyraOS Specific Interrupts */
        static void handleInitWindow(interrupts::CPURegisters* regs);
        static void handleCloseWindow(interrupts::CPURegisters* regs);
        static void handleNextKeyboardEvent(interrupts::CPURegisters* regs);
        static void handleNextMouseEvent(interrupts::CPURegisters* regs);
        static void handleGetWindowStatus(interrupts::CPURegisters* regs);

        static void handleWaitPID(interrupts::CPURegisters* regs);

        /* Socket syscalls */
        static void handleSocket(interrupts::CPURegisters* regs);
        static void handleBind(interrupts::CPURegisters* regs);
        static void handleConnect(interrupts::CPURegisters* regs);
        static void handleListen(interrupts::CPURegisters* regs);
        static void handleAccept(interrupts::CPURegisters* regs);
        static void handleSendto(interrupts::CPURegisters* regs);
        static void handleRecvfrom(interrupts::CPURegisters* regs);
        static void handleSetsockopt(interrupts::CPURegisters* regs);
        static void handleGetsockopt(interrupts::CPURegisters* regs);
        static void handleGetsockname(interrupts::CPURegisters* regs);
        static void handleGetpeername(interrupts::CPURegisters* regs);
        static void handleShutdown(interrupts::CPURegisters* regs);
        static void handleSpawn(interrupts::CPURegisters* regs);

        static void handleBrk(interrupts::CPURegisters* regs);
        static void handleSetThreadArea(interrupts::CPURegisters* regs);

        static void handleGetUID(interrupts::CPURegisters* regs);
        static void handleGetGID(interrupts::CPURegisters* regs);
        static void handleGetEUID(interrupts::CPURegisters* regs);
        static void handleGetEGID(interrupts::CPURegisters* regs);

        static void handleReboot(interrupts::CPURegisters* regs);

        static KMap<uint32_t, SystemCallHandler> systemCallHandlers_;
    };


}  // namespace PalmyraOS::kernel