#include "core/sync/Mutex.h"
#include "core/Interrupts.h"
#include "core/peripherals/Logger.h"
#include "core/tasks/ProcessManager.h"

namespace PalmyraOS::kernel {

    // ==================== Mutex Implementation ====================

    Mutex::Mutex() : isLocked_(0), ownerPid_(0), queueSpinlock_(0) {
        // CircularQueue initializes itself
    }

    Mutex::~Mutex() {
        // Safety check: warn if processes are still waiting
        if (!waitQueue_.isEmpty()) { LOG_WARN("Mutex: Destroyed while %u processes are waiting!", waitQueue_.size()); }
        if (isLocked_ != 0) { LOG_WARN("Mutex: Destroyed while still locked by PID %u", ownerPid_); }
    }

    // ==================== Pure Mutex API (no Process dependencies!) ====================

    bool Mutex::tryAcquire(uint32_t pid) {
        // Atomic compare-and-swap: if isLocked_==0, set to 1
        if (__sync_bool_compare_and_swap(&isLocked_, 0, 1)) {
            ownerPid_ = pid;
            return true;
        }
        return false;
    }

    bool Mutex::release(uint32_t pid) {
        // Verify ownership
        if (ownerPid_ != pid) {
            LOG_ERROR("Mutex: PID %u tried to release mutex owned by PID %u", pid, ownerPid_);
            return false;
        }

        // Release the lock
        ownerPid_ = 0;
        __sync_lock_release(&isLocked_);

        return true;
    }

    bool Mutex::enqueueWaiter(uint32_t pid) {
        lockQueue();
        bool success = waitQueue_.enqueue(pid);
        if (success) { LOG_DEBUG("Mutex: Enqueued PID %u (queue size: %u)", pid, waitQueue_.size()); }
        else { LOG_ERROR("Mutex: Wait queue full (max 32 waiters)"); }
        unlockQueue();
        return success;
    }

    bool Mutex::dequeueWaiter(uint32_t* outPid) {
        if (!outPid) return false;

        lockQueue();
        bool success = waitQueue_.dequeue(outPid);
        if (success) { LOG_DEBUG("Mutex: Dequeued PID %u (queue size: %u)", *outPid, waitQueue_.size()); }
        unlockQueue();

        return success;
    }

    // ==================== Deprecated Methods (use Process::acquireMutex instead) ====================

    void Mutex::lock() {
        Process* current = TaskManager::getCurrentProcess();

        // Boot time fallback (no scheduler available)
        if (!current) {
            while (!__sync_bool_compare_and_swap(&isLocked_, 0, 1)) { asm volatile("pause"); }
            ownerPid_ = 0;
            return;
        }

        uint32_t myPID = current->getPid();

        // Main acquisition loop (NO recursion!)
        while (true) {
            // Step 1: Try to acquire lock atomically (fast path)
            if (__sync_bool_compare_and_swap(&isLocked_, 0, 1)) {
                // SUCCESS - we got the lock!
                ownerPid_ = myPID;
                return;  // Exit - we own the lock!
            }

            // Step 2: Lock is held - check for deadlock
            if (ownerPid_ == myPID) {
                LOG_ERROR("Mutex: Deadlock detected! PID %u tried to re-lock mutex it already owns", myPID);
                return;
            }

            // Step 3: Add ourselves to wait queue (only if we're not already in it)
            if (current->getState() != Process::State::Waiting) {
                lockQueue();
                if (!waitQueue_.enqueue(myPID)) { LOG_ERROR("Mutex: Wait queue full (max %u waiters)!", MAX_WAITERS); }
                else { LOG_DEBUG("Mutex: Enqueued PID %u (queue size: %u)", myPID, waitQueue_.size()); }
                current->setState(Process::State::Waiting);
                unlockQueue();
            }

            // Step 4: Yield until woken up by unlock()
            while (current->getState() == Process::State::Waiting) { TaskManager::yield(); }

            // Step 5: We've been woken - loop back to Step 1 to try CAS again!
            // (The lock was released, we compete with other woken waiters)
        }
    }

