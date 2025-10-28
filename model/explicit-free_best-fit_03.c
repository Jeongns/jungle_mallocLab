#include "mm.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "7조",
    /* First member's full name */
    "이정훈",
    /* First member's email address */
    "jhlee030313@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

#define META_SIZE 4
#define POINTER_SIZE 8
#define ALIGNMENT 8
#define MIN_SIZE 24
#define ALIGN_BLOCK_SIZE(size) ((size) + (ALIGNMENT - 1) & ~0x7)
#define CHUNKSIZE (1 << 8)

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) > (y) ? (y) : (x))

#define PACK(size, alloc, prev_alloc) ((size) | (alloc) | (prev_alloc << 1))

#define GET(p) (*(unsigned int*)(p))
#define PUT(p, value) (*(unsigned int*)(p) = (value))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_PREV_ALLOC(p) (!!(GET(p) & 0x2))

#define PREV_FOOTER_POINTER(bp) ((char*)(bp) - (META_SIZE * 2))
#define HEADER_POINTER(bp) ((char*)(bp) - META_SIZE)
#define FOOTER_POINTER(bp) ((char*)(bp) + GET_SIZE(HEADER_POINTER(bp)) - (META_SIZE * 2))

#define NEXT_BLOCK_POINTER(bp) ((char*)(bp) + GET_SIZE(HEADER_POINTER(bp)))
#define PREV_BLOCK_POINTER(bp) ((char*)(bp) - GET_SIZE(PREV_FOOTER_POINTER(bp)))

#define NEXT_FREE(bp) (*(void**)bp)
#define PREV_FREE(bp) (*(void**)((bp) + POINTER_SIZE))

static char* heap_listp;
static char* free_listp = NULL;

static void add_free_block(void* bp) {
    NEXT_FREE(bp) = free_listp;
    PREV_FREE(bp) = 0;
    if (free_listp != NULL) PREV_FREE(free_listp) = bp;
    free_listp = bp;
}

static void remove_free_block(void* bp) {
    if (PREV_FREE(bp) == NULL)
        free_listp = NEXT_FREE(bp);
    else
        NEXT_FREE(PREV_FREE(bp)) = NEXT_FREE(bp);

    if (NEXT_FREE(bp) != NULL) PREV_FREE(NEXT_FREE(bp)) = PREV_FREE(bp);
}

static void* coalesce(void* bp) {
    size_t prev_aclloc = GET_PREV_ALLOC(HEADER_POINTER(bp));
    size_t next_aclloc = GET_ALLOC(HEADER_POINTER(NEXT_BLOCK_POINTER(bp)));
    size_t size = GET_SIZE(HEADER_POINTER(bp));

    if (prev_aclloc && next_aclloc) {
        add_free_block(bp);
        return bp;
    }

    if (!next_aclloc) remove_free_block(NEXT_BLOCK_POINTER(bp));
    if (!prev_aclloc) remove_free_block(PREV_BLOCK_POINTER(bp));

    size += GET_SIZE(PREV_FOOTER_POINTER(bp)) * !prev_aclloc +
            GET_SIZE(HEADER_POINTER(NEXT_BLOCK_POINTER(bp))) * !next_aclloc;

    if (!prev_aclloc) bp = PREV_BLOCK_POINTER(bp);
    PUT(HEADER_POINTER(bp), PACK(size, 0, 1));
    PUT(FOOTER_POINTER(bp), PACK(size, 0, 1));
    add_free_block(bp);
    return bp;
}

static void* extend_heap(size_t byte) {
    char* bp;
    size_t size = byte;

    char* ep_bp = (char*)mem_heap_hi() + 1;
    char* last_bp = PREV_BLOCK_POINTER(ep_bp);

    if ((bp = mem_sbrk(size)) == (void*)-1) return NULL;

    PUT(HEADER_POINTER(bp), PACK(size, 0, GET_ALLOC(HEADER_POINTER(last_bp))));
    PUT(FOOTER_POINTER(bp), PACK(size, 0, GET_ALLOC(HEADER_POINTER(last_bp))));

    PUT(HEADER_POINTER(NEXT_BLOCK_POINTER(bp)), PACK(0, 1, 0));

    return coalesce(bp);
}

