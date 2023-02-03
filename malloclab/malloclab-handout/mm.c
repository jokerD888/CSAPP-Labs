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
    "ateam",
    /* First member's full name */
    "MingHui Lv",
    /* First member's email address */
    "joker868@126.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/*
* We use explitict segregated free lists with rounding to upper power of 2 as the class equivalence condition
* Blocks within each class are sorted based on size in descending order
*
* Format of allocated block and free block are shown below

///////////////////////////////// Block information /////////////////////////////////////////////////////////
/*

A   : Allocated? (1: true, 0:false)

< Allocated Block >

         31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
bp --->     +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
Header :   |                              size of the block                                       |  |  | A|
                    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
        |                                                                                               |
        |                                                                                               |
        .                              Payload . . . . .
        +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
Footer :   |                              size of the block                                       |     | A|
        +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

< Free block >

         31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
bp --->    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
Header :   |                              size of the block                                       |  |  | A|
bp+4 --->  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
        |                        pointer to its predecessor in Segregated list                          |
                    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
        |                        pointer to its successor in Segregated list                            |
        +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
        .                                                                                               .
        .                                                                                               .
        .                                                                                               .
        +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
Footer :   |                              size of the block                                       |     | A|
        +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

///////////////////////////////// End of Block information /////////////////////////////////////////////////////////

*
*/

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4              // 字和头部/脚部的大小（bytes)
#define DSIZE 8              // 双字
#define CHUNKSIZE (1 << 12)  // 按照CHUNKSIZE大小（bytes)扩展堆

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

// #define IMPLICIT
// #define EXPLICIT
#define SEGREGATED

// IMPLICIT 隐式空闲链表--------------------------------
// 分配策略：默认是首次分配，通过定义以下宏，可以选择其他策略
// 下一次适配
// #define NEXT_FIT
//  最佳适配
// #define BEST_FIT

// EXPLICIT 显式空闲链表----------------------------------
// 链表维护策略：默认是按地址顺序维护
// 后进先出 LIFO
// #define LIFO
//

// SEGREGATED 分离适配-----------------------------------
// 链表维护策略：默认是按块大小排序
// 后进先出
// #define LIFO


// 常数及宏 begin-----------------------------------------

#ifndef SEGREGATED

#ifdef EXPLICIT
#define NEXT_LINK_RP(bp) ((char *)(bp))
#define PREV_LINK_RP(bp) ((char *)(bp) + WSIZE)
static char *list_head;  // 表头
#endif

#ifdef NEXT_FIT
static char *prev_bp = NULL;
#endif

// 常数及宏 end-----------------------------------------

#ifdef EXPLICIT
// 链表插入
static void _insert(char *bp) {
    // 指针先初始化为空
    PUT(NEXT_LINK_RP(bp), 0);
    PUT(PREV_LINK_RP(bp), 0);
#ifdef LIFO
    char *t_head = GET(list_head);
    if (t_head) {  // 链表不为空
        PUT(PREV_LINK_RP(t_head), bp);
    }
    PUT(NEXT_LINK_RP(bp), t_head);
    PUT(list_head, bp);
#else
    char *cur = GET(list_head);
    if (!cur) {  // 链表为空
        PUT(NEXT_LINK_RP(bp), cur);
        PUT(PREV_LINK_RP(bp), list_head);
        PUT(list_head, bp);
        return;
    }

    // 循环条件为下一个节点不为空，且当前节点地址小于bp
    while (cur < bp && GET(NEXT_LINK_RP(cur))) {
        cur = GET(NEXT_LINK_RP(cur));
    }

    if (cur >= bp) {                          // 要将bp节点插入到cur之前
        char *prev = GET(PREV_LINK_RP(cur));  // 注意：PREV_LINK_RP获取的是指针的地址，GET后才是指针的值
        PUT(NEXT_LINK_RP(bp), cur);
        PUT(PREV_LINK_RP(bp), prev);
        PUT(NEXT_LINK_RP(prev), bp);
        PUT(PREV_LINK_RP(cur), bp);
    } else {  // 没有后续节点了，尾插
        PUT(NEXT_LINK_RP(cur), bp);
        PUT(PREV_LINK_RP(bp), cur);
    }
#endif
}
// 链表移除节点
static void _remove(char *bp) {
    char *next = GET(NEXT_LINK_RP(bp));
    char *prev = GET(PREV_LINK_RP(bp));
    if (!prev) {  // 头节点
        if (next) PUT(PREV_LINK_RP(next), 0);
        PUT(list_head, next);
    } else {  // 非头
        if (next) PUT(PREV_LINK_RP(next), prev);
        PUT(NEXT_LINK_RP(prev), next);
    }
}
#endif

