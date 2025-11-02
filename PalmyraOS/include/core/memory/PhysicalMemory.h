
#pragma once


#include "core/definitions.h"


namespace PalmyraOS::kernel {

    /**
     * Constants for page management
     */
    constexpr uint32_t PAGE_BITS   = 12;
    constexpr uint32_t PAGE_SIZE   = 1 << PAGE_BITS;
    constexpr uint32_t NUM_ENTRIES = 1024;

    /**
     * @brief Allocates memory in the kernel.
     * @param size The size of the memory to allocate in bytes.
     * @return Pointer to the allocated memory.
     */
    void* kmalloc(uint32_t size);  // kernel malloc

    /**
     * @brief Manages physical memory, dividing it into frames (pages) and tracking used and free frames.
     */
    class PhysicalMemory {
    public:
        /**
         * @brief Initializes the physical memory manager.
         * @param safeSpace The safe space for the kernel.
         * @param memorySize The total size of the physical memory.
         */
        static void initialize(uint32_t safeSpace, uint32_t memorySize);

        /**
         * @brief Allocates a single frame.
         * @return Pointer to the allocated frame, or nullptr if no frame is available.
         */
        static void* allocateFrame();

        /**
         * @brief Allocates multiple contiguous frames.
         * @param num The number of frames to allocate.
         * @return Pointer to the first allocated frame, or nullptr if not enough frames are available.
         */
        static void* allocateFrames(uint32_t num);

        /**
         * @brief Frees a previously allocated frame.
         * @param frame Pointer to the frame to free.
         */
        static void freeFrame(void* frame);

        static void freeFrames(void* frame, uint32_t num);

        /**
         * @brief Reserves a frame, marking it as used.
         * @param frame Pointer to the frame to reserve.
         */
        static void reserveFrame(void* frame);

        /**
         * @brief Checks if a frame is free.
         * @param frame Pointer to the frame to check.
         * @return True if the frame is free, false otherwise.
         */
        static bool isFrameFree(void* frame);

        /**
         * @brief Gets the total number of frames.
         * @return The total number of frames.
         */
        static uint32_t size();

        /**
         * @brief Gets the number of free frames.
         * @return The number of free frames.
         */
        static uint32_t getFreeFrames();

        /**
         * @brief Gets the number of allocated frames.
         * @return The number of allocated frames.
         */
        static uint32_t getAllocatedFrames();

    private:
        /**
         * @brief Finds the first free frame.
         * @return Index of the first free frame, or 0 if no free frame is found.
         */
        static uint32_t findFirstFreeFrame();

        /**
         * @brief Finds the first set of free frames.
         * @param num The number of consecutive free frames needed.
         * @return Index of the first free frame set, or 0 if no free set is found.
         */
        static uint32_t findFirstFreeFrames(uint32_t num);

        /**
         * @brief Marks a frame as used.
         * @param num The frame index to mark.
         */
        static void markFrame(uint32_t num);

        /**
         * @brief Marks a frame as free.
         * @param num The frame index to unmark.
         */
        static void unmarkFrame(uint32_t num);

        /**
         * @brief Checks if a frame is marked as used.
         * @param num The frame index to check.
         * @return True if the frame is marked, false otherwise.
         */
        static bool getFrameMark(uint32_t num);

        /**
         * @brief Unmarks all frames, marking them as free.
         */
        static void unmarkAll();

    private:
        static uint32_t* frameBits_;   ///< Bitmap array to track frame usage
        static uint32_t framesCount_;  ///< Total number of frames

        static uint32_t freeFramesCount_;  ///< Track how many frames are free
        static uint32_t allocatedFrames_;  ///< Track how many frames are currently allocated
    };


}  // namespace PalmyraOS::kernel
