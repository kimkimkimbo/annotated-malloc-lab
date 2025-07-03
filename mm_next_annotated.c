/*
 * mm_next_annotated.c - Next-fit 기반 동적 메모리 할당기 (malloc lab)
 *
 * 이 파일은 next-fit 전략을 사용하는 동적 메모리 할당기 구현입니다.
 * - next-fit: 마지막으로 탐색이 끝난 위치부터 다음 가용 블록을 찾는 방식
 * - 블록은 header/footer를 통해 크기와 할당 여부를 저장
 * - free 시 인접 가용 블록과 coalesce(병합)하여 단편화 최소화
 * - realloc, malloc, free 등 표준 인터페이스 제공
 *
 * 전체 프로그램 흐름:
 * 1. mm_init()에서 heap 초기화 및 prologue/epilogue 블록 생성
 * 2. mm_malloc()에서 요청 크기만큼 가용 블록 탐색(find_fit), 없으면 heap 확장(extend_heap)
 * 3. mm_free()에서 블록 해제 및 인접 가용 블록 병합(coalesce)
 * 4. mm_realloc()에서 크기 조정, 필요시 새 블록 할당 후 데이터 복사
 *
 * 각 함수/매크로/전역변수에 상세 주석을 추가하여 동작 원리와 이유를 설명합니다.
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



// 워드(Word)와 더블워드(Double Word) 크기 (바이트 단위)
#define WSIZE 4              // header/footer의 크기 (4바이트)
#define DSIZE 8              // 더블워드(8바이트) - 최소 블록 단위
#define CHUNKSIZE (1 << 12)  // heap을 한 번에 확장할 기본 크기 (4096바이트)

// 최대값 계산 매크로
#define MAX(x, y) ((x) > (y) ? (x) : (y))

// size와 alloc(할당 여부)를 하나의 워드에 저장 (header/footer)
#define PACK(size, alloc) ((size) | (alloc))

// 포인터 p가 가리키는 워드 값 읽기/쓰기
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// header/footer에서 블록 크기와 할당 여부 추출
#define GET_SIZE(p) (GET(p) & ~0x7)   // 하위 3비트 제외(8의 배수)
#define GET_ALLOC(p) (GET(p) & 0x1)   // 할당 여부(1:할당, 0:가용)

// 블록 포인터(bp)에서 header/footer 위치 계산
#define HDRP(bp) ((char *)(bp) - WSIZE) // bp: payload 시작, header는 그 앞에 위치
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // footer는 payload 뒤쪽

// 다음/이전 블록의 bp 계산
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// 정렬 관련 매크로 (8바이트 단위)
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


// 내부 함수 선언 (static: 파일 내에서만 사용)
static void *coalesce(void *bp);           // 인접 가용 블록 병합
static void *extend_heap(size_t words);     // heap 확장
static void *find_fit(size_t asize);        // next-fit 방식 가용 블록 탐색
static void place(void *bp, size_t asize);  // 블록 할당 및 분할

// 전역 변수
static char *heap_listp = NULL; // heap의 시작(프롤로그 블록의 payload)
static char *last_fitp;         // next-fit 탐색의 시작 위치(마지막 탐색 위치)


// heap 초기화 함수
// - prologue(프롤로그)와 epilogue(에필로그) 블록 생성
// - heap_listp, last_fitp 초기화
// - 최초 heap 확장
int mm_init(void)
{
    // heap에 4워드 공간 할당: [정렬패딩][프롤로그 header][프롤로그 footer][에필로그 header]
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == ((void *)-1))
    {
        return -1; // 메모리 할당 실패
    }

    PUT(heap_listp, 0); // 정렬 패딩 (사용X)
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // 프롤로그 header (8바이트, 할당)
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // 프롤로그 footer (8바이트, 할당)
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     // 에필로그 header (0바이트, 할당)

    heap_listp += (2 * WSIZE); // payload 시작 위치로 이동
    last_fitp = heap_listp;    // next-fit 탐색 시작 위치도 초기화

    // 최초 heap 확장 (CHUNKSIZE만큼 가용 블록 생성)
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }

    return 0;
}


// heap을 words(워드)만큼 확장하여 새 가용 블록 생성
// - 항상 8바이트(더블워드) 정렬 보장
// - 새로 만든 블록을 coalesce로 병합 시도
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    // words가 홀수면 짝수로 맞춰 8바이트 정렬
    size = (words % 2) ? ((words + 1) * WSIZE) : (words * WSIZE);

    // heap 공간 실제 확장 (mem_sbrk)
    if ((long)(bp = mem_sbrk(size)) == -1)
    {
        return NULL; // 확장 실패
    }

    // 새 가용 블록의 header/footer 설정
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    // 새로운 에필로그 블록 header 추가
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // 인접 가용 블록과 병합 시도
    return coalesce(bp);
}


// 인접 가용 블록과 병합(coalesce)
// - free 시 단편화 최소화 목적
// - 4가지 경우: 앞/뒤 모두 할당, 뒤만 가용, 앞만 가용, 앞뒤 모두 가용
// - 병합 후 last_fitp를 병합된 블록으로 이동
void *coalesce(void *bp)
{
    size_t prev_alloc;
    // heap의 첫 블록이면 이전 블록 없음(프롤로그)
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

    // case 1: 앞/뒤 모두 할당 -> 병합X
    if (prev_alloc && next_alloc)
    {
        return bp;
    }
    // case 2: 앞은 할당, 뒤는 가용 -> 뒤와 병합
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // case 3: 앞은 가용, 뒤는 할당 -> 앞과 병합
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // case 4: 앞/뒤 모두 가용 -> 앞/뒤 모두와 병합
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // 병합 후 next-fit 탐색 시작 위치도 병합된 블록으로 이동
    last_fitp = bp;
    return bp;
}


// 메모리 할당 함수
// - 요청 크기(size)를 8바이트 정렬 및 최소 블록 크기로 변환(asize)
// - next-fit 방식으로 가용 블록 탐색(find_fit)
// - 없으면 heap 확장(extend_heap)
// - 블록 할당(place)
void *mm_malloc(size_t size)
{
    size_t asize;       // 정렬 및 최소 크기 반영한 실제 할당 크기
    size_t extendsize;  // heap 확장 필요시 확장 크기
    char *bp;

    if (size == 0)
        return NULL; // 0바이트 요청은 무시

    // 최소 블록 크기(16바이트) 보장 및 8바이트 정렬
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    // next-fit으로 가용 블록 탐색
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize); // 블록 할당 및 분할
        return bp;
    }

    // 가용 블록 없으면 heap 확장
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL; // 확장 실패

    place(bp, asize);
    return bp;
}


// 메모리 해제 함수
// - header/footer를 가용 상태로 변경
// - 인접 가용 블록과 병합(coalesce)
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0)); // header: 가용 표시
    PUT(FTRP(bp), PACK(size, 0)); // footer: 가용 표시

    coalesce(bp); // 인접 가용 블록 병합
}


// realloc: 블록 크기 재조정 함수
// - size==0이면 free와 동일
// - bp==NULL이면 malloc과 동일
// - 크기가 같으면 그대로 반환
// - 새 블록 할당 후 데이터 복사, 기존 블록 free
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

    // 최소 블록 크기 및 정렬 반영
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if (asize == oldsize)
        return bp; // 크기 같으면 그대로 반환

    // 새 블록 할당
    void *new_bp = mm_malloc(size);
    if (new_bp == NULL)
        return NULL;

    // 데이터 복사 (payload만큼)
    size_t copySize = oldsize - DSIZE;
    if (size < copySize)
        copySize = size;
    memcpy(new_bp, bp, copySize);

    mm_free(bp); // 기존 블록 해제
    return new_bp;
}


// next-fit 방식 가용 블록 탐색 함수
// - last_fitp부터 heap 끝까지 탐색
// - 못 찾으면 heap_listp부터 last_fitp 직전까지 다시 탐색
// - 조건: 가용 && 크기 충분
static void *find_fit(size_t asize)
{
    char *p = last_fitp;

    // 1. last_fitp부터 heap 끝(에필로그)까지 탐색
    while (GET_SIZE(HDRP(p)) > 0)
    {
        // 블록이 가용 && 크기 충분하면 반환
        if (!GET_ALLOC(HDRP(p)) && GET_SIZE(HDRP(p)) >= asize)
        {
            last_fitp = p; // 다음 탐색 시작 위치 갱신
            return p;
        }
        p = NEXT_BLKP(p);
    }

    // 2. heap_listp부터 last_fitp 직전까지 탐색 (원형)
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

    // 가용 블록 없음
    return NULL;
}


// 블록 할당 및 분할 함수
// - 남는 공간이 충분(최소 블록 크기 이상)하면 분할
// - 아니면 전체 블록 할당
// - 마지막으로 할당한 위치를 last_fitp로 갱신
static void place(void *bp, size_t asize)
{
    if (bp == NULL || GET_SIZE(HDRP(bp)) == 0)
    {
        return; // 잘못된 포인터/빈 블록 방지
    }

    size_t csize = GET_SIZE(HDRP(bp)); // 현재 블록 크기

    // 남는 공간이 충분하면(최소 블록 크기 이상) 분할
    if ((csize - asize) >= (2 * DSIZE))
    {
        // 앞부분 asize만큼 할당
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp); // 남은 부분으로 bp 이동
        // 남은 부분은 가용 블록으로 만듦
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else
    {
        // 남는 공간이 적으면 전체 블록 할당
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }

    // 마지막으로 할당한 위치를 next-fit 탐색 시작점으로
    last_fitp = bp;
}