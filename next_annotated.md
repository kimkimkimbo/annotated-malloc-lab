# 단순한 Malloc 구현 (Simple Malloc Implementation)

가장 빠르지만 메모리 효율은 최악인 malloc 구현입니다. 이 단순한 방식에서는 `brk` 포인터를 증가시키기만 하면 블록을 할당합니다.


---

## 상수 정의

```c
#define WSIZE 4         // 워드(4바이트) 크기, 헤더/푸터의 크기
#define DSIZE 8         // 더블 워드(8바이트) 크기, 블록 최소 단위 및 정렬 기준
#define CHUNKSIZE (1 << 12)  // 힙을 한 번에 확장할 기본 크기(4096바이트)
#define ALIGNMENT 8     // 8바이트 정렬
```

---

## 핵심 매크로 함수

### 유틸리티 매크로
```c
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))  // 크기와 할당 비트를 합침
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)  // 8의 배수로 정렬
```

### 메모리 접근 매크로
```c
#define GET(p) (*(unsigned int *)(p))          // p에서 4바이트 값 읽기
#define PUT(p, val) (*(unsigned int *)(p) = (val))  // p에 4바이트 값 저장
```

### 블록 정보 추출 매크로
```c
#define GET_SIZE(p) (GET(p) & ~0x7)   // 헤더/푸터에서 블록 크기 추출
#define GET_ALLOC(p) (GET(p) & 0x1)   // 헤더/푸터에서 할당 비트 추출
```

### 블록 포인터 계산 매크로
```c
#define HDRP(bp) ((char *)(bp) - WSIZE)  // 블록의 헤더 주소
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)  // 블록의 푸터 주소
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))  // 다음 블록
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))  // 이전 블록
```

---

## 전역 변수 및 함수 선언

```c
static char *heap_listp = NULL;  // 힙 리스트 포인터

// 정적 함수 선언
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
```

---

## 주요 함수 구현

### 1. `mm_init()` - 힙 초기화

```c
int mm_init(void)
{
    // 힙에 16바이트(4워드) 공간 요청
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == ((void *)-1)) {
        return -1;
    }

    PUT(heap_listp, 0);                          // 패딩 (정렬용)
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));  // 프롤로그 헤더
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));  // 프롤로그 푸터
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));      // 에필로그 헤더
    
    heap_listp += (2 * WSIZE);  // 프롤로그 블록의 페이로드로 이동

    // 힙을 CHUNKSIZE만큼 확장
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }
    return 0;
}
```

**힙 초기 구조:**
```
[패딩] [프롤로그헤더] [프롤로그푸터] [에필로그헤더]
  0        8|1          8|1         0|1
```

### 2. `extend_heap()` - 힙 확장

```c
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    // 8바이트 정렬을 위해 짝수 개의 워드로 조정
    size = (words % 2) ? ((words + 1) * WSIZE) : (words * WSIZE);

    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    // 새 free 블록의 헤더와 푸터 설정
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    
    // 새로운 에필로그 헤더 생성
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // 인접 free 블록과 병합
    return coalesce(bp);
}
```

### 3. `coalesce()` - 블록 병합

```c
void *coalesce(void *bp)
{
    // 이전/다음 블록의 할당 상태 확인
    size_t prev_alloc = ((char*)bp == heap_listp) ? 1 : GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // Case 1: 이전, 다음 모두 할당됨 - 병합 불가
    if (prev_alloc && next_alloc) {
        return bp;
    }
    // Case 2: 다음 블록만 free - 다음과 병합
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // Case 3: 이전 블록만 free - 이전과 병합
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // Case 4: 이전, 다음 모두 free - 모두 병합
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    //병합이 되고나면 병합된 블록 헤더를 가리키도록 변경해 줘야 함.
    last_fitp = bp;
    return bp;
}
```

### 4. `mm_malloc()` - 메모리 할당

```c
void *mm_malloc(size_t size)
{
    size_t asize;      // 조정된 블록 크기
    size_t extendsize; // 힙 확장 크기
    char *bp;

    if (size == 0) return NULL;

    // 오버헤드와 정렬 조건을 반영해 크기 조정
    if (size <= DSIZE) {
        asize = 2 * DSIZE;  // 최소 블록 크기
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    // 적합한 free 블록 탐색
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // 적합한 블록이 없으면 힙 확장
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    return bp;
}
```

