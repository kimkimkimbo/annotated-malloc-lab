# malloc-lab: 동적 메모리 할당기 구현 및 주석 해설

## 프로젝트 개요
- **목표:** C로 동적 메모리 할당기(First-fit/Next-fit 등)를 직접 구현
- **기능:** malloc, free, realloc을 직접 구현하여 메모리 효율성과 안정성, 성능을 모두 고려

### 자세한 설명
first: first_annotated.md
next: next_annotated.md


## 블록 구조 및 매크로 설명
- **블록:** [헤더][페이로드][푸터] 구조
    - 헤더/푸터: 블록 크기와 할당 여부 저장
    - 페이로드: 사용자 데이터 영역
- **프롤로그/에필로그 블록:**
    - 프롤로그: 힙 앞 경계, 헤더/푸터 모두 생성(병합 안전성)
    - 에필로그: 힙 끝 경계, 헤더만 생성(힙 끝 표시)
- **주요 매크로:**
    - `PACK(size, alloc)`: 크기와 할당 비트를 합쳐 저장
    - `GET/PUT(p, val)`: 헤더/푸터 읽기/쓰기
    - `GET_SIZE/GET_ALLOC(p)`: 크기/할당 여부 추출
    - `HDRP/FTRP(bp)`: 헤더/푸터 주소 계산
    - `NEXT_BLKP/PREV_BLKP(bp)`: 인접 블록 탐색
    - `ALIGN(size)`: 8바이트 정렬

## mm 함수별 동작 흐름 및 설계 의도

### mm_init
- 힙을 초기화하고 프롤로그/에필로그 블록 생성
- 힙을 기본 크기(CHUNKSIZE)만큼 확장

### mm_malloc
- 요청 크기를 헤더/푸터/정렬까지 반영해 asize로 변환
- free 리스트에서 적합한 블록 탐색(find_fit)
- 찾으면 place로 할당, 없으면 힙 확장 후 할당

### mm_free
- 헤더/푸터를 free 상태로 변경
- 인접 free 블록이 있으면 coalesce로 병합

### mm_realloc
- 크기가 0이면 free, bp==NULL이면 malloc
- 크기가 같으면 기존 포인터 반환
- 새 블록 할당, 데이터 복사, 기존 블록 free

### coalesce
- 인접 free 블록과 병합(4가지 경우)
- 병합된 블록의 시작 주소 반환

### place
- 블록 분할: 남는 공간이 2*DSIZE 이상이면 분할
- 아니면 전체 블록 할당

### 최적화/안정성 포인트
- 8바이트 정렬, 최소 블록 크기 강제
- 예외 상황(잘못된 포인터, 크기 부족 등) 방어 코드
- 파편화 최소화: free 시 즉시 병합

### 개선 아이디어/한계점
- 명시적 free list, 세그리게이티드 리스트 등으로 성능 향상 가능
- 현재는 first-fit 기반, next-fit/explicit list로 확장 가능

---
## mm_first, mm_next

### First-Fit
- 항상 heap_listp부터 힙의 끝까지 순차 탐색.
- 처음으로 조건에 맞는 블록을 발견하면 그 즉시 반환.
- 단순하지만, 초반 free 블록이 자주 사용되며 앞부분 단편화 심해진다.

### Next-Fit
- 탐색을 항상 힙의 처음부터 시작하지 않는다.
- 대신, "마지막으로 할당에 성공했던 위치 이후"부터 탐색 시작.
- 힙 끝까지 못 찾으면, 다시 힙 처음부터 last_fitp 직전까지 탐색.
- 탐색 범위가 줄어들 수 있지만, 패턴에 따라 성능이 더 나빠질 수도 있다.



### 실행할 파일명을 mm.c로 수정.

실행 순서

make clean && make

m1에선 gdb를 사용할 수 없어서 lldb 사용

lldb ./mdriver

(lldb) run

analysis_v.txt 파일 내용
./mdriver -v

#####################################################################
# CS:APP Malloc Lab
# Handout files for students
#
# Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
# May not be used, modified, or copied without permission.
#
######################################################################

***********
Main Files:
***********

mm.{c,h}	
	Your solution malloc package. mm.c is the file that you
	will be handing in, and is the only file you should modify.

mdriver.c	
	The malloc driver that tests your mm.c file

short{1,2}-bal.rep
	Two tiny tracefiles to help you get started. 

Makefile	
	Builds the driver

**********************************
Other support files for the driver
**********************************

config.h	Configures the malloc lab driver
fsecs.{c,h}	Wrapper function for the different timer packages
clock.{c,h}	Routines for accessing the Pentium and Alpha cycle counters
fcyc.{c,h}	Timer functions based on cycle counters
ftimer.{c,h}	Timer functions based on interval timers and gettimeofday()
memlib.{c,h}	Models the heap and sbrk function

*******************************
Building and running the driver
*******************************
To build the driver, type "make" to the shell.

To run the driver on a tiny test trace:

	unix> mdriver -V -f short1-bal.rep

The -V option prints out helpful tracing and summary information.

To get a list of the driver flags:

	unix> mdriver -h

