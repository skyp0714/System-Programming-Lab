/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

// Global variables
static void *heap_listp;            /* Pointing starting adress of the heap*/
static void *last_fitp;             /* Pointing last adress that was allocated*/
// Functions
static void *extend_heap(size_t size);
static void *coalesce(void *bp);
static void *place(void *bp, size_t asize);
static void *find_first_fit(size_t asize);
static void *find_next_fit(size_t asize);

// Constants
#define ALIGNMENT   8           /* single word (4) or double word (8) alignment */
#define WSIZE       4           /* Word and hearder/footer size (bytes) */
#define DSIZE       8           /* Double word size (bytes) */
#define INITSIZE    (1<<6)      /* Initial size of heap */
#define CHUNKSIZE   (1<<12)     /* Extend heap by this amount (bytes) */
#define MINBLKSIZE  16          /* Minimum size of allocated block */
#define THRESHOLD   100         /* Threshold determining the fit policy */

// Macros
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)                   /* rounds up to the nearest multiple of ALIGNMENT */

#define MAX(x, y)   ((x) > (y) ? (x) : (y))                             /* Max function */

#define PACK(size, alloc) ((size) | (alloc))                            /* Generating header/footer content */

#define GET(p)          (*(unsigned int*)(p))                           /* Get pt value */
#define PUT(p, val)     (*(unsigned int*)(p) = (val))                   /* Assign pt value */

#define GET_SIZE(p)     (GET(p) & ~0x7)                                 /* Get size from header/footer */
#define GET_ALLOC(p)    (GET(p) & 0x1)                                  /* Get aloocation from header/footer */

#define HDRP(bp)        ((char*)(bp) - WSIZE)                           /* Header pointer */
#define FTRP(bp)        ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)      /* Footer pointer */

#define NEXT_BLKP(bp)   ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE))) /* Next block pointer */
#define PREV_BLKP(bp)   ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE))) /* Prev block pointer */


// Helper Functions
/* 
 * extend-heap - extend heap in two cases
 * (1) when heap is initialized
 * (2) when mm_malloc is unable to fit
 */
static void *extend_heap(size_t size){
    char *bp;
    size_t asize = ALIGN(size);
    if((long)(bp=mem_sbrk(asize)) == -1)
        return NULL;
    // Set free block header/footer
    PUT(HDRP(bp), PACK(asize, 0));
    PUT(FTRP(bp), PACK(asize, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));            /* New Epilogue */
    // Coalesce with the previous block
    return coalesce(bp);
}
/* 
 * coalesce - coalesce current block & prev, next free blocks
 */
static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    size_t last_chk = 0, isbig = 0;

    if(size>THRESHOLD) isbig =1;

    if(prev_alloc && next_alloc){                   /* Case 1 */
        return bp;
    }else if(prev_alloc && !next_alloc){            /* Case 2 */
        if(last_fitp == NEXT_BLKP(bp)) last_chk = 1;
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }else if(!prev_alloc && next_alloc){            /* Case 3 */
        if(last_fitp == bp) last_chk = 1;
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }else if(!prev_alloc && !next_alloc){           /* Case 4 */
        if((last_fitp == NEXT_BLKP(bp)) | (last_fitp == bp)) last_chk = 1;
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // If big block was eleminated, modify last_fitp
    if(last_chk&isbig) last_fitp = bp;
    return bp;
}
/* 
 * place - allocate asize at bp (splitting optional)
 * split if the remainder is bigger than minimum block size
 */
