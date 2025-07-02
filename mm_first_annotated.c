/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.

가장 빠르지만 메모리 효율은 최악인 malloc 구현. 이 단순한 방식에서는, brk 포인터를 증가시키기만 하면 블록을 할당한다.
블록은 오직 페이로드만 존재한다. 헤더나 푸터는 없다. 블록은 절대 병합(coalescing)되거나 재사용되지 않는다.
realloc은 단순히 mm_malloc과 mm_free를 이용해 직접 구현된다.


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

#define WSIZE 4 /* 워드(4바이트) 크기, 헤더/푸터의 크기. 블록의 메타데이터(헤더/푸터)는 항상 4바이트 단위로 저장됨 */
#define DSIZE 8 // 더블 워드(8바이트) 크기, 블록 최소 단위 및 정렬 기준
#define CHUNKSIZE (1 << 12) // 힙을 한 번에 확장할 기본 크기(4096바이트)

#define MAX(x, y) ((x) > (y) ? (x) : (y)) // 두 값 중 큰 값을 반환

/*
 * PACK(size, alloc): 크기와 할당 비트를 하나의 워드로 합쳐 헤더/푸터에 저장할 값을 만든다.
 * - size: 블록의 전체 크기(바이트)
 * - alloc: 할당 여부(1: 할당, 0: free)
 * 예) PACK(16, 1) → 0x11 (16 | 1)
 */
#define PACK(size, alloc) ((size) | (alloc))

/*
 * GET(p): p가 가리키는 주소(헤더/푸터)에서 4바이트 값을 읽어온다.
 * PUT(p, val): p가 가리키는 주소(헤더/푸터)에 4바이트 값을 저장한다.
 * - p는 void* 또는 char* 타입이어야 하며, 반드시 워드(4바이트) 정렬되어야 한다.
 */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/*
 * GET_SIZE(p): 헤더/푸터에서 블록의 크기(하위 3비트 제외)를 추출한다.
 * GET_ALLOC(p): 헤더/푸터에서 할당 비트(가장 하위 비트)를 추출한다.
 * - 헤더/푸터는 [size | alloc] 형식으로 저장됨
 * - size는 항상 8의 배수이므로 하위 3비트(0x7)는 할당 비트로 사용 가능
 */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/*
 * HDRP(bp): 블록 포인터(bp, payload의 시작 주소)에서 헤더의 주소를 계산
 * - 헤더는 payload 바로 앞에 위치하므로 bp - WSIZE
 * FTRP(bp): 블록 포인터(bp)에서 푸터의 주소를 계산
 * - 푸터는 payload 시작 + 블록 크기 - DSIZE(헤더+푸터)
 */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/*
 * NEXT_BLKP(bp): 다음 블록의 payload 주소를 반환
 * - 현재 블록의 헤더에서 크기를 읽어와 bp에 더함
 * PREV_BLKP(bp): 이전 블록의 payload 주소를 반환
 * - bp에서 바로 앞 푸터를 찾아 크기를 읽어와 bp에서 뺌
 */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* single word (4) or double word (8) alignment
    8byte로 정렬*/
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT
0x7은 2진수로 0000 0111 / ~0x7은 1111 1000
이걸 bitwise AND 하면 하위 3비트를 0으로 만든다 = 즉, 8의 배수로 만든다는 뜻
*/
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

/*
sizeof(size_t)는 시스템마다 4 또는 8
보통은 sizeof(size_t) == 4 (32비트 시스템 기준)
이걸 ALIGN으로 정렬시키면 → 항상 8이 된다.
*/
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);


static char *heap_listp = NULL;

/*
 * mm_init - initialize the malloc package.

- `mm_malloc`, `mm_realloc`, `mm_free`를 호출하기 전에 응용 프로그램(트레이스 기반 드라이버 프로그램)이 초기화를 수행하기 위해 호출합니다.
- 초기 힙 영역 할당과 같은 필요한 초기화를 수행합니다.
- 초기화에 문제가 있으면 -1을 반환하고, 성공하면 0을 반환합니다.
 */
