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

    if ((u8 *)memory->current + size > (u8 *)memory->start + memory->size) {
        ASSERT(0); /** Out of memory! */
    }

    void *ptr = memory->current;
    memory->current = (u8 *)memory->current + size;

    return ptr;
}

void *memory_in_use(Memory *memory) {
    ASSERT(!memory->in_use); /** Memory is already in use! */
    memory->in_use = true;

    return memory->current;
}

void memory_out_of_use(Memory *memory, void *tmp) {
    memory->in_use = false;
    memory->current = (u8 *)tmp;
}

void memory_reset(Memory *memory, u8 *ptr) {
    size_t set_bytes = (u8 *)memory->current - ptr;
    memset(ptr, 0, set_bytes);

    memory->current = ptr;
}