    bool Mutex::tryLock() {
        // Non-blocking lock attempt (NO interrupt disable needed!)
        bool acquired = __sync_bool_compare_and_swap(&isLocked_, 0, 1);

        if (acquired) {
            // Success - set owner
            Process* current = TaskManager::getCurrentProcess();
            ownerPid_        = current ? current->getPid() : 0;
        }

        return acquired;
    }

    void Mutex::unlock() {
        // Verify we own the lock
        Process* current = TaskManager::getCurrentProcess();
        uint32_t myPID   = current ? current->getPid() : 0;

        if (ownerPid_ != myPID) {
            LOG_ERROR("Mutex: PID %u tried to unlock mutex owned by PID %u", myPID, ownerPid_);
            return;
        }

        // Release the lock completely first
        ownerPid_ = 0;
        __sync_lock_release(&isLocked_);  // Atomic: locked_ = 0

        // Now wake up ONE valid waiter (skip dead/killed processes)
        lockQueue();

        bool wokeUpSomeone = false;
        while (hasWaiters() && !wokeUpSomeone) {
            // Try to wake up the first waiter
            uint32_t nextPID = 0;

            // Queue empty?
            if (!waitQueue_.dequeue(&nextPID)) break;

            Process* nextProc = TaskManager::getProcess(nextPID);

            if (nextProc && nextProc->getState() == Process::State::Waiting) {
                // Found a valid waiter - wake them up!
                nextProc->setState(Process::State::Ready);
                wokeUpSomeone = true;
                LOG_DEBUG("Mutex: Woke up PID %u (released by PID %u), queue size: %u", nextPID, myPID, waitQueue_.size());
            }
            else {
                // Process was killed or not waiting - skip to next waiter
                if (nextProc) { LOG_WARN("Mutex: Waiter PID %u in state %s (not Waiting), trying next", nextPID, nextProc->stateToString()); }
                else { LOG_WARN("Mutex: Waiter PID %u no longer exists, trying next", nextPID); }
                // Loop continues to try next waiter!
            }
        }

        unlockQueue();
    }

    void Mutex::forceUnlock(uint32_t pid) {
        /**
         * @brief Force-unlock mutex if owned by a dying process
         *
         * Called by ProcessManager::kill() to prevent permanent deadlock.
         * Only unlocks if the specified PID actually owns the lock.
         */

        if (ownerPid_ != pid) {
            return;  // Not owned by this process, nothing to do
        }

        LOG_WARN("Mutex: Force-unlocking mutex held by dying process PID %u", pid);

        // Release the lock
        ownerPid_ = 0;
        __sync_lock_release(&isLocked_);

        // Wake up one waiter
        lockQueue();

        bool wokeUpSomeone = false;
        while (hasWaiters() && !wokeUpSomeone) {
            uint32_t nextPID = 0;
            if (!waitQueue_.dequeue(&nextPID)) break;

            Process* nextProc = TaskManager::getProcess(nextPID);
            if (nextProc && nextProc->getState() == Process::State::Waiting) {
                nextProc->setState(Process::State::Ready);
                wokeUpSomeone = true;
                LOG_DEBUG("Mutex: Woke up PID %u after force-unlock", nextPID);
            }
        }

        unlockQueue();
    }

    // ==================== Queue Spinlock Helpers ====================

    void Mutex::lockQueue() {
        // Acquire spinlock for wait queue (busy-wait, but very short!)
        while (!__sync_bool_compare_and_swap(&queueSpinlock_, 0, 1)) {
            asm volatile("pause");  // x86 hint: reduce contention
        }
    }

    void Mutex::unlockQueue() {
        // Release spinlock for wait queue
        __sync_lock_release(&queueSpinlock_);
    }

    // ==================== MutexGuard (RAII) Implementation ====================

    MutexGuard::MutexGuard(Mutex& mutex) : mutex_(mutex) { mutex_.lock(); }

    MutexGuard::~MutexGuard() { mutex_.unlock(); }

}  // namespace PalmyraOS::kernel