static void *place(void *bp, size_t asize){
    size_t bsize = GET_SIZE(HDRP(bp));
    size_t rsize = bsize - asize;
    
    if(rsize >= MINBLKSIZE){
        // split blocks
        if(asize > THRESHOLD){                  /* big block -> place at back*/
            PUT(HDRP(bp), PACK(rsize, 0));
            PUT(FTRP(bp), PACK(rsize, 0));
            bp = NEXT_BLKP(bp);
            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));
        }else{                                  /* big block -> place at front*/
            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));
            bp = NEXT_BLKP(bp);
            PUT(HDRP(bp), PACK(rsize, 0));
            PUT(FTRP(bp), PACK(rsize, 0));
            bp = PREV_BLKP(bp);
        }
    }else{
        // no split
        PUT(HDRP(bp), PACK(bsize, 1));
        PUT(FTRP(bp), PACK(bsize, 1));
    }
    last_fitp = bp;
    return bp;
}
/* 
 * fitting policy - find place to allocate asize
 * first_fit and next_fit
 */
 static void *find_first_fit(size_t asize){
    void *bp;
    size_t size;
    // Search from the beginning to the end
    for(bp = heap_listp; (size=GET_SIZE(HDRP(bp))) > 0; bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (asize <= size)){
            return bp;
        }
    }
    return NULL;
}
static void *find_next_fit(size_t asize){
    void *bp;
    size_t size;
    // Search from the last fit place
    for(bp = last_fitp; (size=GET_SIZE(HDRP(bp))) > 0; bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (asize <= size)){
            last_fitp = bp;
            return bp;
        }
    }
    for(bp = heap_listp; bp < last_fitp; bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
            last_fitp = bp;
            return bp;
        }
    }
    return NULL;
}



// mm Functions
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if((heap_listp = mem_sbrk(2*DSIZE)) == (void *) -1)
        return -1;
    PUT(heap_listp, 0);                         /* Alignment padding */
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));    /* Prologue header */
    PUT(heap_listp + 2*WSIZE, PACK(DSIZE, 1));  /* Prologue footer */
    PUT(heap_listp + 3*WSIZE, PACK(0, 1));      /* Epilogue footer */
    heap_listp += 2*WSIZE;

    // extend heap
    if(extend_heap(INITSIZE) == NULL)
        return -1;
    last_fitp = heap_listp;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;               /* adjusted block size */
    char *bp;
    // Ignore supirious requests
    if(size == 0)
        return NULL;
    // Adjust block size(header, footer and alignment)
    asize = ALIGN(size+DSIZE);
    
    // Search for free block
    // small bloick-first fit, big block-next fit
    if(asize < THRESHOLD) bp = find_first_fit(asize);
    else bp = find_next_fit(asize);
    
    // Found fit
    if(bp != NULL){
        bp = place(bp, asize);
        return bp;
    }

    // No fit found.-> extend heap
    if((bp = extend_heap(MAX(asize, CHUNKSIZE))) == NULL)
        return NULL;
    bp = place(bp, asize);

    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    // coalesce at every free instruction(constant time)
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size)
{
    // old_size, size : size of content
    // asize, ext_size : total size of block
    size_t old_size = GET_SIZE(HDRP(bp))-DSIZE;
    size_t asize = ALIGN(size+DSIZE);

    // Exceptions: NULLptr, zero_size, no allocated block
    if(bp == NULL) return mm_malloc(size);
    if(size == 0) free(bp);
    if(!GET_ALLOC(HDRP(bp))) return NULL;

    // if new size is smaller, assign to current place.
    if(size<=old_size){
        //place(bp, asize);
        return bp;
    }else{
        // If prev/next block is free, coalesce current block
        void * tmp_bp = coalesce(bp);
        size_t csize = GET_SIZE(HDRP(tmp_bp));
        if(csize > asize){
            if(bp != tmp_bp)    memmove(tmp_bp, bp, old_size);
            PUT(HDRP(tmp_bp), PACK(csize, 1));
            PUT(FTRP(tmp_bp), PACK(csize, 1));
            return tmp_bp;
        }
        // Even coalescing is not enough
        // Move to a new place
        void *new_bp = mm_malloc(size);
        memcpy(new_bp, bp, old_size);
        mm_free(bp);
        return new_bp;
    }
}
