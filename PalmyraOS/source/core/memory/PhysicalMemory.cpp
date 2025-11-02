

#include "core/memory/PhysicalMemory.h"
#include "libs/memory.h"

/**
 * End of the kernel data, provided by the linker (linker.ld)
 */
extern "C" uint32_t end;

/**
 * Initial placement address, pointing to the end of the kernel data.
 */
uint32_t placement_address = reinterpret_cast<uint32_t>(&end);

/**
 * Macros to get the index and offset from a bit.
 */
#define INDEX_FROM_BIT(a) (a / 32)
#define OFFSET_FROM_BIT(a) (a % 32)


/* primitive memory allocation with alignment and outputs physical address */
/* align = 1    =>  align the memory to a new page of 4096 bytes           */
void* PalmyraOS::kernel::kmalloc(uint32_t size) {
    // Ensure the size is valid
    if (size == 0) return nullptr;

    // Allocate memory and update the placement address
    uint32_t temp = placement_address;
    placement_address += size;
    return (void*) temp;
}

// --------------


// Globals


uint32_t* PalmyraOS::kernel::PhysicalMemory::frameBits_      = nullptr;  ///< Pointer to the frame usage bitmap
uint32_t PalmyraOS::kernel::PhysicalMemory::framesCount_     = 0;        ///< Total number of frames

uint32_t PalmyraOS::kernel::PhysicalMemory::freeFramesCount_ = 0;  // Initialize free frames count
uint32_t PalmyraOS::kernel::PhysicalMemory::allocatedFrames_ = 0;  // Initialize allocated frames count


void PalmyraOS::kernel::PhysicalMemory::initialize(uint32_t safeSpace, uint32_t memorySize) {
    // Initialize frames count
    framesCount_     = memorySize >> PAGE_BITS;  //  = size / PAGE_SIZE
    freeFramesCount_ = framesCount_;             // Initially all frames are free

    // Allocate and clear memory for the bitmap
    frameBits_       = (uint32_t*) kmalloc(INDEX_FROM_BIT(framesCount_));
    memset(frameBits_, 0, INDEX_FROM_BIT(framesCount_));

    // Unmark all frames initially
    unmarkAll();

    // Reserve frames for the kernel
    uint32_t reserved             = placement_address + safeSpace;
    uint32_t reservedUpperAligned = (reserved >> PAGE_BITS) + 1;

    // Mark all frames until the end of the reserved space
    for (uint32_t i = 0; i < reservedUpperAligned; ++i) {
        markFrame(i);
        allocatedFrames_++;  // Increment reserved frames count
        freeFramesCount_--;  // Decrement free frames count
    }

    // TODO also mark this class as 'used'
}

void* PalmyraOS::kernel::PhysicalMemory::allocateFrame() {
    // Find the first free frame
    uint32_t frame = findFirstFreeFrame();
    if (frame == 0) return nullptr;

    // Mark the frame as used
    markFrame((uint32_t) frame);
    allocatedFrames_++;  // Track allocated frames
    freeFramesCount_--;  // Reduce the count of free frames

    // Return the frame address (* PAGE_SIZE)
    return (void*) (frame << PAGE_BITS);
}

void PalmyraOS::kernel::PhysicalMemory::freeFrame(void* frame) {
    // Check for null frame
    if (frame == nullptr) return;

    // Unmark the frame
    unmarkFrame((uint32_t) frame >> PAGE_BITS);
    allocatedFrames_--;  // Reduce allocated frame count
    freeFramesCount_++;  // Increase free frame count
}

uint32_t PalmyraOS::kernel::PhysicalMemory::findFirstFreeFrame() {
    // Iterate through the bitmap to find a free frame
    for (uint32_t i = 0; i < INDEX_FROM_BIT(framesCount_); i++) {
        // If at least one bit is free
        if (frameBits_[i] != 0xFFFFFFFF) {
            // at least a bit is free (corresponds to 4KiB)
            for (uint32_t j = 0; j < 32; j++) {
                uint32_t toTest = 0x1 << j;

                // Check if the bit is free
                if (!(frameBits_[i] & toTest)) { return (i * 32) + j; }
            }
        }
    }

    // No free frame found
    return 0;
}

