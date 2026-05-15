#ifndef MEMORY_PLEASE_H
#define MEMORY_PLEASE_H

#ifdef DEBUG
    #define LOG(input) do { fprintf(stderr, "[ DEBUG ] " input "\n"); } while(0)
#else
    #define LOG(input) do { ((void)0); } while(0)
#endif

typedef struct segment {
    struct segment *next;
    struct segment *previous;
    void *memory;
    size_t capacity; // per byte.
    size_t used; // per byte.
} memory_segment;

bool mem_pls(memory_segment *segment);

#endif

