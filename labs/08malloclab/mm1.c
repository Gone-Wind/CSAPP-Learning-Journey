// 基于 隐式空闲链表的 堆实现
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

#define WSIZE       4       
#define DSIZE       8       
#define CHUNKSIZE  (1<<12)  

#define MAX(x, y) ((x) > (y)? (x) : (y))

#define PACK(size, alloc)  ((size) | (alloc))

#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static char *heap_listp = 0;  /* Pointer to first block */  

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */

int mm_init(void)
{
    // 初始化为16字节
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    
    PUT(heap_listp, 0);                          // 对齐内存块
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // 序言块
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); 
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     // 结尾块
    heap_listp += (2*WSIZE);                     // 指向序言快块内容(0字节)

    // 首次初始化 普通块
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) 
        return -1;

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      
    size_t extendsize; 
    char *bp;      

    if (size == 0)
        return NULL;

    // 调节分配块大小，使其8字节对齐
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    // 判读是否有空闲块能够使用
    if ((bp = find_fit(asize)) != NULL) {  
        place(bp, asize);                  
        return bp;
    }

    // 若无，则扩展新内存
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)  
        return NULL;                                  

    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if (ptr == 0) 
        return;

    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    if (ptr == NULL) {
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    
    size_t oldSize = GET_SIZE(HDRP(oldptr));
    copySize = oldSize - DSIZE; // 原数据有效载荷字节大小
    if (size < copySize)
      copySize = size;
      
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; //使大小8字对齐

    if ((long)(bp = mem_sbrk(size)) == -1)  
        return NULL;                                        

    PUT(HDRP(bp), PACK(size, 0));         
    PUT(FTRP(bp), PACK(size, 0));         
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); 

    return coalesce(bp);
}

static void *coalesce(void *bp) 
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {            
        return bp;
    }

    else if (prev_alloc && !next_alloc) {      
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    }

    else if (!prev_alloc && next_alloc) {      
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {                                     
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

// 采用 First Fit 策略
static void *find_fit(size_t asize)
{
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL; 
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));   

    // 若空闲块太大则分割
    if ((csize - asize) >= (2*DSIZE)) { 
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    else { 
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}