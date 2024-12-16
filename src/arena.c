#include "headers.h"

/**
 * Initializes a new memory arena, with the arena header stored at the beginning of the allocated memory block.
 */
Arena *arena_init(size_t size) {
    Arena *arena = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(arena != NULL);

    memset(arena, 0, size);

    arena->size = size;
    arena->start = arena;
    arena->current = (uint8_t *)arena + sizeof(Arena);

    return arena;
}

/**
 * Allocates memory from the arena and advances the current pointer by the requested size.
 */
void *arena_alloc(Arena *arena, size_t size) {
    if ((uint8_t *)arena->current + size > (uint8_t *)arena->start + arena->size) {
        assert(0);
    }

    void *ptr = arena->current;
    arena->current = (uint8_t *)arena->current + size;

    return ptr;
}

/**
 *  Resets the arena allocator by clearing its used memory and resetting the current pointer.
 */
void arena_reset(Arena *arena, size_t arena_header_size) {
    uint8_t *start = (uint8_t *)arena->start + arena_header_size;

    size_t set_bytes = (uint8_t *)arena->current - start;
    memset(start, 0, set_bytes);

    arena->current = start;
}

void arena_free(Arena *arena) {
    if (munmap(arena->start, arena->size) == -1) {
        assert(0);
    }
}
