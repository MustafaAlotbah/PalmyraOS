
#include "libs/utils.h"
#include "libs/memory.h"


// Read an unsigned 8-bit integer from data at the given offset
uint8_t get_uint8_t(const uint8_t* data, size_t offset) {
    uint8_t value;
    memcpy((void*) &value, (void*) (data + offset), sizeof(value));
    return value;
}

// Read an unsigned 16-bit integer from data at the given offset
uint16_t get_uint16_t(const uint8_t* data, size_t offset) {
    uint16_t value;
    memcpy((void*) &value, (void*) (data + offset), sizeof(value));
    return value;
}

// Read an unsigned 32-bit integer from data at the given offset
uint32_t get_uint32_t(const uint8_t* data, size_t offset) {
    uint32_t value;
    memcpy((void*) &value, (void*) (data + offset), sizeof(value));
    return value;
}