// 私有全局变量及函数

static char *heap_listp;
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    // 为对齐，分配偶数个字
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) return NULL;

    // 初始化空闲块头部脚部以及结尾块
    PUT(HDRP(bp), PACK(size, 0));          // 空闲块头部
    PUT(FTRP(bp), PACK(size, 0));          // 空闲块脚部
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  // 新的结尾块

    // 合并空闲块
    return coalesce(bp);
}

static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));  // 通过前一个块的脚部获取分配状态
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));  // 通过后一个块的头部获取分配状态
    size_t size = GET_SIZE(HDRP(bp));
#ifdef IMPLICIT
    // 都已分配
    if (prev_alloc && next_alloc) {
        return bp;
    } else if (prev_alloc && !next_alloc) {     // 下一个块为空闲
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));  // 加上下一个块的size
        PUT(HDRP(bp), PACK(size, 0));           // 修改bp块的头部
        PUT(FTRP(bp), PACK(size, 0));           // 修改“下一块”的脚部
        // 需要注意的是：FTRP是通过HDRP运作的，所以要注意两者的先后关系
    } else if (!prev_alloc && next_alloc) {  // 上一个块为空闲
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else {  // 上下两个块皆为空闲
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
#ifdef NEXT_FIT
    prev_bp = bp;
#endif
    return bp;
#elif defined(EXPLICIT)
    if (prev_alloc && !next_alloc) {            // 下一个块为空闲
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));  // 加上下一个块的size
        _remove(NEXT_BLKP(bp));                 // 先从空闲链表删除下一个块
        PUT(HDRP(bp), PACK(size, 0));           // 修改bp块的头部
        PUT(FTRP(bp), PACK(size, 0));           // 修改“下一块”的脚部
        // 需要注意的是：FTRP是通过HDRP运作的，所以要注意两者的先后关系
    } else if (!prev_alloc && next_alloc) {  // 上一个块为空闲
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        _remove(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else if (!prev_alloc && !next_alloc) {  // 上下两个块皆为空闲
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        _remove(PREV_BLKP(bp));
        _remove(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    _insert(bp);  // 插入合并后的块，若不需要合并，直接插入

    return bp;
#endif
}
static void *find_fit(size_t asize) {
#ifdef IMPLICIT
#ifdef NEXT_FIT
    int size = 0;
    for (; (size = GET_SIZE(HDRP(prev_bp))) > 0; prev_bp = NEXT_BLKP(prev_bp)) {
        if (!GET_ALLOC(HDRP(prev_bp)) && (asize <= GET_SIZE(HDRP(prev_bp)))) {
            return prev_bp;
        }
    }
    return NULL;

#elif defined(BEST_FIT)
    void *best_bp = NULL;
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            if (best_bp == NULL) best_bp = bp;
            if (GET_SIZE(HDRP(best_bp)) > GET_SIZE(HDRP(bp))) best_bp = bp;
            if (GET_SIZE(HDRP(best_bp)) == asize) break;
        }
    }
    return best_bp;
#else
    // 首次适配算法,从头开始搜索，选择第一个合适的空闲块
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) return bp;
    }
    return NULL;

#endif
#elif defined(EXPLICIT)
    void *bp = GET(list_head);
    while (bp) {
        if (GET_SIZE(HDRP(bp)) >= asize) return bp;
        bp = GET(NEXT_LINK_RP(bp));
    }
    return NULL;
#endif
}
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));
#ifdef EXPLICIT
    _remove(bp);  // 先从空闲链表删除bp块
#endif
    // 当分割剩下的块大小 >= 最小块大小（2*DSIZE）时才进程分割
    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));  // 需要注意的是：FTRP是通过HDRP运作的，所有要注意两者的先后关系
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        coalesce(bp);  // 将多余的空闲块合并
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
#ifdef IMPLICIT
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) return -1;
    PUT(heap_listp, 0);  // 第一个字是双字边界对齐的不使用填充字
    // 序言块是一个8字节的已分配块，只由一个头部和脚部组成，序言块和结尾块允许我们忽略潜在的麻烦边界问题
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));  // 序言块
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));  // 序言块
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));      // 结尾块
    heap_listp += (2 * WSIZE);                      // 指向序言快的下一个字节
