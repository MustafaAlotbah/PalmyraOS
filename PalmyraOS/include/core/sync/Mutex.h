#pragma once

#include "core/definitions.h"
#include "libs/CircularQueue.h"

namespace PalmyraOS::kernel {

    // Forward declarations
    class Process;

    /**
     * @brief Production-grade mutual exclusion lock (Mutex)
     *
     * Provides sleep-based locking for safe concurrent access to shared resources.
     * When a process cannot acquire the lock, it yields the CPU to other processes
     * instead of busy-waiting (unlike spinlocks).
     *
     * Features:
     * - FIFO fairness: First process to wait gets the lock first
     * - Deadlock detection: Detects and prevents self-locking
     * - Owner tracking: Tracks which process holds the lock
     * - Interrupt-safe: Can be used from syscalls
     *
     * Usage:
     *   Mutex diskMutex;
     *
     *   // Manual locking:
     *   diskMutex.lock();
     *   // Critical section (e.g., disk I/O)
     *   diskMutex.unlock();
     *
     *   // RAII (recommended):
     *   {
     *       MutexGuard guard(diskMutex);
     *       // Critical section - auto-unlocks on scope exit
     *   }
     */
    class Mutex {
    public:
        /**
         * @brief Construct an unlocked mutex
         */
        Mutex();

        /**
         * @brief Destructor - warns if waiters still present
         */
        ~Mutex();

        // ==================== Pure Mutex API (no Process dependencies) ====================

        /**
         * @brief Try to acquire lock atomically
         *
         * @param pid Process ID attempting to acquire
         * @return true if acquired, false if already locked
         */
        bool tryAcquire(uint32_t pid);

        /**
         * @brief Release lock
         *
         * @param pid Process ID releasing the lock
         * @return true if released successfully, false if not owned by pid
         */
        bool release(uint32_t pid);

        /**
         * @brief Add PID to wait queue
         *
         * @param pid Process ID to enqueue
         * @return true if enqueued, false if queue full
         */
        bool enqueueWaiter(uint32_t pid);

        /**
         * @brief Get next waiter from queue
         *
         * @param outPid Pointer to store dequeued PID
         * @return true if dequeued, false if queue empty
         */
        bool dequeueWaiter(uint32_t* outPid);

        // ==================== Deprecated (for backwards compatibility) ====================

        /**
         * @deprecated Use Process::acquireMutex() instead
         */
        void lock();

        /**
         * @deprecated Use Process::tryAcquireMutex() instead
         */
        bool tryLock();

        /**
         * @deprecated Use Process::releaseMutex() instead
         */
        void unlock();

        /**
         * @brief Check if mutex is currently locked
         * @return true if locked by any process
         */
        [[nodiscard]] bool isLocked() const { return isLocked_ != 0; }

        /**
         * @brief Get PID of process holding the lock
         * @return PID of lock holder, or 0 if unlocked
         */
        [[nodiscard]] uint32_t getOwner() const { return ownerPid_; }

        /**
         * @brief Force-unlock mutex (used when owner process dies)
         *
         * Called by ProcessManager when a process is killed to prevent
         * permanent deadlock from held mutexes.
         *
         * @param pid PID of the process being killed
         */
        void forceUnlock(uint32_t pid);

        REMOVE_COPY(Mutex);

    private:
        // ==================== Lock State ====================

        /// @brief Lock status (0 = unlocked, 1 = locked)
        volatile uint32_t isLocked_;

        /// @brief PID of process holding the lock (0 if unlocked)
        volatile uint32_t ownerPid_;

        // ==================== Wait Queue (FIFO) ====================

        /// @brief Maximum number of processes that can wait for this mutex
        static constexpr uint8_t MAX_WAITERS = 32;

        /// @brief Circular queue of waiting process PIDs (reusable data structure!)
        CircularQueue<uint32_t, MAX_WAITERS> waitQueue_;

        /// @brief Spinlock protecting wait queue operations (0 = unlocked, 1 = locked)
        /// This allows us to avoid disabling global interrupts!
        volatile uint32_t queueSpinlock_;

        // ==================== Helper Methods ====================

        /**
         * @brief Acquire spinlock for wait queue operations
         * Uses busy-wait but only for microseconds (queue ops are fast)
         */
        void lockQueue();

        /**
         * @brief Release spinlock for wait queue
         */
        void unlockQueue();

        /**
         * @brief Check if any processes are waiting
         * @return true if wait queue has entries
         */
        [[nodiscard]] bool hasWaiters() const { return !waitQueue_.isEmpty(); }
    };

    /**
     * @brief RAII wrapper for Mutex (automatic unlock on scope exit)
     *
     * Ensures the mutex is always unlocked when the guard goes out of scope,
     * even if exceptions occur or early returns happen.
     *
     * Usage:
     *   void processFile() {
     *       MutexGuard guard(fileMutex);
     *       // Critical section
     *       if (error) return;  // Mutex auto-unlocks here
     *       // More work
     *   }  // Mutex auto-unlocks here too
     */
    class MutexGuard {
    public:
        /**
         * @brief Construct guard and acquire mutex
         * @param mutex The mutex to guard
         */
        explicit MutexGuard(Mutex& mutex);

        /**
         * @brief Destructor - automatically unlocks mutex
         */
        ~MutexGuard();

        REMOVE_COPY(MutexGuard);

    private:
        Mutex& mutex_;
    };

}  // namespace PalmyraOS::kernel
