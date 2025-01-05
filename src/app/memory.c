#include <string.h>

/* clang-format off */
#include "shared.h"
#include "memory.h"
/* clang-format on */

Memory *memory_setup(void *raw_memory, size_t size) {
    memset(raw_memory, 0, size);

    Memory *memory = raw_memory;

    memory->size = size;
    memory->start = memory;
    memory->current = (char *)memory + sizeof(Memory);

    return memory;
}

void *memory_alloc(Memory *memory, size_t size) {
    ASSERT(!memory->in_use); /** Cannot allocate while the memory is in use! */

    if ((uint8_t *)memory->current + size > (uint8_t *)memory->start + memory->size) {
        ASSERT(0); /** Out of memory! */
    }

    void *ptr = memory->current;
    memory->current = (uint8_t *)memory->current + size;

    return ptr;
}

void *memory_in_use(Memory *memory) {
    ASSERT(!memory->in_use); /** Memory is already in use! */
    memory->in_use = true;

    return memory->current;
}

void memory_in_use2(Memory *memory, void **ptr, void **tmp) {
    ASSERT(!memory->in_use); /** Memory is already in use! */
    memory->in_use = true;
    if (ptr != NULL) {
        *ptr = memory->current;
    }
    *tmp = memory->current;
}

void memory_out_of_use(Memory *memory, void *tmp) {
    memory->in_use = false;
    memory->current = (uint8_t *)tmp;
}

void memory_reset(Memory *memory, uint8_t *ptr) {
    size_t set_bytes = (uint8_t *)memory->current - ptr;
    memset(ptr, 0, set_bytes);

    memory->current = ptr;
}
