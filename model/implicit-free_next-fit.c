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
#define ALIGNMENT 8
#define CHUNKSIZE (1 << 12)  // 4096 bytes = 4KB

#define MAX(x, y) ((x) > (y) ? (x) : (y))

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

char* heap_listp;
char* last_bp;

static void* coalesce(void* bp) {
    size_t prev_aclloc = GET_ALLOC(PREV_FOOTER_POINTER(bp));
    size_t next_aclloc = GET_ALLOC(HEADER_POINTER(NEXT_BLOCK_POINTER(bp)));
    size_t size = GET_SIZE(HEADER_POINTER(bp));

    if (prev_aclloc && next_aclloc) return bp;
    size += GET_SIZE(PREV_FOOTER_POINTER(bp)) * !prev_aclloc +
            GET_SIZE(HEADER_POINTER(NEXT_BLOCK_POINTER(bp))) * !next_aclloc;

    if (!prev_aclloc) bp = PREV_BLOCK_POINTER(bp);
    PUT(HEADER_POINTER(bp), PACK(size, 0));
    PUT(FOOTER_POINTER(bp), PACK(size, 0));

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
    void* bp = last_bp;

    while (GET_SIZE(HEADER_POINTER(bp)) > 0) {
        if (!GET_ALLOC(HEADER_POINTER(bp)) && (asize <= GET_SIZE(HEADER_POINTER(bp)))) {
            last_bp = bp;
            return bp;
        }
        bp = NEXT_BLOCK_POINTER(bp);
    }

    bp = heap_listp;
    while (bp < last_bp) {
        if (!GET_ALLOC(HEADER_POINTER(bp)) && (asize <= GET_SIZE(HEADER_POINTER(bp)))) {
            last_bp = bp;
            return bp;
        }
        bp = NEXT_BLOCK_POINTER(bp);
    }

    return NULL;
}

static void place(void* bp, size_t asize) {
    size_t csize = GET_SIZE(HEADER_POINTER(bp));
    if ((csize - asize) >= (META_SIZE * 2 + ALIGNMENT)) {
        PUT(HEADER_POINTER(bp), PACK(asize, 1));
        PUT(FOOTER_POINTER(bp), PACK(asize, 1));
        bp = NEXT_BLOCK_POINTER(bp);
        PUT(HEADER_POINTER(bp), PACK(csize - asize, 0));
        PUT(FOOTER_POINTER(bp), PACK(csize - asize, 0));
        return;
    }
    PUT(HEADER_POINTER(bp), PACK(csize, 1));
    PUT(FOOTER_POINTER(bp), PACK(csize, 1));
}

int mm_init(void) {
    if ((heap_listp = mem_sbrk(4 * META_SIZE)) == (void*)-1) return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (META_SIZE * 1), PACK(META_SIZE * 2, 1));
    PUT(heap_listp + (META_SIZE * 2), PACK(META_SIZE * 2, 1));
    PUT(heap_listp + (META_SIZE * 3), PACK(0, 1));
    heap_listp += META_SIZE * 2;
    last_bp = heap_listp;

    if (extend_heap(CHUNKSIZE) == NULL) return -1;

    return 0;
}

void* mm_malloc(size_t size) {
    size_t asize;
    size_t extendSize;
    char* bp;

    if (size == 0) return 0;

    asize = ((size + 2 * META_SIZE + (ALIGNMENT - 1)) & ~0x7);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendSize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendSize)) == NULL) return NULL;
    last_bp = bp;
    place(bp, asize);
    return bp;
}

void mm_free(void* ptr) {
    size_t size = GET_SIZE(HEADER_POINTER(ptr));
    PUT(HEADER_POINTER(ptr), PACK(size, 0));
    PUT(FOOTER_POINTER(ptr), PACK(size, 0));

    last_bp = coalesce(ptr);
}

void* mm_realloc(void* ptr, size_t size) {
    size_t pointerSize = GET_SIZE(HEADER_POINTER(ptr)) - META_SIZE * 2;

    if (pointerSize == size) return ptr;
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    if (size < pointerSize) {
        place(ptr, size + META_SIZE * 2);
        return ptr;
    }

    void* newPointer = mm_malloc(size);

    size_t copy_size = GET_SIZE(HEADER_POINTER(ptr)) - META_SIZE * 2;
    for (size_t i = 0; i < copy_size; i++) *((char*)newPointer + i) = *((char*)ptr + i);

    mm_free(ptr);

    return newPointer;
}
