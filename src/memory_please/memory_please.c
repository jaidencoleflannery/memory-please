#include <sys/mman.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "./memory_please.h"

static void * pool;
static bool initialized = false;

static size_t pool_used = 0; // per byte.
static size_t pool_capacity = 16000; // per byte (16kb default).

// linked list is stack based.
static memory_segment *used_segments_head;
static memory_segment *used_segments_tail;

static memory_segment *free_segments_head;
static memory_segment *free_segments_tail;

static bool scale_pool(size_t scale) {
    void *new_pool = mmap(NULL, (pool_capacity * scale), PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if(new_pool == MAP_FAILED) {
        LOG("scale_pool: Failed to map memory.");
        return false;
    }

    if(initialized) {
        LOG("scale_pool: Pool was already initialized, copying data to new pool.");
        memcpy(new_pool, pool, pool_capacity); 

        memory_segment **free_cursor = &(free_segments_head);
        while(free_cursor != NULL) {
            size_t free_cursor_offset = ((char *)(*free_cursor)->memory - (char *)pool);
            (*free_cursor)->memory = ((char *)new_pool + free_cursor_offset);
            free_cursor = &(*free_cursor)->next;
            if((memory_segment **)((*free_cursor)->next) == NULL) {
                *free_segments_tail = **free_cursor;
                break;
            }
        }

        memory_segment **used_cursor = &(used_segments_head);
        while(used_cursor != NULL) {
            size_t used_cursor_offset = ((char *)*used_cursor - (char *)pool);
            *used_cursor = (new_pool + used_cursor_offset); 
            used_cursor = &(*used_cursor)->next;
            if((memory_segment **)((*used_cursor)->next) == NULL) {
                *used_segments_tail = **used_cursor;
                break;
            }
        }

        munmap(pool, pool_capacity);

    } else {
        free_segments_head->memory = new_pool;
        free_segments_tail->memory = new_pool;
    }

    pool_capacity *= scale;
    pool = new_pool;
    initialized = true;

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

bool mem_pls(memory_segment *segment) {
    // if init, init.
    if(pool == NULL) {
        if(!scale_pool(pool_capacity)) {
            LOG("\nmem_pls: Failed to scale pool.\n");
            return false;
        }
    }
    
    // find a slot.
    if(!fit_segment(pool_capacity, segment)) {
        LOG("\nmem_pls: Failed to fit segment into pool.\n");
        return false;
    }

    return true;
}