#ifdef NEXT_FIT
    prev_bp = heap_listp;
#endif

#elif defined(EXPLICIT)
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *)-1) return -1;
    PUT(heap_listp, 0);  // 第一个字是双字边界对齐的不使用填充字
    // 哨兵节点
    PUT(heap_listp + (1 * WSIZE), 0);  // next
    PUT(heap_listp + (2 * WSIZE), 0);  // prev
    // 序言块是一个8字节的已分配块，只由一个头部和脚部组成，序言块和结尾块允许我们忽略潜在的麻烦边界问题
    PUT(heap_listp + (3 * WSIZE), PACK(DSIZE, 1));  // 序言块
    PUT(heap_listp + (4 * WSIZE), PACK(DSIZE, 1));  // 序言块
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));      // 结尾块

    list_head = heap_listp + (1 * WSIZE);
    heap_listp += (4 * WSIZE);  // 指向序言快的下一个字节
#endif
    // 以空闲块扩展空堆
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) return -1;

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
// 返回有效载荷的开始处
void *mm_malloc(size_t size) {
    size_t asize;       // 调整后的块的大小
    size_t extendsize;  // 扩展堆的大小
    char *bp;

    if (size == 0) return NULL;

    // 调整块大小，包括头部和脚部的开销以及对齐要求
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else  // 向上取整到8的倍数，(DSIZE)是头部和脚部的开销
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    // 如果找到了合适的空闲块，分配
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // 没找到，扩展堆，再分配
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    // size==0,只需要进行释放
    if (size == 0) {
        mm_free(oldptr);
        return NULL;
    }
    // 如果ptr为空，只需要调用mm_malloc即可
    if (oldptr == NULL) {
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;

    size = GET_SIZE(HDRP(oldptr));         // 获取原块的大小
    copySize = GET_SIZE(HDRP(newptr));     // 获取新块的大小
    if (size < copySize) copySize = size;  // 截断

    memcpy(newptr, oldptr, copySize - DSIZE);  //-DSIZE 是减去头部脚部的开销
    mm_free(oldptr);
    return newptr;
}

#else

// 链表操作 bp->next bp->prev
#define NEXT_LINK_RP(bp) ((char *)(bp))
#define PREV_LINK_RP(bp) ((char *)(bp) + WSIZE)

static char *heap_listp;
static char *block_list_start;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void *place(void *bp, size_t asize);

static void _remove(char *bp);
static void _insert(char *bp);
static char *find_list(size_t size); /** 查找该大小所在的链表 **/

#define SIZE_0 16
#define SIZE_1 32
#define SIZE_2 64
#define SIZE_3 128
#define SIZE_4 256
#define SIZE_5 512
#define SIZE_6 1024
#define SIZE_7 2048
#define SIZE_8 4096
#define SIZE_9 8192
// 共有11个大小类，最后一个是>8192的统一划分
#define SIZE_CLASS_CNT 11

int mm_init(void) {
    if ((heap_listp = mem_sbrk(14 * WSIZE)) == (void *)-1) return -1;
    // PUT(heap_listp, 0);  // 第一个字是双字边界对齐的不使用填充字
    // 大小类的块大小划分最多到2^13=8192，因为测试文件测试的大小大多不超过8192，除了一些特殊的测试文件，统一划分到一个链表
    // 2^4  2^5 2^6 ... 2^13 >8192,由于空闲块的设计以及对齐要求，最小的块要求为16字节，所以从2^4次方开始

    for (int i = 0; i < 11; ++i) {
        PUT(heap_listp + (i * WSIZE), 0);
    }

    PUT(heap_listp + (11 * WSIZE), PACK(DSIZE, 1));  // 序言头
    PUT(heap_listp + (12 * WSIZE), PACK(DSIZE, 1));  // 序言尾
    PUT(heap_listp + (13 * WSIZE), PACK(0, 1));      // 结尾块

    block_list_start = heap_listp;
    heap_listp += (12 * WSIZE);

    if (extend_heap((CHUNKSIZE + 8) / WSIZE) == NULL) return -1;
    return 0;
}

void *mm_malloc(size_t size) {
    size_t asize;       // 调整后的块的大小
    size_t extendsize;  // 扩展堆的大小
    char *bp;

    if (size == 0) return NULL;

    // 调整块大小，包括头部和脚部的开销以及对齐要求
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else  // 向上取整到8的倍数，(DSIZE)是头部和脚部的开销
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    // 如果找到了合适的空闲块，分配
    if ((bp = find_fit(asize)) != NULL) {
        return place(bp, asize);
    }

    // 没找到，扩展堆，再分配
    extendsize = MAX(asize, CHUNKSIZE);

    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) return NULL;
    return place(bp, asize);
}

