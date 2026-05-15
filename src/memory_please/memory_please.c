#include <sys/mman.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "./memory_please.h"

static void * pool;
static bool initialized = false;

static size_t pool_used = 0; // per byte.
static size_t pool_capacity = 16384; // per byte (16kb default).

// head nodes are dummy sentinels, tails are literal.
static memory_segment used_segments_head = {0};
static memory_segment *used_segments_tail = &used_segments_head;

static memory_segment free_segments_head = {0};
static memory_segment *free_segments_tail = &free_segments_head;

static bool scale_pool(size_t scale) {
    void *new_pool = mmap(NULL, (pool_capacity * scale), PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if(new_pool == MAP_FAILED) {
        LOG("+ scale_pool: Failed to map memory.\n");
        return false;
    }

    if(initialized && pool != NULL) {
        LOG("+ scale_pool: Pool was already initialized, copying data to new pool.\n");
        memcpy(new_pool, pool, pool_capacity); 

        // following loops clean up dangling pointers.
        
        memory_segment **free_cursor = &(free_segments_head.next);
        while(free_cursor != NULL) {
            size_t free_cursor_offset = (((char *)*free_cursor) - (char *)pool);
            *free_cursor = (memory_segment *)((char *)new_pool + free_cursor_offset);
            free_cursor = &(*free_cursor)->next;
            if((*free_cursor)->next == NULL) {
                *free_segments_tail = **free_cursor;
                break;
            }
        }

        memory_segment **used_cursor = &(used_segments_head.next);
        while(used_cursor != NULL) {
            size_t used_cursor_offset = ((char *)*used_cursor - (char *)pool);
            *used_cursor = (memory_segment *)((char *)new_pool + used_cursor_offset); 
            used_cursor = &(*used_cursor)->next;
            if((*used_cursor)->next == NULL) {
                *used_segments_tail = **used_cursor;
                break;
            }
        }

        LOG("+ scale_pool: Cleaning up old pool.\n");
        munmap(pool, pool_capacity);

    } else {
        LOG("+ scale_pool: Pool was NULL, assigning node pointers to new pool.\n");
        // capacity of init node is the entire alloc, it has to be broken up per request.
        free_segments_head.next = (memory_segment *)new_pool; 
        *(free_segments_tail = free_segments_head.next) = (memory_segment){
            .memory = (new_pool + sizeof(memory_segment)),
            .capacity = pool_capacity,
            .used = 0
        };
    } 

    pool_capacity *= scale;
    pool = new_pool;
    initialized = true;

    return true;
}

static bool fit_segment(size_t size, void *memory) {
    // check if the requested segment will fit in the existing nodes.
    // init case is handled by breaking the free segment within the loop.
    memory_segment *cursor = free_segments_head.next;
    while(cursor != NULL) { 
        // if fits, it sits.
        if(cursor->capacity >= size) {
            LOG("+ fit_segment: Eligible free node found.\n");
            // set pointer to that loc.
            memory_segment *segment = cursor;
            segment->capacity = size;

            // if slot larger than new segment.
            if(size < cursor->capacity) {
                LOG("+ fit_segment: Free node larger than size parameter, adjusting tracked nodes.\n");
                memory_segment *new_free_node = (memory_segment *)((char *)cursor + size);

                // heal free list.
                cursor->previous->next = new_free_node;
                new_free_node->previous = cursor->previous;

                cursor->next->previous = new_free_node;
                new_free_node->next = cursor->next;

            } else {
                // heal free list.
                cursor->previous->next = cursor->next;
                cursor->next->previous = cursor->previous;
            }

            // add to used list.
            used_segments_tail->next = cursor;
            cursor->previous = used_segments_tail->next;
            cursor->next = NULL;
            used_segments_tail = cursor;
            
            LOG("+ fit_segment: Adjusting size .\n");
            cursor->used += size;

            return true;
        }
        cursor = cursor->next;
    }

    // if no free node found, grow used.
    // if no room, grow pool first.

    // if pool too small, grow pool.
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
        if(!scale_pool(1)) {
            LOG("+ mem_pls: Failed to scale pool.\n");
            return false;
        }
    }
    
    // find a slot.
    if(!fit_segment(pool_capacity, segment)) {
        LOG("+ mem_pls: Failed to fit segment into pool.\n");
        return false;
    }

    return true;
}

