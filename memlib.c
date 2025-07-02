/*
 * memlib.c - a module that simulates the memory system.  Needed because it 
 *            allows us to interleave calls from the student's malloc package 
 *            with the system's malloc package in libc.
 *            메모리 시스템을 시뮬레이션하는 모듈입니다. 
*             학생의 malloc 패키지 호출을 libc에 있는 시스템의 malloc 패키지와 인터리빙할 수 있기 때문에 필요합니다.
*
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include "memlib.h"
#include "config.h"

/* private variables */
static char *mem_start_brk;  /* points to first byte of heap */
static char *mem_brk;        /* points to last byte of heap */
static char *mem_max_addr;   /* largest legal heap address */ 

/* 
 * mem_init - initialize the memory system model
    메모리 시스템 모델을 초기화하는 함수 즉, 힙 영역 초기화
 */
void mem_init(void)
{
    /* allocate the storage we will use to model the available VM 
        사용 가능한 VM을 모델링하는 데 사용할 저장소를 할당합니다.
    */

    //예외처리 / 할당했는데 NULL이면 종료
    if ((mem_start_brk = (char *)malloc(MAX_HEAP)) == NULL) { //
	fprintf(stderr, "mem_init_vm: malloc error\n");
	exit(1);
    }

    //최대 주소니까 시작 주소 + 최대 크기
    mem_max_addr = mem_start_brk + MAX_HEAP;  /* max legal heap address */
    //초기화 하는 거니까 brk가 시작 주소랑 같음
    mem_brk = mem_start_brk;                  /* heap is empty initially */
}

/* 
 * mem_deinit - free the storage used by the memory system model
    메모리 시스템 모델에서 사용하는 저장소를 비우세요
 */
void mem_deinit(void)
{
    free(mem_start_brk);
}

/*
 * mem_reset_brk - reset the simulated brk pointer to make an empty heap
    시뮬레이션된 brk 포인터를 재설정하여 빈 힙을 만듭니다.
 */
void mem_reset_brk()
{
    mem_brk = mem_start_brk;
}

/* 
 * mem_sbrk - simple model of the sbrk function. Extends the heap 
 *    by incr bytes and returns the start address of the new area. In
 *    this model, the heap cannot be shrunk.
 *    sbrk 함수의 간단한 모델입니다. 힙을 incr 바이트만큼 확장하고 새 영역의 시작 주소를 반환합니다.
 *    이 모델에서는 힙을 축소할 수 없습니다.
 */
void *mem_sbrk(int incr) 
{
    //확장하기 전에 brk 값 저장!
    char *old_brk = mem_brk;

    //예외처리
    //(incr < 0) 음수 or  mem_max_addr 최대 크기를 초과하면
    if ( (incr < 0) || ((mem_brk + incr) > mem_max_addr)) {
	errno = ENOMEM; //에러 변수에 담고 에러 메시지 출력
	fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory...\n");

    //확장에 실패했으므로 -1 리턴!
	return (void *)-1;
    }

    //brk에 확장하는 값 추가
    mem_brk += incr;
    //예전 brk 리턴하기 왜?
    //사용한 게 아니라 늘리기만 한 거라 마지막으로 사용한 brk 리턴
    return (void *)old_brk;
}

/*
 * mem_heap_lo - return address of the first heap byte
    첫 번째 힙 바이트의 반환 주소

    가용 영역을 넘으면 안 되니까 예외 방지 처리 의미
 */
void *mem_heap_lo()
{
    return (void *)mem_start_brk;
}

/* 
 * mem_heap_hi - return address of last heap byte
    마지막 힙 바이트의 반환 주소
    
    가용 영역을 넘으면 안 되니까 예외 방지 처리 의미
 */
void *mem_heap_hi()
{
    return (void *)(mem_brk - 1);
}

/*
 * mem_heapsize() - returns the heap size in bytes
    힙 크기를 바이트 단위로 반환합니다.
 */
size_t mem_heapsize() 
{
    return (size_t)(mem_brk - mem_start_brk);
}

/*
 * mem_pagesize() - returns the page size of the system
    시스템의 페이지 크기를 반환합니다
 */
size_t mem_pagesize()
{
    return (size_t)getpagesize();
}