void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}
void *mm_realloc(void *ptr, size_t size) {
    void *newptr = ptr;
    size_t copySize, asize, total_size;

    // size==0,只需要进行释放
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    // 如果ptr为空，只需要调用mm_malloc即可
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE + 1)) / DSIZE);

    ssize_t old_size = GET_SIZE(HDRP(ptr));
    ssize_t next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));
    ssize_t prev_size = GET_SIZE(HDRP(PREV_BLKP(ptr)));
    // 原先size大小满足要求，直接返回原指针
    if (old_size >= asize)
        return ptr;
    else if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) && old_size + next_size >= asize) {
        total_size = old_size + next_size;
        _remove(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(total_size, 1));
        PUT(FTRP(ptr), PACK(total_size, 1));
    } else if (!next_size) {
        if (extend_heap(MAX(asize - old_size, CHUNKSIZE)) == NULL) return NULL;
        total_size = old_size + GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        _remove(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(total_size, 1));
        PUT(FTRP(ptr), PACK(total_size, 1));
    } else if (!GET_ALLOC(HDRP(PREV_BLKP(ptr))) && old_size + prev_size >= asize) {
        total_size = old_size + prev_size;
        _remove(PREV_BLKP(ptr));
        PUT(FTRP(ptr), PACK(total_size, 1));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(total_size, 1));
        newptr = PREV_BLKP(ptr);
        memmove(newptr, ptr, old_size);  // 有可能出现重叠区域要用memmove来代替memcpy
    } else {
        newptr = mm_malloc(asize);
        memcpy(newptr, ptr, old_size - DSIZE);  //-DSIZE 是减去头部脚部的开销
        mm_free(ptr);
    }

    return newptr;
}

static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    // 为对齐，分配偶数个字
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1) return NULL;

    // 初始化空闲块头部脚部以及结尾块
    PUT(HDRP(bp), PACK(size, 0));          // 空闲块头部
    PUT(FTRP(bp), PACK(size, 0));          // 空闲块脚部
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  // 新的结尾块
    // 合并空闲块
    return coalesce(bp);
}

