
#include "palmyraOS/stdlib.h"
#include "palmyraOS/unistd.h"


void* malloc(size_t size) {
    auto address = (uint32_t*) mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    return address;
}

void free(void* ptr) {
    // TODO
}
