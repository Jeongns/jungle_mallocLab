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
#define ALIGN_BLOCK_SIZE(size) ((size) + (META_SIZE * 2 + POINTER_SIZE * 2) + (ALIGNMENT - 1) & ~0x7)
#define CHUNKSIZE (1 << 8)  // 4096 bytes = 4KB

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) > (y) ? (y) : (x))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int*)(p))
#define PUT(p, value) (*(unsigned int*)(p) = (value))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define PREV_FOOTER_POINTER(bp) ((char*)(bp) - (META_SIZE * 2))
#define HEADER_POINTER(bp) ((char*)(bp) - META_SIZE)
#define FOOTER_POINTER(bp) ((char*)(bp) + GET_SIZE(HEADER_POINTER(bp)) - (META_SIZE * 2))

#define NEXT_BLOCK_POINTER(bp) ((char*)(bp) + GET_SIZE(HEADER_POINTER(bp)))
#define PREV_BLOCK_POINTER(bp) ((char*)(bp) - GET_SIZE(PREV_FOOTER_POINTER(bp)))

#define NEXT_FREE(bp) (*(void**)bp)
#define PREV_FREE(bp) (*(void**)((bp) + POINTER_SIZE))

static char* heap_listp;
static char* free_listp = NULL;

static void addFreeBlock(void* bp) {
    NEXT_FREE(bp) = free_listp;
    PREV_FREE(bp) = 0;
    if (free_listp != NULL) PREV_FREE(free_listp) = bp;
    free_listp = bp;
}

static void removeFreeBlock(void* bp) {
    if (PREV_FREE(bp) == NULL)
        free_listp = NEXT_FREE(bp);
    else
        NEXT_FREE(PREV_FREE(bp)) = NEXT_FREE(bp);

    if (NEXT_FREE(bp) != NULL) PREV_FREE(NEXT_FREE(bp)) = PREV_FREE(bp);
}

static void* coalesce(void* bp) {
    size_t prev_aclloc = GET_ALLOC(PREV_FOOTER_POINTER(bp));
    size_t next_aclloc = GET_ALLOC(HEADER_POINTER(NEXT_BLOCK_POINTER(bp)));
    size_t size = GET_SIZE(HEADER_POINTER(bp));

    if (prev_aclloc && next_aclloc) {
        addFreeBlock(bp);
        return bp;
    }

    if (!next_aclloc) removeFreeBlock(NEXT_BLOCK_POINTER(bp));
    if (!prev_aclloc) removeFreeBlock(PREV_BLOCK_POINTER(bp));

    size += GET_SIZE(PREV_FOOTER_POINTER(bp)) * !prev_aclloc +
            GET_SIZE(HEADER_POINTER(NEXT_BLOCK_POINTER(bp))) * !next_aclloc;

    if (!prev_aclloc) bp = PREV_BLOCK_POINTER(bp);
    PUT(HEADER_POINTER(bp), PACK(size, 0));
    PUT(FOOTER_POINTER(bp), PACK(size, 0));
    addFreeBlock(bp);
    return bp;
}

static void* extend_heap(size_t byte) {
    char* bp;
    size_t size = byte;
    if ((bp = mem_sbrk(size)) == (void*)-1) return NULL;

    PUT(HEADER_POINTER(bp), PACK(size, 0));
    PUT(FOOTER_POINTER(bp), PACK(size, 0));
    PUT(HEADER_POINTER(NEXT_BLOCK_POINTER(bp)), PACK(0, 1));

    return coalesce(bp);
}

static void* find_fit(size_t asize) {
    void* cur = free_listp;
    while (cur != NULL) {
        if (GET_SIZE(HEADER_POINTER(cur)) >= asize) return cur;
        cur = NEXT_FREE(cur);
    }

    return NULL;
}

static void place(void* bp, size_t asize) {
    size_t csize = GET_SIZE(HEADER_POINTER(bp));
    removeFreeBlock(bp);
    if ((csize - asize) >= (META_SIZE * 2 + POINTER_SIZE * 2)) {
        PUT(HEADER_POINTER(bp), PACK(asize, 1));
        PUT(FOOTER_POINTER(bp), PACK(asize, 1));
        bp = NEXT_BLOCK_POINTER(bp);
        PUT(HEADER_POINTER(bp), PACK(csize - asize, 0));
        PUT(FOOTER_POINTER(bp), PACK(csize - asize, 0));
        addFreeBlock(bp);
        return;
    }
    PUT(HEADER_POINTER(bp), PACK(csize, 1));
    PUT(FOOTER_POINTER(bp), PACK(csize, 1));
}

//

int mm_init(void) {
    if ((heap_listp = mem_sbrk(4 * META_SIZE)) == (void*)-1) return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (META_SIZE * 1), PACK(META_SIZE * 2, 1));
    PUT(heap_listp + (META_SIZE * 2), PACK(META_SIZE * 2, 1));
    PUT(heap_listp + (META_SIZE * 3), PACK(0, 1));
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

    asize = ALIGN_BLOCK_SIZE(size);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extend_size = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extend_size)) == NULL) return NULL;
    place(bp, asize);
    return bp;
}

void mm_free(void* ptr) {
    size_t size = GET_SIZE(HEADER_POINTER(ptr));
    PUT(HEADER_POINTER(ptr), PACK(size, 0));
    PUT(FOOTER_POINTER(ptr), PACK(size, 0));

    coalesce(ptr);
}

void* mm_realloc(void* ptr, size_t size) {
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size_t oldSize = GET_SIZE(HEADER_POINTER(ptr));
    size_t asize = ALIGN_BLOCK_SIZE(size);
    //TODO 뒤 앞 합칠수 있는지

    void* new_pointer = mm_malloc(size);
    if (new_pointer == NULL) return NULL;

    size_t copySize = MIN(oldSize - META_SIZE * 2, size);
    memcpy(new_pointer, ptr, copySize);

    mm_free(ptr);
    return new_pointer;
}
