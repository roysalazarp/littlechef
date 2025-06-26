#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <x86intrin.h>

#include "./profiler.h"

/**
 * Frequency of the OS timer in ticks per second. In Linux return 1000000,
 * indicating that the OS timer is assumed to have a resolution of microseconds
 * (1,000,000 microseconds in a second)
 */
u64 get_os_timer_freq() { return 1000000; }

u64 read_os_current_time() {
    struct timeval value;
    gettimeofday(&value, 0); /** Read the current time from the OS */

    u64 result = get_os_timer_freq() * (u64)value.tv_sec + (u64)value.tv_usec; /** OS current time in microseconds */
    return result;
}

u64 cpu_cycles_elapsed_since_processor_started() { return __rdtsc(); } /** Get CPU cycles that have elapsed since the processor started */

void print_time_elapsed(char const *label, u64 elapsed_cpu_cycles, u64 begin, u64 end) {
    u64 elapsed = end - begin;
    f64 percent = 100.0 * ((f64)elapsed / (f64)elapsed_cpu_cycles);
    printf("%s: %llu (%.2f%%)\n", label, elapsed, percent);
}

void print_time_elapsed2(u64 total_tsc_elapsed, ProfileAnchor *anchor) {
    u64 elapsed = anchor->tsc_elapsed;
    f64 percent = 100.0 * ((f64)elapsed / (f64)total_tsc_elapsed);
    printf("  %s[%llu]: %llu (%.2f%%)\n", anchor->label, anchor->hit_count, elapsed, percent);
}

/**
 * Estimate the CPU timer frequency (TSC frequency) in ticks per second.
 * It does this by measuring how many CPU cycles elapse during one second of OS time.
 */
u64 estimate_cpu_timer_freq() {
    u64 os_freq = get_os_timer_freq();

    u64 cpu_start = cpu_cycles_elapsed_since_processor_started();

    u64 os_start = read_os_current_time();
    u64 os_end = 0;
    u64 os_ticks_elapsed = 0;
    while (os_ticks_elapsed < os_freq) {
        os_end = read_os_current_time();
        os_ticks_elapsed = os_end - os_start;
    }

    u64 cpu_end = cpu_cycles_elapsed_since_processor_started();
    u64 cpu_freq = cpu_end - cpu_start;

    return cpu_freq;
}

#ifndef READ_BLOCK_TIMER
#define READ_BLOCK_TIMER cpu_cycles_elapsed_since_processor_started
#endif

Profiler global_profiler;

void begin_profile() { global_profiler.start_tsc = READ_BLOCK_TIMER(); }

void end_and_print_profile() {
    global_profiler.end_tsc = cpu_cycles_elapsed_since_processor_started();
    u64 cpu_freq = estimate_cpu_timer_freq();

    u64 elapsed_cpu_cycles = global_profiler.end_tsc - global_profiler.start_tsc;

    if (cpu_freq) {
        printf("\nTotal time: %0.4fms (Estimated CPU %.2fGHz or %lluHz)\n", 1000.0 * (f64)elapsed_cpu_cycles / (f64)cpu_freq, (f64)cpu_freq / 1000000000.0, cpu_freq);
    }

    u32 anchor_index;

    for (anchor_index = 0; anchor_index < array_count(global_profiler.anchors); ++anchor_index) {
        ProfileAnchor *anchor = global_profiler.anchors + anchor_index;

        if (anchor->tsc_elapsed) {
            print_time_elapsed2(elapsed_cpu_cycles, anchor);
        }
    }

    memset(&global_profiler, 0, sizeof(global_profiler));
}

ProfileBlock profile_block_scope_start(const char *label, u32 anchor_index) {
    ProfileBlock block = {0};

    block.anchor_index = anchor_index;
    block.label = label;
    block.start_tsc = READ_BLOCK_TIMER();

    return block;
}

void profile_block_scope_end(ProfileBlock *block) {
    u64 elapsed = READ_BLOCK_TIMER() - block->start_tsc;

    ProfileAnchor *anchor = global_profiler.anchors + block->anchor_index;

    anchor->tsc_elapsed += elapsed;
    ++anchor->hit_count;

    anchor->label = block->label;
}
