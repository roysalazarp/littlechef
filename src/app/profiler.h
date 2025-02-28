#ifndef PROFILER_H
#define PROFILER_H

#include "./shared.h"

/** IMPORTANT: The profiler does NOT support nested blocks yet */

typedef struct {
    u64 tsc_elapsed;
    u64 hit_count;
    const char *label;
} ProfileAnchor;

typedef struct {
    const char *label;
    u64 start_tsc;
    u32 anchor_index;
} ProfileBlock;

typedef struct {
    ProfileAnchor anchors[4096];
    u64 start_tsc;
    u64 end_tsc;
} Profiler;

#define array_count(array) (sizeof(array) / sizeof((array)[0]))

void begin_profile();
void end_and_print_profile();

ProfileBlock profile_block_scope_start(const char *label, u32 anchor_index);
void profile_block_scope_end(ProfileBlock *block);

/* clang-format off */
#define NameConcat2(A, B) A##B
#define NameConcat(A, B) NameConcat2(A, B)
#define profiler_end_of_compilation_unit static_assert(__COUNTER__ < array_count(global_profiler_anchors), "Number of profile points exceeds size of profiler::Anchors array")

#define TIME_BLOCK_LINE_HELPER(name, line) \
    ProfileBlock NameConcat(block, line) = profile_block_scope_start(name, __COUNTER__ + 1); \
    NameConcat(int first_time_, line) = 1; \
    for(; NameConcat(first_time_, line); profile_block_scope_end(&(NameConcat(block, line))), NameConcat(first_time_, line) = 0)

#define TIME_BLOCK(name) TIME_BLOCK_LINE_HELPER(name, __LINE__)
/* clang-format on */

extern Profiler global_profiler;

#endif /* PROFILER_H */