int mm_init(void)
{

    // 힙 생성 예외 처리
    // mem_sbrk(4 * WSIZE): 힙에 16바이트(4워드) 공간을 요청한다.
    // 이 공간은 프롤로그 블록(헤더+푸터)과 에필로그 헤더를 만들기 위한 최소 공간이다.
    // 만약 메모리 할당에 실패하면 -1을 반환하여 초기화 실패를 알린다.
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == ((void *)-1))
    {
        return -1;
    }

    // 첫 워드(4바이트)에 0을 저장한다.
    // 이는 더블 워드 정렬을 맞추기 위한 패딩 역할을 한다.
    PUT(heap_listp, 0);

    /*
    경계값 태그를 해 주기 위해서 푸터를 추가한 것
    */

    // 프롤로그 헤더 생성: 8바이트(DSIZE) 크기, 할당 상태(1)
    // 프롤로그 블록은 힙의 경계 조건 처리를 단순하게 해주기 위한 가짜 블록이다.
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));

    // 프롤로그 푸터 생성: 8바이트(DSIZE) 크기, 할당 상태(1)
    // 프롤로그 블록의 헤더와 푸터 모두 같은 값을 가진다.
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));

    // 에필로그 헤더: 크기 0, 할당 상태 (1)
    // 에필로그 블록은 항상 힙의 끝에 위치하며, 블록 탐색 시 힙의 끝을 알리는 역할을 한다.
    // 지금 힙 크기가 0이기 때문에 크기가 0
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));

    // heap_listp를 프롤로그 블록의 페이로드(실제 데이터 영역)로 이동
    // 이후 할당/해제 함수에서 블록 탐색 시 이 포인터를 시작점으로 사용한다.
    heap_listp += (2 * WSIZE);

    /*
    heap_listp는 mem_sbrk로 할당받은 힙의 시작 주소를 가리킵니다.
    0번째 워드: 패딩(정렬용, 4바이트)
    1번째 워드: 프롤로그 헤더(8바이트, 할당됨)
    2번째 워드: 프롤로그 푸터(8바이트, 할당됨)
    3번째 워드: 에필로그 헤더(0바이트, 할당됨)
    */

    // 힙을 CHUNKSIZE(기본 4096바이트)만큼 확장한다.
    // extend_heap 함수는 실제로 힙에 새로운 메모리 블록을 추가하고,
    // 그 블록의 헤더/푸터를 초기화하며, 필요하다면 인접한 free 블록과 병합(coalescing)까지 수행한다.
    // 만약 확장에 실패하면(NULL 반환) -1을 반환하여 초기화 실패를 알린다.
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }

    return 0;
}

// malloc 등에서 더 이상 할당할 공간이 없을 때 힙을 확장해 추가 메모리를 확보하기 위해 사용한다.
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    // words가 홀수면 1을 더해서 짝수로 만든 뒤 WSIZE(4바이트)를 곱하고, 짝수면 그대로 WSIZE를 곱한다
    size = (words % 2) ? ((words + 1) * WSIZE) : (words * WSIZE);

    /*
    동적 메모리 할당에서 블록 크기는 항상 8바이트(더블 워드) 단위로 정렬되어야 합니다.
    만약 words가 홀수라면, 8의 배수가 아니게 되어 정렬이 깨질 수 있습니다.
    그래서 홀수면 1을 더해 짝수로 만들어주고, 짝수면 그대로 사용합니다.
    */

    // 예외처리, sbrk가 실패했는지 확인한다.
    if ((long)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }

    // 힙을 확장해야 하니까 에필로그 헤더와 푸터를 프리 해 줘야한다.
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    // 새로운 에필로그 헤더 생성
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // 인접 free 블록이 있으면 병합(coalescing)하고, 그 블록의 포인터를 반환한다.
    return coalesce(bp);
}

/*
 * coalesce - 인접한 free 블록이 있으면 병합하여 하나의 큰 free 블록으로 만든다.
 *
 * [동작 흐름 및 이유]
 * 1. 이전/다음 블록의 할당 여부를 확인한다.
 * 2. 4가지 경우에 따라 병합 여부와 방법을 결정한다.
 * 3. 병합된(혹은 병합되지 않은) free 블록의 시작 주소를 반환한다.
 */
