#pragma once

#include "core/definitions.h"

namespace PalmyraOS {

/**
 * @brief Lock-free circular queue (fixed-size FIFO buffer)
 * 
 * A lightweight, fixed-capacity queue with O(1) enqueue/dequeue.
 * Thread-safe when used with external synchronization (e.g., spinlock).
 * 
 * Features:
 * - Fixed capacity (known at compile time)
 * - No dynamic allocation
 * - Cache-friendly (contiguous array)
 * - Wrap-around indexing
 * 
 * Usage:
 *   CircularQueue<uint32_t, 32> queue;
 *   queue.enqueue(42);
 *   uint32_t value = queue.dequeue();
 * 
 * @tparam T Element type
 * @tparam Capacity Maximum number of elements (must be > 0)
 */
template<typename T, uint8_t Capacity>
class CircularQueue {
public:
    static_assert(Capacity > 0, "CircularQueue capacity must be greater than 0");
    
    /**
     * @brief Construct an empty queue
     */
    CircularQueue() : head_(0), tail_(0), count_(0) {
        // Initialize array to zeros (for POD types)
        for (uint8_t i = 0; i < Capacity; ++i) {
            items_[i] = T{};
        }
    }
    
    /**
     * @brief Add element to the back of the queue
     * 
     * @param item Element to add
     * @return true if added successfully, false if queue is full
     */
    bool enqueue(T item) {
        if (count_ >= Capacity) {
            return false;  // Queue full
        }
        
        items_[tail_] = item;
        tail_ = (tail_ + 1) % Capacity;  // Wrap around
        count_++;
        
        return true;
    }
    
    /**
     * @brief Remove and return element from the front of the queue
     * 
     * @param outItem Pointer to store the dequeued item
     * @return true if dequeued successfully, false if queue is empty
     */
    bool dequeue(T* outItem) {
        if (count_ == 0) {
            return false;  // Queue empty
        }
        
        if (outItem) {
            *outItem = items_[head_];
        }
        
        head_ = (head_ + 1) % Capacity;  // Wrap around
        count_--;
        
        return true;
    }
    
    /**
     * @brief Check if queue is empty
     * @return true if no elements in queue
     */
    [[nodiscard]] bool isEmpty() const {
        return count_ == 0;
    }
    
    /**
     * @brief Check if queue is full
     * @return true if queue is at capacity
     */
    [[nodiscard]] bool isFull() const {
        return count_ >= Capacity;
    }
    
    /**
     * @brief Get current number of elements
     * @return Number of elements in queue
     */
    [[nodiscard]] uint8_t size() const {
        return count_;
    }
    
    /**
     * @brief Get maximum capacity
     * @return Maximum number of elements
     */
    [[nodiscard]] constexpr uint8_t capacity() const {
        return Capacity;
    }
    
    /**
     * @brief Peek at front element without removing it
     * 
     * @param outItem Pointer to store the peeked item
     * @return true if peeked successfully, false if queue is empty
     */
    bool peek(T* outItem) const {
        if (count_ == 0) {
            return false;
        }
        
        if (outItem) {
            *outItem = items_[head_];
        }
        
        return true;
    }
    
    /**
     * @brief Clear all elements from the queue
     */
    void clear() {
        head_ = 0;
        tail_ = 0;
        count_ = 0;
    }

private:
    T items_[Capacity];      ///< Circular buffer storage
    uint8_t head_;            ///< Index of first element (dequeue position)
    uint8_t tail_;            ///< Index of next free slot (enqueue position)
    uint8_t count_;           ///< Current number of elements
};

}  // namespace PalmyraOS