void* PalmyraOS::kernel::PhysicalMemory::allocateFrames(uint32_t num) {
    // Find the first set of free frames
    uint32_t firstFrame = findFirstFreeFrames(num);
    if (firstFrame == 0) return nullptr;
    for (uint32_t i = 0; i < num; ++i) {

        // No free frame found
        markFrame(firstFrame + i);
    }

    allocatedFrames_ += num;  // Track allocated frames
    freeFramesCount_ -= num;  // Reduce free frame count

    // Return the address of the first frame
    return (void*) (firstFrame << PAGE_BITS);
}

void PalmyraOS::kernel::PhysicalMemory::freeFrames(void* frame, uint32_t num) {
    // Calculate the starting frame index from the frame address
    uint32_t firstFrame = (uint32_t) frame >> PAGE_BITS;

    for (int i = 0; i < num; ++i) { unmarkFrame(firstFrame + i); }

    allocatedFrames_ -= num;  // Reduce allocated frame count
    freeFramesCount_ += num;  // Increase free frame count
}


uint32_t PalmyraOS::kernel::PhysicalMemory::findFirstFreeFrames(uint32_t num) {
    // Find the first consecutive set of free frames
    uint32_t consecutive = 0;
    uint32_t firstFrame  = 0;

    for (uint32_t i = 0; i < framesCount_; ++i) {
        // Check if the frame is free
        if (!getFrameMark(i)) {
            if (consecutive == 0) firstFrame = i;
            ++consecutive;

            // Have we found the required number of free frames?
            if (consecutive == num) return firstFrame;
        }
        else {
            // search again
            consecutive = 0;
        }
    }
    return 0;  // No sufficient free frames found
}

void PalmyraOS::kernel::PhysicalMemory::markFrame(uint32_t num) {
    // Mark the frame as used in the bitmap
    uint32_t word_index = INDEX_FROM_BIT(num);
    uint32_t bit_index  = OFFSET_FROM_BIT(num);
    frameBits_[word_index] |= (1 << bit_index);
}

void PalmyraOS::kernel::PhysicalMemory::unmarkFrame(uint32_t num) {
    // Unmark the frame in the bitmap
    uint32_t word_index = INDEX_FROM_BIT(num);
    uint32_t bit_index  = OFFSET_FROM_BIT(num);
    frameBits_[word_index] &= ~(1 << bit_index);
}

bool PalmyraOS::kernel::PhysicalMemory::getFrameMark(uint32_t num) {
    // Check if the frame is marked as used
    uint32_t word_index = INDEX_FROM_BIT(num);
    uint32_t bit_index  = OFFSET_FROM_BIT(num);
    //	return (frameBits[word_index] & (1 << bit_index)) >> bit_index;
    return (frameBits_[word_index] & (1 << bit_index)) != 0;
}

void PalmyraOS::kernel::PhysicalMemory::unmarkAll() {
    // Unmark all frames in the bitmap
    for (uint32_t i = 0; i < INDEX_FROM_BIT(framesCount_); i++) frameBits_[i] = 0;
}

uint32_t PalmyraOS::kernel::PhysicalMemory::size() {
    // Return the total number of frames
    return framesCount_;
}

void PalmyraOS::kernel::PhysicalMemory::reserveFrame(void* frame) {
    // Reserve a frame by marking it as used
    markFrame((uint32_t) frame >> PAGE_BITS);
    allocatedFrames_++;  // Increment reserved frames count
    freeFramesCount_--;  // Decrement free frames count
}

bool PalmyraOS::kernel::PhysicalMemory::isFrameFree(void* frame) {
    // Check if a frame is free
    return getFrameMark((uint32_t) frame >> PAGE_BITS);
}

uint32_t PalmyraOS::kernel::PhysicalMemory::getFreeFrames() {
    return freeFramesCount_;  // Return the number of free frames
}

uint32_t PalmyraOS::kernel::PhysicalMemory::getAllocatedFrames() {
    return allocatedFrames_;  // Return the number of allocated frames
}
