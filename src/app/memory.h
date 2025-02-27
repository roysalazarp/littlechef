#ifndef MEMORY_H
#define MEMORY_H

#include "shared.h"

typedef struct {
    size_t size;
    void *start;
    void *current;
    boolean in_use;
} Memory;

Memory *memory_setup(void *raw_memory, size_t size);
void *memory_alloc(Memory *memory, size_t size);
void memory_reset(Memory *memory, u8 *ptr);

void *memory_in_use(Memory *memory);
void memory_out_of_use(Memory *memory, void *tmp);

#endif /* MEMORY_H */