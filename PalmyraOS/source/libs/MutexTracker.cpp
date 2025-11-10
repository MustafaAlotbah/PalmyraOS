#include "libs/MutexTracker.h"
#include "core/sync/Mutex.h"
#include "core/peripherals/Logger.h"

namespace PalmyraOS {

MutexTracker::MutexTracker() : count_(0) {
    // Initialize array to nullptr
    for (uint8_t i = 0; i < MAX_TRACKED_MUTEXES; ++i) {
        mutexes_[i] = nullptr;
    }
}

bool MutexTracker::track(kernel::Mutex* mutex) {
    if (!mutex) {
        return false;  // Null pointer
    }
    
    if (count_ >= MAX_TRACKED_MUTEXES) {
        LOG_ERROR("MutexTracker: Cannot track mutex - limit of %u reached!", MAX_TRACKED_MUTEXES);
        return false;
    }
    
    // Check if already tracked (prevent duplicates)
    for (uint8_t i = 0; i < count_; ++i) {
        if (mutexes_[i] == mutex) {
            LOG_WARN("MutexTracker: Mutex already tracked");
            return false;
        }
    }
    
    // Add to array
    mutexes_[count_++] = mutex;
    LOG_DEBUG("MutexTracker: Tracked mutex (total: %u/%u)", count_, MAX_TRACKED_MUTEXES);
    
    return true;
}

bool MutexTracker::untrack(kernel::Mutex* mutex) {
    if (!mutex) {
        return false;
    }
    
    // Find mutex in array
    for (uint8_t i = 0; i < count_; ++i) {
        if (mutexes_[i] == mutex) {
            // Found it - remove by shifting remaining elements down
            for (uint8_t j = i; j < count_ - 1; ++j) {
                mutexes_[j] = mutexes_[j + 1];
            }
            
            count_--;
            mutexes_[count_] = nullptr;  // Clear last slot
            
            LOG_DEBUG("MutexTracker: Untracked mutex (remaining: %u)", count_);
            return true;
        }
    }
    
    LOG_WARN("MutexTracker: Attempted to untrack mutex that wasn't tracked");
    return false;
}

void MutexTracker::forceReleaseAll(uint32_t pid) {
    LOG_INFO("MutexTracker: Force-releasing %u held mutexes for PID %u", count_, pid);
    
    // Release all held mutexes
    for (uint8_t i = 0; i < count_; ++i) {
        if (mutexes_[i]) {
            mutexes_[i]->forceUnlock(pid);
            mutexes_[i] = nullptr;
        }
    }
    
    count_ = 0;
}

void MutexTracker::clear() {
    for (uint8_t i = 0; i < count_; ++i) {
        mutexes_[i] = nullptr;
    }
    count_ = 0;
}

kernel::Mutex* MutexTracker::get(uint8_t index) const {
    if (index >= count_) {
        return nullptr;
    }
    return mutexes_[index];
}

}  // namespace PalmyraOS

