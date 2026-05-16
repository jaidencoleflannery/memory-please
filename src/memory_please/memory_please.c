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

/*
 * NEED TO MAKE IT SO ALL FUNCS RETURN/DEAL WITH memory_segment NOT VOID * !!!
 */

static bool scale_pool(size_t scale) {
    void *new_pool = mmap(NULL, (pool_capacity * scale), PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if(new_pool == MAP_FAILED) {
        LOG("+ scale_pool: Failed to map memory.\n");
        return false;
    }

    if(initialized && pool != NULL) {
        LOG("+ scale_pool: The pool was already initialized, copying data to new pool.\n");
        memcpy(new_pool, pool, pool_capacity); 

        // following loops clean up dangling pointers.
        
        memory_segment **free_cursor = &(free_segments_head.next);
        while(free_cursor != NULL) {
            size_t free_cursor_offset = (((char *)*free_cursor) - (char *)pool);
            *free_cursor = (memory_segment *)((char *)new_pool + free_cursor_offset);
            free_cursor = &(*free_cursor)->next;
            if((*free_cursor)->next == NULL) { 
                *free_segments_tail = **free_cursor;
                LOG("+ scale_pool: Cleaned up linked list of free nodes.\n");
                break;
            }
            LOG("+ scale_pool: Cleanup of free nodes fell through without hitting break, list was not properly re-linked.\n");
        }

        memory_segment **used_cursor = &(used_segments_head.next);
        while(used_cursor != NULL) {
            size_t used_cursor_offset = ((char *)*used_cursor - (char *)pool);
            *used_cursor = (memory_segment *)((char *)new_pool + used_cursor_offset); 
            used_cursor = &(*used_cursor)->next;
            if((*used_cursor)->next == NULL) {
                *used_segments_tail = **used_cursor;
                LOG("+ scale_pool: Cleaned up linked list of used nodes.\n");
                break;
            }
            LOG("+ scale_pool: Cleanup of used nodes fell through without hitting break, list was not properly re-linked.\n");
        }

        LOG("+ scale_pool: Cleaning up old pool.\n");
        munmap(pool, pool_capacity);

    } else {
        LOG("+ scale_pool: The pool was NULL, assigning node pointers to new pool.\n");
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
    // init instance case is handled by breaking the free segment within the loop.
    memory_segment *cursor = free_segments_head.next;
    while(cursor != NULL) { 
        // if fits, it sits.
        if(cursor->capacity >= size) {
            LOG("+ fit_segment: Eligible free node found.\n");
            // set pointer to that loc.
            memory_segment *segment = cursor;
            *segment = (memory_segment){
                .memory = (((char *)segment) + sizeof(memory_segment)),
                .capacity = size,
                .used = 0
            };

            // if slot is some epsilon factor larger than new segment, break it up.
            // otherwise just give them the whole chunk to avoid fragmentation.
            if((cursor->capacity - size) > (cursor->capacity * EPSILON)) {
                LOG("+ fit_segment: Free node larger than size parameter, adjusting tracked nodes.\n");
                memory_segment *new_free_node = (memory_segment *)((char *)cursor + size);
                *new_free_node = (memory_segment){
                    .memory = (((char *)new_free_node) + sizeof(memory_segment)),
                    .capacity = (cursor->capacity - size),
                    .used = 0
                };

                // heal free list.
                cursor->previous->next = new_free_node;
                new_free_node->previous = cursor->previous;

                cursor->next->previous = new_free_node;
                new_free_node->next = cursor->next;

            } else {
                LOG("+ fit_segment: Free node was of similar size, giving entire free node.\n");
                segment->capacity = cursor->capacity;
                // heal free list.
                cursor->previous->next = cursor->next;
                cursor->next->previous = cursor->previous;
            }

            // add to used list.
            used_segments_tail->next = segment;
            segment->previous = used_segments_tail;
            used_segments_tail = cursor;
            segment->next = NULL;  
            
            LOG("+ fit_segment: Adjusting size.\n");
            segment->used += size;

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
    new_segment->previous = used_segments_tail;
    used_segments_tail = new_segment;

    return true;
}


bool mem_free(memory_segment *segment) {
    bool found = false;
    memory_segment *cursor = &used_segments_head;
    memory_segment *prev = NULL;

    // heal used list.
    segment->previous->next = segment->next;
    segment->next->previous = segment->previous;

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

