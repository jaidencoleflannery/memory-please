#include <sys/mman.h>
#include <stdbool.h>
#include <stddef.h>

#include "./memory_please.h"

static const void *pool;

static size_t pool_used = 0; // per byte.
static size_t pool_capacity = 64; // per byte.

static memory_segment *used_segments_head;
static memory_segment *used_segments_tail;

static memory_segment *free_segments_head;
static memory_segment *free_segments_tail;

static bool scale_pool(size_t scale) { 
    pool = mmap(NULL, (pool_capacity * scale), PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
    if(pool == MAP_FAILED)
        return false;

    pool_capacity *= scale;
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
    if((pool_used + size) > pool_capacity)
        scale_pool(pool_capacity * 2);
        
    memory_segment* new_segment = (memory_segment *)((char *)used_segments_tail + sizeof(memory_segment) + (used_segments_tail->capacity));
    used_segments_tail->next = new_segment;
    used_segments_tail = new_segment;

    return true;
}


bool mem_free(memory_segment *segment) {
    bool found = false;
    memory_segment *cursor = used_segments_head;
    memory_segment *prev = NULL;
    // sacrifice speed for memory and just store the prev pointer so this is O(1)?
    while(cursor != NULL) {
        if(cursor == segment) {
            found = true;
            if(prev != NULL)
                prev->next = cursor->next;
            free_segments_tail->next = segment;
            free_segments_tail = cursor;
            segment->used = 0;
        } else {
            prev = cursor;
            cursor = cursor->next;
        }
    } 
    return found;
}

bool mem_pls(size_t size, memory_segment *segment) {
    if(pool == NULL)
        scale_pool(pool_capacity);
        
    if(!fit_segment(size, segment))
        return false;

    return true;
}

