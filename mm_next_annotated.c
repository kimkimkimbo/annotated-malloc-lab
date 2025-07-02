/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.


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

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "11team",
    /* First member's full name */
    "kim boa",
    /* First member's email address */
    "303-9",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define ALIGNMENT 8

#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static char *heap_listp = NULL;
static char *last_fitp;

int mm_init(void)
{

    if ((heap_listp = mem_sbrk(4 * WSIZE)) == ((void *)-1))
    {
        return -1;
    }

    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));

    heap_listp += (2 * WSIZE);
    last_fitp = heap_listp;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }

    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? ((words + 1) * WSIZE) : (words * WSIZE);

    if ((long)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

void *coalesce(void *bp)
{
    size_t prev_alloc;
    if ((char *)bp == heap_listp)
    {
        prev_alloc = 1;
    }
    else
    {
        prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    }
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
    {
        return bp;
    }
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    //병합이 되고나면 병합된 블록 헤더를 가리키도록 변경해 줘야 함.
    last_fitp = bp;
    return bp;
}

void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL)
    {
        //printf("mm_malloc1 %p / %d \n", bp, asize);
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;

    //printf("mm_malloc2 %p / %d \n", bp, asize);
    place(bp, asize);
    return bp;
}

void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    coalesce(bp);
}

void *mm_realloc(void *bp, size_t size)
{
    if (size == 0)
    {
        mm_free(bp);
        return NULL;
    }
    if (bp == NULL)
    {
        return mm_malloc(size);
    }

    size_t oldsize = GET_SIZE(HDRP(bp));
    size_t asize;

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if (asize == oldsize)
        return bp;

    void *new_bp = mm_malloc(size);
    if (new_bp == NULL)
        return NULL;

    size_t copySize = oldsize - DSIZE;
    if (size < copySize)
        copySize = size;
    memcpy(new_bp, bp, copySize);

    mm_free(bp);
    return new_bp;
}

static void *find_fit(size_t asize)
{
    // 1. last_fitp부터 끝까지
    // 2. 못 찾으면 heap_listp부터 last_fitp 직전까지 다시 돌아와서 찾는 구조
    /*
    while (GET_SIZE(p) > 0)
if (!GET_ALLOC(p) && GET_SIZE(p) >= asize)
수정 필요
     */
    char *p = last_fitp;
    int count = 0;

    // GET_SIZE(p) == 0인 블록이 epilogue block 끝
    while (GET_SIZE(HDRP(p)) > 0)
    {
        //printf("find_fit: p=%p, size=%u, alloc=%u\n", p, GET_SIZE(HDRP(p)), GET_ALLOC(HDRP(p)));
        // TODO: 조건 검사 후 적절하면 리턴
        // 블록이 가용하고 크기가 충분한 경우
        if (!GET_ALLOC(HDRP(p)) && GET_SIZE(HDRP(p)) >= asize)
        {
            last_fitp = p;
            //printf("last_fitp : %p", last_fitp);
            return p;
        }

        // TODO: NEXT_BLKP(p)로 이동
        p = NEXT_BLKP(p);
        //if (++count > 1000) { printf("find_fit: 무한루프 감지!\n"); break; }
    }

    p = heap_listp;

    while (p < last_fitp)
    {
        if (!GET_ALLOC(HDRP(p)) && GET_SIZE(HDRP(p)) >= asize)
        {
            last_fitp = p;
            return p;
        }

        p = NEXT_BLKP(p);
    }

    return NULL;
}

/*
1. 남는 공간이 충분한지 확인
2. 그렇지 않다면 전체 블록을 할당 처리

*/
static void place(void *bp, size_t asize)
{

    //printf("place %p\n", bp);
    if (bp == NULL || GET_SIZE(HDRP(bp)) == 0)
    {
        //printf("place1 %p", bp);
        return;
    }
    
    //printf("place2 %p\n", bp);
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE))
    {
        //printf("place3 %p\n", bp);
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }

    last_fitp = bp;
}