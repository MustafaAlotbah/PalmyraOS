#pragma once

#include "core/definitions.h"

namespace PalmyraOS {

    // Forward declaration (avoid circular dependency)
    namespace kernel {
        class Mutex;
    }

    /**
     * @brief Tracks mutexes held by a process
     *
     * Pure data structure for tracking which mutexes a process currently holds.
     * Used for automatic cleanup when a process dies to prevent permanent deadlock.
     *
     * Design:
     * - Fixed-size array (no dynamic allocation)
     * - Simple linear search (acceptable for small N)
     * - No dependencies on Process or Mutex internals
     *
     * Usage:
     *   MutexTracker tracker;
     *   tracker.track(&myMutex);
     *   // ... use mutex ...
     *   tracker.untrack(&myMutex);
     *
     * On process death:
     *   tracker.forceReleaseAll(pid);
     */
    class MutexTracker {
    public:
        /// @brief Maximum number of mutexes a single process can hold simultaneously
        static constexpr uint8_t MAX_TRACKED_MUTEXES = 8;

        /**
         * @brief Construct an empty tracker
         */
        MutexTracker();

        /**
         * @brief Add a mutex to the tracking list
         *
         * @param mutex Pointer to the mutex to track
         * @return true if tracked successfully, false if list is full
         */
        bool track(kernel::Mutex* mutex);

        /**
         * @brief Remove a mutex from the tracking list
         *
         * @param mutex Pointer to the mutex to untrack
         * @return true if found and removed, false if not in list
         */
        bool untrack(kernel::Mutex* mutex);

        /**
         * @brief Force-release all tracked mutexes (called on process death)
         *
         * Iterates through all held mutexes and force-unlocks them to prevent
         * permanent deadlock when a process dies while holding locks.
         *
         * @param pid Process ID of the dying process (for verification)
         */
        void forceReleaseAll(uint32_t pid);

        /**
         * @brief Clear all tracked mutexes without releasing them
         *
         * Used after forceReleaseAll() to reset the tracker state.
         */
        void clear();

        /**
         * @brief Get number of currently tracked mutexes
         * @return Count of held mutexes
         */
        [[nodiscard]] uint8_t count() const { return count_; }

        /**
         * @brief Check if no mutexes are being tracked
         * @return true if tracker is empty
         */
        [[nodiscard]] bool isEmpty() const { return count_ == 0; }

        /**
         * @brief Get mutex at specific index (for iteration/debugging)
         *
         * @param index Index in tracking array (0 to count-1)
         * @return Pointer to mutex, or nullptr if index out of bounds
         */
        [[nodiscard]] kernel::Mutex* get(uint8_t index) const;

    private:
        kernel::Mutex* mutexes_[MAX_TRACKED_MUTEXES];  ///< Array of held mutex pointers
        uint8_t count_;                                 ///< Number of currently tracked mutexes
    };

}  // namespace PalmyraOS