void *coalesce(void *bp)
{
    // 1. 이전 블록의 할당 여부를 확인 (이전 블록의 푸터에서 alloc 비트 확인)
    size_t prev_alloc;
    // heap_listp는 항상 첫 번째 실제 블록의 payload를 가리킴
    if ((char*)bp == heap_listp) {
        // 프롤로그 블록 바로 뒤의 첫 블록은 이전 블록이 항상 할당된 것으로 간주
        prev_alloc = 1;
    } else {
        prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    }
    // 2. 다음 블록의 할당 여부를 확인 (다음 블록의 헤더에서 alloc 비트 확인)
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    // 3. 현재 블록의 크기를 헤더에서 읽어옴
    size_t size = GET_SIZE(HDRP(bp));

    // Case 1: 이전, 다음 블록 모두 할당된 상태 (병합 불가)
    if (prev_alloc && next_alloc)
    {
        // 병합할 필요가 없으므로 현재 블록 포인터 그대로 반환
        return bp;
    }
    // Case 2: 이전 블록은 할당, 다음 블록만 free (다음 블록과 병합)
    else if (prev_alloc && !next_alloc)
    {
        // 다음 블록의 크기를 더해 새로운 크기 계산
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        // 현재 블록의 헤더에 새로운 크기와 free 상태 기록
        PUT(HDRP(bp), PACK(size, 0));
        // 병합된 블록의 푸터에도 동일하게 기록
        PUT(FTRP(bp), PACK(size, 0));
        // bp는 그대로 (현재 블록이 병합의 시작점)
    }
    // Case 3: 이전 블록만 free, 다음 블록은 할당 (이전 블록과 병합)
    else if (!prev_alloc && next_alloc)
    {
        // 이전 블록의 크기를 더해 새로운 크기 계산
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        // 병합된 블록의 푸터에 새로운 크기와 free 상태 기록
        PUT(FTRP(bp), PACK(size, 0));
        // 이전 블록의 헤더에도 동일하게 기록
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        // bp를 이전 블록의 시작 주소로 이동 (병합된 블록의 시작점)
        bp = PREV_BLKP(bp);
    }
    // Case 4: 이전, 다음 블록 모두 free (이전, 다음 모두와 병합)
    else
    {
        // 이전, 다음 블록의 크기를 모두 더해 새로운 크기 계산
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        // 병합된 블록의 헤더에 새로운 크기와 free 상태 기록
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        // 병합된 블록의 푸터에도 동일하게 기록
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        // bp를 이전 블록의 시작 주소로 이동 (병합된 블록의 시작점)
        bp = PREV_BLKP(bp);
    }
    // 병합(혹은 병합하지 않은) 블록의 시작 주소 반환
    return bp;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
- 최소 `size` 바이트의 할당된 블록 페이로드에 대한 포인터를 반환합니다.
- 전체 할당된 블록은 힙 영역 내에 있어야 하며 다른 할당된 청크와 겹치지 않아야 합니다.
- libc `malloc`과 일관성을 위해 항상 8바이트 정렬된 포인터를 반환해야 합니다.
 */
/*
 * mm_malloc - size 바이트의 메모리 블록을 할당한다.
 *
 * [동작 흐름 및 이유]
 * 1. 요청 크기가 0이면 NULL 반환 (불필요한 요청 무시)
 * 2. 블록 크기를 오버헤드(헤더/푸터)와 정렬 조건을 반영해 조정
 * 3. free 리스트에서 적합한 블록을 탐색
 * 4. 찾으면 해당 블록에 메모리 할당(분할 포함)
 * 5. 없으면 힙을 확장하고 새 블록에 할당
 */
void *mm_malloc(size_t size)
{
    size_t asize;      // 조정된 블록 크기 (오버헤드+정렬 포함)
    size_t extendsize; // fit이 없을 때 힙을 얼마나 확장할지
    char *bp;

    // 1. 요청 크기가 0이면 NULL 반환
    if (size == 0)
        return NULL;

    // 2. 오버헤드(헤더/푸터)와 정렬 조건을 반영해 블록 크기 조정
    if (size <= DSIZE)
        asize = 2 * DSIZE; // 최소 블록 크기(헤더+푸터+페이로드)
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); // 8의 배수로 올림

    // 3. free 리스트에서 적합한 블록 탐색
    if ((bp = find_fit(asize)) != NULL)
    {
        // 4. 찾으면 해당 블록에 메모리 할당(필요시 분할)
        place(bp, asize);
        return bp;
    }

    // 5. 적합한 블록이 없으면 힙을 확장하고 새 블록에 할당
    extendsize = MAX(asize, CHUNKSIZE); // 최소 CHUNKSIZE만큼 확장
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
- `bp`가 가리키는 블록을 해제합니다.
- 반환 값은 없습니다.
- 이 루틴은 `bp` 이전 `mm_malloc` 또는 `mm_realloc` 호출에 의해 반환되었고 아직 해제되지 않은 경우에만 작동이 보장됩니다.
 */
void mm_free(void *bp)
{
    /*
    1. 블록의 크기를 헤더에서 읽어온다.
    2. 헤더와 푸터를 free 상태(alloc=0)로 변경한다.
    3. 인접한 free 블록이 있다면 병합(coalescing)한다.
    */

    // 헤더에서 블록의 크기를 읽어온다.
    size_t size = GET_SIZE(HDRP(bp));

    /*
    왜 필요한가?
    free할 때, 해당 블록의 헤더와 푸터 모두를 free 상태로 바꿔줘야 한다.
    푸터의 위치를 계산하려면 블록의 크기가 필요하다.
    블록의 크기는 헤더에 저장되어 있으므로, GET_SIZE(HDRP(bp))로 읽어온다.
    */

    // 프리할 블록의 헤더를 프리 한다.
    PUT(HDRP(bp), PACK(size, 0));

    // 프리할 블록의 푸터를 프리한다.
    PUT(FTRP(bp), PACK(size, 0));

    // 인접한 free 블록이 있다면 병합(coalescing)한다.
    coalesce(bp);
}

/*
 * mm_realloc - 기존 블록의 크기를 size 바이트로 변경하고, 새 블록의 주소를 반환한다.
 *
 * [동작 흐름 및 이유]
 * 1. 예외 처리: size가 0이면 free, bp가 NULL이면 malloc과 동일하게 동작
 * 2. 기존 블록의 크기를 헤더에서 읽어온다.
 * 3. 새 크기를 mm_malloc과 동일하게 정렬하여 계산한다.
 * 4. 크기가 같으면 기존 포인터 반환(불필요한 복사 방지)
 * 5. 새 블록을 할당하고, 기존 데이터의 최소값만큼 복사
 * 6. 기존 블록을 해제하고 새 블록의 포인터 반환
 */
void *mm_realloc(void *bp, size_t size)
{
    // 1. 예외 처리: size가 0이면 free, bp가 NULL이면 malloc
    if (size == 0)
    {
        mm_free(bp);
        return NULL;
    }
    if (bp == NULL)
    {
        return mm_malloc(size);
    }

    // 2. 기존 블록의 크기를 헤더에서 읽어온다.
    size_t oldsize = GET_SIZE(HDRP(bp));
    size_t asize;

    // 3. 새 크기를 mm_malloc과 동일하게 정렬하여 계산
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    // 4. 크기가 같으면 기존 포인터 반환(불필요한 복사 방지)
    if (asize == oldsize)
        return bp;

    // 5. 새 블록을 할당하고, 기존 데이터의 최소값만큼 복사
    void *new_bp = mm_malloc(size);
    if (new_bp == NULL)
        return NULL;

    // 복사할 크기는 기존 블록의 페이로드와 요청한 크기 중 작은 값
    size_t copySize = oldsize - DSIZE; // 헤더/푸터 제외
    if (size < copySize)
        copySize = size;
    memcpy(new_bp, bp, copySize);

    // 6. 기존 블록 해제
    mm_free(bp);
    return new_bp;
}

/*
 * find_fit - free 리스트에서 asize 이상인 첫 번째 free 블록을 찾는다 (first-fit)
 *
 * [동작 흐름 및 이유]
 * 1. heap_listp부터 힙의 끝(에필로그 헤더)까지 순차적으로 탐색
 * 2. 할당되지 않은 블록이면서 크기가 asize 이상이면 해당 블록 포인터 반환
 * 3. 끝까지 못 찾으면 NULL 반환
 */
static void *find_fit(size_t asize)
{
    void *bp;

    // 1. heap_listp부터 힙의 끝까지 순차적으로 탐색
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        // 2. 할당되지 않은 블록이면서 크기가 asize 이상이면 반환
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    // 3. 끝까지 못 찾으면 NULL 반환
    return NULL;
}




/*
 * place - free 블록 bp에 asize만큼 할당하고, 남는 공간이 충분하면 분할한다.
 *
 * [동작 흐름 및 이유]
 * 1. 현재 블록의 크기를 읽어온다.
 * 2. 남는 공간이 2*DSIZE 이상이면 블록을 분할(앞쪽은 할당, 뒤쪽은 free)
 * 3. 그렇지 않으면 전체 블록을 할당 상태로 표시
 */
static void place(void *bp, size_t asize)
{
    // 1. 현재 블록의 크기를 읽어온다.
    size_t csize = GET_SIZE(HDRP(bp));

    // 2. 남는 공간이 2*DSIZE 이상이면 블록을 분할
    if ((csize - asize) >= (2 * DSIZE)) {
        // 앞쪽 asize만큼 할당
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        // 남은 공간을 새로운 free 블록으로 만듦
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    // 3. 남는 공간이 충분하지 않으면 전체 블록을 할당 상태로 표시
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}