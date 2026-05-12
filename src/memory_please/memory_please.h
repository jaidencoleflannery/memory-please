#ifndef MEMORY_PLEASE_H
#define MEMORY_PLEASE_H

typedef struct segment {
    struct segment *next;
    size_t capacity; // per byte.
    size_t used; // per byte.
} memory_segment;

bool mem_pls(size_t size, memory_segment *segment);

#endif