static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));  // 通过前一个块的脚部获取分配状态
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));  // 通过后一个块的头部获取分配状态
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && !next_alloc) {            // 下一个块为空闲
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));  // 加上下一个块的size
        _remove(NEXT_BLKP(bp));                 // 先从空闲链表删除下一个块
        PUT(HDRP(bp), PACK(size, 0));           // 修改bp块的头部
        PUT(FTRP(bp), PACK(size, 0));           // 修改“下一块”的脚部
        // 需要注意的是：FTRP是通过HDRP运作的，所以要注意两者的先后关系
    } else if (!prev_alloc && next_alloc) {  // 上一个块为空闲
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        _remove(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else if (!prev_alloc && !next_alloc) {  // 上下两个块皆为空闲
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        _remove(PREV_BLKP(bp));
        _remove(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    _insert(bp);  // 插入合并后的块，若不需要合并，直接插入

    return bp;
}

static void *find_fit(size_t asize) {
    char *head = find_list(asize);
    // 如果在head的空闲链表找不到，就搜索下一个更大的大小类的空闲链表
    for (; head < block_list_start + (SIZE_CLASS_CNT * WSIZE); head += WSIZE) {
        char *bp = GET(head);
        while (bp) {
            if (GET_SIZE(HDRP(bp)) >= asize) return bp;
            bp = GET(NEXT_LINK_RP(bp));
        }
    }
    return NULL;
}
static void *place(void *bp, size_t asize) {
    void *res_bp = bp;
    size_t csize = GET_SIZE(HDRP(bp));
    size_t remain_size = csize - asize;
    _remove(bp);  // 先从空闲链表删除bp块

    if (remain_size < (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    } else if (asize < 88) {
        PUT(HDRP(bp), PACK(remain_size, 0));
        PUT(FTRP(bp), PACK(remain_size, 0));
        _insert(bp);
        bp = NEXT_BLKP(bp);
        res_bp = bp;
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
    } else {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));  // 需要注意的是：FTRP是通过HDRP运作的，所有要注意两者的先后关系
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(remain_size, 0));
        PUT(FTRP(bp), PACK(remain_size, 0));
        _insert(bp);
    }
    return res_bp;
}
// debug辅助函数，检查链表是否按照大小排序，且返回链表节点个数
static int check_list(char *bp) {
    int size = 0;
    int cnt = 0;
    while (bp && GET_SIZE(HDRP(bp)) >= size) {
        size = GET_SIZE(HDRP(bp));
        bp = GET(NEXT_LINK_RP(bp));
        ++cnt;
    }

    return cnt;
}
// 链表插入
static void _insert(char *bp) {
    // 指针先初始化为空
    PUT(NEXT_LINK_RP(bp), 0);
    PUT(PREV_LINK_RP(bp), 0);
#ifdef LIFO
    char *head_ptr = find_list(GET_SIZE(HDRP(bp)));  // 头节点指针的地址
    char *head = GET(head_ptr);                      // 头节点地址

    if (head) {
        PUT(PREV_LINK_RP(head), bp);
    }
    PUT(NEXT_LINK_RP(bp), head);
    PUT(PREV_LINK_RP(bp), head_ptr);
    PUT(head_ptr, bp);
#else
    int bp_size = GET_SIZE(HDRP(bp));
    char *head_ptr = find_list(bp_size);
    char *cur = GET(head_ptr);

    if (!cur) {
        PUT(NEXT_LINK_RP(bp), cur);
        PUT(PREV_LINK_RP(bp), head_ptr);
        PUT(head_ptr, bp);
        return;
    }

    while (GET_SIZE(HDRP(cur)) < bp_size && GET(NEXT_LINK_RP(cur))) {
        cur = GET(NEXT_LINK_RP(cur));
    }

    if (GET_SIZE(HDRP(cur)) >= bp_size) {
        char *prev = GET(PREV_LINK_RP(cur));
        PUT(NEXT_LINK_RP(bp), cur);
        PUT(PREV_LINK_RP(bp), prev);
        PUT(NEXT_LINK_RP(prev), bp);
        PUT(PREV_LINK_RP(cur), bp);
    } else {
        PUT(NEXT_LINK_RP(cur), bp);
        PUT(PREV_LINK_RP(bp), cur);
    }

#endif
}
// 链表移除节点
static void _remove(char *bp) {
    char *head_ptr = find_list(GET_SIZE(HDRP(bp)));  // 地址
    char *head = GET(head_ptr);                      // 值
    char *next = GET(NEXT_LINK_RP(bp));
    char *prev = GET(PREV_LINK_RP(bp));

    if (prev == head_ptr) {  // 头节点
        if (next) {
            // dummy -> bp -> xxx
            PUT(PREV_LINK_RP(next), prev);
            PUT(head_ptr, next);
        } else {
            // dummy ->bp
            PUT(head_ptr, NULL);
        }
    } else {  // 非头
        if (next) {
            // dummy -> xxx -> bp -> xxx
            PUT(PREV_LINK_RP(next), prev);
            PUT(NEXT_LINK_RP(prev), next);
        } else {
            // dummy -> xxx -> bp
            PUT(NEXT_LINK_RP(prev), NULL);
        }
    }
}

static char *find_list(size_t size) {
    int i = 0;
    if (size <= SIZE_0)
        i = 0;
    else if (size <= SIZE_1)
        i = 1;
    else if (size <= SIZE_2)
        i = 2;
    else if (size <= SIZE_3)
        i = 3;
    else if (size <= SIZE_4)
        i = 4;
    else if (size <= SIZE_5)
        i = 5;
    else if (size <= SIZE_6)
        i = 6;
    else if (size <= SIZE_7)
        i = 7;
    else if (size <= SIZE_8)
        i = 8;
    else if (size <= SIZE_9)
        i = 9;
    else
        i = 10;

    return block_list_start + (i * WSIZE);
}

#endif