static void* find_fit(size_t asize) {
    void* cur = free_listp;
    void* best_bp = NULL;
    size_t best_size = (size_t)-1;

    while (cur != NULL) {
        size_t cur_size = GET_SIZE(HEADER_POINTER(cur));
        if (cur_size >= asize) {
            if (cur_size == asize) return cur;
            if (cur_size < best_size) {
                best_size = cur_size;
                best_bp = cur;
            }
        }
        cur = NEXT_FREE(cur);
    }

    return best_bp;
}

static void place(void* bp, size_t asize) {
    size_t csize = GET_SIZE(HEADER_POINTER(bp));
    remove_free_block(bp);
    if ((csize - asize) >= (META_SIZE * 2 + POINTER_SIZE * 2)) {
        PUT(HEADER_POINTER(bp), PACK(asize, 1, GET_PREV_ALLOC(HEADER_POINTER(bp))));
        PUT(FOOTER_POINTER(bp), PACK(asize, 1, GET_PREV_ALLOC(HEADER_POINTER(bp))));
        bp = NEXT_BLOCK_POINTER(bp);
        PUT(HEADER_POINTER(bp), PACK(csize - asize, 0, 1));
        PUT(FOOTER_POINTER(bp), PACK(csize - asize, 0, 1));
        add_free_block(bp);
        return;
    }
    PUT(HEADER_POINTER(bp), PACK(csize, 1, GET_PREV_ALLOC(HEADER_POINTER(bp))));
    PUT(FOOTER_POINTER(bp), PACK(csize, 1, GET_PREV_ALLOC(HEADER_POINTER(bp))));

    void* next_header = HEADER_POINTER(NEXT_BLOCK_POINTER(bp));
    PUT(next_header, PACK(GET_SIZE(next_header), GET_ALLOC(next_header), 1));
}

//

int mm_init(void) {
    if ((heap_listp = mem_sbrk(4 * META_SIZE)) == (void*)-1) return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (META_SIZE * 1), PACK(META_SIZE * 2, 1, 1));
    PUT(heap_listp + (META_SIZE * 2), PACK(META_SIZE * 2, 1, 1));
    PUT(heap_listp + (META_SIZE * 3), PACK(0, 1, 1));
    heap_listp += META_SIZE * 2;
    free_listp = NULL;
    if (extend_heap(CHUNKSIZE) == NULL) return -1;

    return 0;
}

void* mm_malloc(size_t size) {
    size_t asize;
    size_t extend_size;
    char* bp;

    if (size == 0) return 0;

    asize = MAX(ALIGN_BLOCK_SIZE(size) + META_SIZE * 2, MIN_SIZE);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extend_size = asize;
    char* ep_bp = (char*)mem_heap_hi() + 1;
    char* last_bp = PREV_BLOCK_POINTER(ep_bp);
    if (!GET_ALLOC(HEADER_POINTER(last_bp)))
        extend_size = asize - GET_SIZE(HEADER_POINTER(last_bp));

    if (extend_size == 0) extend_size = asize;

    if ((bp = extend_heap(extend_size)) == NULL) return NULL;
    place(bp, asize);
    return bp;
}

void mm_free(void* ptr) {
    size_t size = GET_SIZE(HEADER_POINTER(ptr));
    PUT(HEADER_POINTER(ptr), PACK(size, 0, GET_PREV_ALLOC(HEADER_POINTER(ptr))));
    PUT(FOOTER_POINTER(ptr), PACK(size, 0, GET_PREV_ALLOC(HEADER_POINTER(ptr))));

    void* next_header = HEADER_POINTER(NEXT_BLOCK_POINTER(ptr));
    PUT(next_header, PACK(GET_SIZE(next_header), GET_ALLOC(next_header), 0));

    coalesce(ptr);
}

void* mm_realloc(void* ptr, size_t size) {
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size_t oldSize = GET_SIZE(HEADER_POINTER(ptr));
    size_t asize = MAX(ALIGN_BLOCK_SIZE(size) + META_SIZE * 2, MIN_SIZE);
    size_t copySize = MIN(oldSize - META_SIZE * 2, size);

    char* temp[copySize];
    memcpy(temp, ptr, copySize);

    mm_free(ptr);

    void* new_pointer = mm_malloc(size);
    if (new_pointer == NULL) return NULL;

    memcpy(new_pointer, temp, copySize);
    return new_pointer;
}
