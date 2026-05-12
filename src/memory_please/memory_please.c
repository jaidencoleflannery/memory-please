#include <sys/mman.h>
#include <stdbool.h>
#include <stddef.h>

#include "./memory_please.h"

static const void *pool;
static size_t pool_used = 0; // per byte.
static size_t pool_capacity = 0; // per byte.

static memory_segment *used_segments_head;
static memory_segment *used_segments_tail;

static memory_segment *free_segments_head;
static memory_segment *free_segments_tail;

static bool get_pool() {
    pool = mmap(NULL, 64, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
    return true;
}

static bool fit_segment(size_t size, void *memory) {
    // check if the requested segment will fit in the existing nodes.
    memory_segment *cursor = free_segments_head;
    while(cursor != NULL) {
        if((cursor->capacity - cursor->used) >= size) {
            memory = (cursor + cursor->used);
            return true;
        }
        cursor = cursor->next;
    }

    // get a new node.
    if((pool_used + size) > pool_capacity) {
        memory_segment* new_segment = (used_segments_tail + (used_segments_tail->capacity / 8) + 1)
        used_segments_tail->next = 
    }
    return false;
}

bool mem_pls(size_t size, void *segment) {
    if(pool == NULL)
        get_pool();
        
    fit_segment(size, segment);

    
    return false;
}