### 5. `mm_free()` - 메모리 해제

```c
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    // 헤더와 푸터를 free 상태로 변경
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    // 인접한 free 블록과 병합
    coalesce(bp);
}
```

### 6. `mm_realloc()` - 메모리 재할당

```c
void *mm_realloc(void *bp, size_t size)
{
    if (size == 0) {
        mm_free(bp);
        return NULL;
    }
    if (bp == NULL) {
        return mm_malloc(size);
    }

    size_t oldsize = GET_SIZE(HDRP(bp));
    size_t asize;

    // 새 크기 계산
    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    // 크기가 같으면 기존 포인터 반환
    if (asize == oldsize) return bp;

    // 새 블록 할당 및 데이터 복사
    void *new_bp = mm_malloc(size);
    if (new_bp == NULL) return NULL;

    size_t copySize = oldsize - DSIZE;  // 헤더/푸터 제외
    if (size < copySize) copySize = size;
    memcpy(new_bp, bp, copySize);

    mm_free(bp);
    return new_bp;
}
```

---

## 보조 함수

### `find_fit()` - Next-Fit 탐색

```c
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
```

### `place()` - 블록 배치 및 분할

```c
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    // 남는 공간이 충분하면 블록 분할
    if ((csize - asize) >= (2 * DSIZE)) {
        // 앞쪽은 할당
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        // 뒤쪽은 새로운 free 블록으로
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    } else {
        // 전체 블록을 할당 상태로
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
    last_fitp = bp;
}
```

---

좋습니다. 주신 구조 설명과 특징들을 **Next Fit** 전략을 기준으로 다시 작성해드리겠습니다. First Fit과는 전략 자체가 다르므로, 일부 평가 기준도 달라져야 합니다. 특히 "빠른 할당" 같은 장점은 애매해질 수 있고, 탐색 속도는 최적화 여부에 따라 달라지므로 회의적인 시각도 포함하겠습니다.

---

## 블록 구조 (Next Fit 기반 구현)

### 할당된 블록

```
[헤더: size|1] [페이로드...] [푸터: size|1]
```

### Free 블록

```
[헤더: size|0] [미사용 공간...] [푸터: size|0]
```

### 힙 전체 구조

```
[패딩] [프롤로그] [블록1] [블록2] ... [블록n] [에필로그]
  0      8|1     [data]  [data]      [data]    0|1
```

---

## 주요 특징 및 제약사항 (Next Fit 기반 구현)

### 장점

* **탐색 시간 단축 가능성**
  `last_fitp`를 기준으로 탐색을 시작하기 때문에, 항상 처음부터 훑지 않아도 되어 평균적인 탐색 시간이 짧아질 가능성이 있음.
  (단, 이는 메모리 사용 패턴이 일정할 때만 해당됨)

* **단순한 상태 유지**
  별도의 free 리스트 없이 단일 힙 구조와 포인터 하나(`last_fitp`)만으로 구현할 수 있음.

* **병합 지원**
  `coalesce` 함수를 통해 인접한 free 블록 간 병합을 수행, 어느 정도 단편화 해소 가능.

### 단점

* **성능 일관성 부족**
  할당 패턴에 따라 `last_fitp`가 항상 힙 끝 근처를 가리키게 되면, 오히려 First Fit보다 탐색 범위가 커지는 역효과 발생 가능.

* **불균형한 free 공간 분포**
  탐색이 힙의 후반부에 집중되므로, 앞쪽의 작은 free 블록들이 활용되지 않고 방치되어 단편화 유발 가능성 있음.

* **공간 낭비**
  헤더/푸터 오버헤드가 여전히 존재하며, 최소 블록 크기도 DSIZE 이상으로 고정되어 있음.

* **정교한 최적화 없음**
  Next Fit은 기본적으로 단순한 전략이기 때문에, segregated free list 같은 구조에 비해 고급 최적화가 어렵고 성능이 상황에 민감함.