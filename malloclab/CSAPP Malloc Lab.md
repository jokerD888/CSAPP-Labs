# CSAPP Malloc Lab

在这个实验室中，您将为C程序编写一个动态存储分配器，即您自己版本的`malloc`、`free`和`realloc`例程，实现一个正确，高效和快速的分配器。本实验性能指标有两个方面，内存利用率和吞吐量，这两个方面都是动态存储分配器优秀与否的重要衡量指标，我们的分配器需要在吞吐量和内存利用率直接取得平衡以获取更高的分数。

若从官网中下载的实验文件，缺少测试文件，可从[这里下载](https://github.com/pgoodman/csc369/tree/master/malloclab/traces)。

本实验的代码中使用了大量的宏和指针，需要特别小心。此外，为方便调试程序以及便利地对比各个版本的差异，使用了宏进行条件编译。

完成实验的方法有多种，下面主要根据书中介绍的分配器进行编码。

## 隐式空闲链表

隐式空闲链表：空闲块通过头部中的大小字段隐含地连接着的，分配器可以通过遍历堆中所有的块，从而间接地遍历整个空闲块的集合。

```c
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

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define IMPLICIT

// 隐式空闲链表--------------------------------
// 分配策略：默认是首次分配，通过定义以下宏，可以选择其他策略
// 下一次适配
// #define NEXT_FIT

//  最佳适配
// #define BEST_FIT

// 常数及宏 begin-----------------------------------------
#ifdef IMPLICIT

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

// 常数及宏 end-----------------------------------------

// 私有全局变量即函数
#ifdef NEXT_FIT
static char *prev_bp = NULL;
#endif

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
    }  // else 都已分配
#ifdef NEXT_FIT
    prev_bp = bp;
#endif
    return bp;
}
static void *find_fit(size_t asize) {
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
}
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    // 当分割剩下的块大小 >= 最小块大小（2*DSIZE）时才进程分割
    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));  // 需要注意的是：FTRP是通过HDRP运作的，所有要注意两者的先后关系
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
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

    newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;

    size = GET_SIZE(HDRP(oldptr));         // 获取原块的大小
    copySize = GET_SIZE(HDRP(newptr));     // 获取新块的大小
    if (size < copySize) copySize = size;  // 截断

    memcpy(newptr, oldptr, copySize - DSIZE);  //-DSIZE 是减去头部脚部的开销
    mm_free(oldptr);
    return newptr;
}

#endif
```

### 首次适配

-   首次适配：从头开始搜索空闲链表，选择第一个合适的空闲块。
    优点：它趋向于将大的空闲块保留在链表的后面；缺点：它趋向于在靠近链表起始处留下空闲块的“碎片”，这就增大了对较大块的搜索时间。

使用首次适配，测试结果如下：

![](E:/csapp/m1.png)

### 下一次适配

-   下一次适配：从上一次查询结束的地方开始进行搜索。
    优点：下一次适配比首次适配运行起来明显要快一些，求其是当链表的前面布满了许多小的碎片时；缺点：然而下一次适配的内存利用率要比首次适配低得多。

需要注意的是需要在`coalescs`内也要相应修改，即在合并空闲块后，让`prev_bp`指向刚合并的空闲块，这样做不仅可以提高内存利用率也可以提高吞吐量，即使不谈这些好处，也必须在`coalescs`内对`prev_bp`相应修改。因为，当扩展块A合并上一个空闲块B后，A和B合为一大块空闲块，`mm_malloc`返回指针`bp`即块B，但`prev_bp`仍然指向A，对`bp`的写入会导致`prev_bp`处的数据被修改即块A的头部会被数据覆盖。所以当下一次适配时，依然从`prev_bp`开始寻找，但实际上`prev_bp`指向的块的头部已被修改，从而导致段错误，但只要在`coalescs`内合并后修改`prev_bp`的指向即可解决这个`BUG`。（这个问题可真是够不好找的，先使用`gdb`查看`core`文件，定位到段错误的位置，再进行调试，通过查看块的内容以及头部方才揪出问题所在，调试截图如下。） 使用下一次适配测试如下，可以看都内存的利用率虽然有所略微下降，但吞吐量却得到了较大的提高。

![](CSAPP%20Malloc%20Lab.assets/m3.png)

![](E:/csapp/m2.png)

### 最佳适配

-   最佳适配：检查每一个空闲块，选择适合所需请求大小的最小空闲块。
    优点：最佳适配比首次适配和下一次适配的内存利用率都要高一些；缺点：然而，在简单空闲链表组织结构中如隐式空闲链表中，使用最佳适配的缺点是它要求对堆进行彻底的搜索，但更加精细复杂的分离式空闲链表组织，它接近于最佳适配策略，不需要进行彻底的堆搜索。

测试结果如下，内存利用率是要高一些，但相反吞吐量却较小。

![](CSAPP%20Malloc%20Lab.assets/m4.png)

## 显式空闲链表

隐式空闲链表对于通用的分配器并不合适，因为块分配与堆块的总数呈线性关系，一种更好的方法是将**空闲块**组织为某种形式的显示数据结构。因为根据定义，程序不需要一个空闲块的主体，所以实现这个数据结构的指针可以存放在这些空闲块的主体。

这样使得分配时就不需要检查已分配的块，例如，使得首次适配的分配时间从**块总数**的线性时间减少到了**空闲块数量**的线性时间。不过，释放一个块的时间可以是线性的（按照地址顺序来维护)，也可以是个常数(LIFO后进先出顺序），这取决于所选择的空闲链表中的排序策略。

```c
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

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// #define IMPLICIT
#define EXPLICIT

// 隐式空闲链表--------------------------------
// 分配策略：默认是首次分配，通过定义以下宏，可以选择其他策略
// 下一次适配
// #define NEXT_FIT
//  最佳适配
// #define BEST_FIT

// 显示空闲链表----------------------------------
// 链表维护策略：
// 最进先出 LIFO
// #define LIFO
// 默认是按地址顺序维护

// 常数及宏 begin-----------------------------------------

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
    char *t_root = GET(list_head);
    if (t_root) {  // 链表不为空
        PUT(PREV_LINK_RP(t_root), bp);
    }
    PUT(NEXT_LINK_RP(bp), t_root);
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

    if (cur >= bp) {  // 要将bp节点插入到cur之前
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
    } // else 都已分配
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

```



### LIFO

-   将新释放的块放置在链表的开始处。使用 LIFO 的顺序和首次适配的放置策略，分配器会最先检查最近使用过的块。在这种情况下，释放一个块可以在常数时间内完成。如果使用了边界标记，那么合并也可以在常数时间内完成。

由于需要操作链表，所以定义一些函数以及宏，如下：

```c
// bp->next
#define NEXT_LINK_RP(bp) ((char *)(bp))		
// bp->prev
#define PREV_LINK_RP(bp) ((char *)(bp) + WSIZE)
static void *list_head;     // 表头
// 链表头插
static void _insert(char *bp) {
    // 指针先初始化为空
    PUT(NEXT_LINK_RP(bp), 0);
    PUT(PREV_LINK_RP(bp), 0);
    char *t_root = GET(list_head);
    if (t_root) {  // 链表不为空
        PUT(PREV_LINK_RP(t_root), bp);
    }
    PUT(NEXT_LINK_RP(bp), t_root);
    PUT(list_head, bp);
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
int mm_init(void) {
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
    // 以空闲块扩展空堆
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) return -1;

    return 0;
}
```

同时需要注意在程序的某些必要地方进行`_remove`和`_insert`的调用，以及调整`mm_init`函数内的堆块的内存分配大小。

-   在`coalescs`内，进行合并时，需要调用`_remove`删除链表上需要合并的空闲块，合并完后再`_insert`插入到空闲链表。
-   在`place`内，进行块的分割前，需要先调用`_remove`从空闲链表上删除所指的一大块，若后续判断确实需要分割时，再将分割后多余空闲块`_insert`插入到空闲链表。
-   同时为了简洁，对链表指针的初始化操作直接放在了`_insert`函数的起始处。
-   在`mm_init`内，对于隐式空闲链表多了一个两个指针作为哨兵节点，所以需要初始化堆时需要分配六个`WSIZE`。

![](CSAPP%20Malloc%20Lab.assets/m5-16747337502741.png)

### 按地址排序

按照地址顺序来维护链表，其中链表中每个块的地址都小于它后继的地址。在这种情况下，释放一个块需要线性时间的搜索来定位合适的前驱。平衡点在于，按照地址排序的首次适配比 LIFO 排序的首次适配有更高的内存利用率，接近最佳适配的利用率。

将LIFO策略变为按地址排序的策略，只需要简单修改`_insert`的实现，将头插变为按地址大小排序插入，如下：

```c
static void _insert(char *bp) {
    // 指针先初始化为空
    PUT(NEXT_LINK_RP(bp), 0);
    PUT(PREV_LINK_RP(bp), 0);
#ifdef LIFO
    char *t_root = GET(list_head);
    if (t_root) {  // 链表不为空
        PUT(PREV_LINK_RP(t_root), bp);
    }
    PUT(NEXT_LINK_RP(bp), t_root);
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
    while (cur < bp && GET(NEXT_LINK_RP(cur))) {	// 注意：GET后才是指针的值
        cur = GET(NEXT_LINK_RP(cur));
    }

    if (cur >= bp) {  // 要将bp节点插入到cur之前
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
```

编写上述链表算法时，不曾想又遇到了段错误，本以为简单的链表插入算法，在宏和指针的折磨下，也是傻傻的`DEBUG`了不少时间，最终是大意混淆了指针的地址和指针的值，如上文注释。测试结果如下：

空间利用率确实得以提高，接近最佳适配，但吞吐量自然是下降，不过仍然可以达到吞吐量的上限百分之四十，所以总的性能指标得以提高达到`84/100`。

![](CSAPP%20Malloc%20Lab.assets/m6.png)

## 分离空闲链表

即使上面的显示空闲链表分配器也需要与空闲块数量呈线性关系的时间来分配块。一种流行的减少分配时间的方法，通常称为分离存储(segregatedstorage)，就是维护多个空闲链表，其中每个链表中的块有大致相等的大小。一般的思路是将所有可能的块大小分成一些等价类，也叫做大小类(size class)。有很多种方式来定义大小类，如根据2的幂来划分块大小。分配器维护着一个空闲链表数组，每个大小类一个空闲链表，按照大小的升序排列。当分配器需要一个大小为 n 的块时，它就搜索相应的空闲链表。如果不能找到合适的块与之匹配，它就搜索下一个链表，以此类推。

书上介绍了两种基本的方法：简单分离存储和分离适配以及分离适配的一个特例伙伴系统。其中简单分离存储，有一个显著的缺点是很容易造成内部和外部碎片，因为空闲块不会被分割，因为不合并，所以某些引用模式会引起极多的外部碎片，如依次对每个大小类做大量的分配和释放请求，导致创建了许多不会被回收的存储器；伙伴系统是分离适配的一中特例，每个大小类都是2的幂，所以可能会导致显著的内部碎片，因此不适合做通用的分配器，不过对块大小预知是2的幂，伙伴系统分配器就很有吸引力了。综上，简单分离存储和分离适配都不是适合做通用的分配器，所以，下面使用书中介绍的分离适配来实现分配器。

分离适配：分配器维护着一个空闲链表的数组。每个空闲链表是和一个大小类相关联的，并且被组织成某种类型的**显式**或**隐式**链表。每个链表包含潜在的**大小不同**的块。这些块的大小是大小类的成员。有许多种不同的分离适配分配器。这里，我们描述了一种简单的版本。

为了分配一个块，必须确定请求的大小类，并且对适当的空闲链表做首次适配，查找一个合适的块。如果找到了一个，那么就(可选地)分割它，并将剩余的部分插入到适当的空闲链表中。如果找不到合适的块，那么就搜索下一个更大的大小类的空闲链表。如此重复，直到找到一个合适的块。如果空闲链表中没有合适的块，那么就向操作系统请求额外的堆内存，从这个新的堆内存中分配出一个块，将剩余部分放置在适当的大小类中。要释放一个块，我们执行合并，并将结果放置到相应的空闲链表中。

分离适配方法是一种常见的选择，C 标准库中提供的 GNU malloc 包就是采用的这种方法，因为这种方法既快速，对内存的使用也很有效率。搜索时间减少了，因为搜索被限制在堆的某个部分，而不是整个堆。内存利用率得到了改善，因为有一个有趣的事实:对分离空闲链表的简单的首次适配搜索，其内存利用率近似于对整个堆的最佳适配搜索的内存利用率。

上面关于分离适配的介绍都是书上的原话，为了代码的简洁，这部分不再和上面的代码混合在一起进行条件编译了，而是整体使用条件编译。关于空闲链表的组织形式这里选择显式链表。

### 分离适配+LIFO

```c
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

// 链表插入
static void _insert(char *bp) {
    // 指针先初始化为空
    PUT(NEXT_LINK_RP(bp), 0);
    PUT(PREV_LINK_RP(bp), 0);

    char *head_ptr = find_list(GET_SIZE(HDRP(bp)));  // 头节点指针的地址
    char *head = GET(head_ptr);                      // 头节点地址

    if (head) {
        PUT(PREV_LINK_RP(head), bp);
    }
    PUT(NEXT_LINK_RP(bp), head);
    PUT(PREV_LINK_RP(bp), head_ptr);
    PUT(head_ptr, bp);

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

```

![](CSAPP%20Malloc%20Lab.assets/m7.png)

### 分离适配+按size排序（最佳适配）

按照size从小到大维护链表中的顺序，这样使用首次适配就相等于最佳适配，选择大小最合适的块进行分配。只需要更改`_insert`函数如下即可。

```c
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
```

测试如下，观察到第4，7，8，9，10表现都不够好，下面将进行逐步优化。

![](CSAPP%20Malloc%20Lab.assets/m8.png)

#### 优化1：`mm_init`中的`extend_heap`多扩展8个字节

先说代码，只要在`mm_init`中修改如下即可。

![](CSAPP%20Malloc%20Lab.assets/m13.png)

再看测试结果：

![](CSAPP%20Malloc%20Lab.assets/m12-167515290942311.png)

可以发现，修改后的程序，表现良好，第4项测试内存利用率达到了99%，整体得以提升至87/100。

这么做的原因是，观察第4项的测试文件`coalescing-bal.rep`，这个测试文件重复分配两个大小相等的块（大小4095）并释放，然后立即分配并释放两倍大的块(大小8190)，添加额外开销（头部和脚部）以及对齐后，4095变为4104，8190变为8200。因为`mm_init`时`extend_heap`了一次4096的块，但遇到第一次要分配4104的块时，不够分配，又`extend_heap`了4104的块，第二次同理，又`extend_heap`了4104的块，后面又释放两个块，合并后空闲块大小为`4096+4104+4104=12304`，随后又要分配8200字节，随后释放。之后都是重复以上步骤，内存利用率最高为`8190/12304 = 66%`，计算结果与未优化之前的内存利用率吻合。
		若`mm_init`初始化时多分配8个，即`4096+8=4140`，即可使得第一次分配4140时刚好找到空闲块，第二次分配4140时，需要`extend_heap`扩展4140的块，随后释放两个，又申请8190，然后释放。重复以上步骤。内存利用率最高为`8190/(4104+4104) = 99%`，与测试结果吻合。

### 优化2：改进`mm_realloc`

观察第9项和第10项的内存利用率很低，这两个是关于`mm_realloc`的测试，下面就对此进行改进，我们可以发现`mm_realloc`的问题非常明显，不管三七二十一，就直接释放原块，再找新块，进行造成内存的浪费，改进如下。

我们先尝试能否在原地进行`realloc`，即判断ptr的下一个是否是空闲块，若是，空闲块的大小+ptr块的大小是否可以满足要求的size，若是就合并两个块。否则判断下一个块为结尾块，就扩展。否则再判断上一个块是否为空闲块，若是空闲块的大小+ptr块的大小是否可以满足要求的size，若是就合并两个块。否则，就释放ptr块，再用`mm_malloc`分配size大小。

```c
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

```

改进后，测试结果如下：

![](CSAPP%20Malloc%20Lab.assets/m14.png)

### 优化3：优化`place`

发现测试文件第7项和第8项内存利用率只有一半，所以观察对于的测试文件`{binary,binary2}-bal.rep`，发现分配模式是交替分配一个小的内存块和大块的内存块。在大量交替分配后，又释放全部的大块内存，导致内存中的分配状态为：`ALLOC->FREE->ALLOC->FREE->ALLOC->FREE->...`。导致大量的空闲块无法合并，随后又大量分配比之前的大块更大的内存，进而只能重新`extend_heap`新的内存，导致内存利用率下降。针对这个间隔空闲块的问题，解决思路如下：

划定一个界限，我这里设置的是88，只要在`place`时，从大块要分割asize大小时，只要剩余大小`remain_size`大于`2*DSIZE`时就分割，不同的是，当asize>=88，就将大块后面的asize个字节分配出去，剩余的前面的块，作为空闲块；当asize<88，就将大块前面的asize字节分配出去，剩余的后面的块就做为空闲块。

```c
static void *place(void *bp, size_t asize) {
    void *res_bp = bp;
    size_t csize = GET_SIZE(HDRP(bp));
    size_t remain_size = csize - asize;
    _remove(bp);  // 先从空闲链表删除bp块

    if (remain_size < (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    } else if (asize >= 88) {
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
```

测试如下，第7项第8项运行良好，但第10项却略有下降，总分达到`96/100`。

![](CSAPP%20Malloc%20Lab.assets/m15-16752638604693.png)

发现第10项数据有所下降，导致总提升不大，观察`realloc2-bal.rep`文件，发现我们之前将>=86的块放后面不太合适，导致第10项测试文件内存利用率下降，只需要将上面的`place`函数中的>=88改为<88即可，测试如下，达到`98/100`。

![](CSAPP%20Malloc%20Lab.assets/m16.png)

代码如下：

```c
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
```



## 总结

总代码起始附近定义了如下的宏用于条件编译，选择不同的分配策略，只需要解开对于的注释即可。总代码见[这里](https://github.com/jokerD888/CSAPP-Labs/blob/main/malloclab/malloclab-handout/mm.c)。

```c
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

```

本实验不愧是非常棘手的一个，耗费的时间也是最多，越到后面想要提升分数越困难。我认为主要的难点，在于大量的宏和非类型的指针操作，进而导致程序难以调试，而代码本身是比较容易理解。做完实验后，对动态内存分配器有了深入的理解，对其工作原理也算是了然于心， 对内存的分配心中也有了把控。内存管理和指针本就是`C/C++`的重要课题，经过此实验，对其理解无疑又上一层楼。

参考文章：[core dumped](https://blog.csdn.net/scjdas/article/details/128585787?spm=1001.2101.3001.6650.1&utm_medium=distribute.pc_relevant.none-task-blog-2%7Edefault%7EOPENSEARCH%7ERate-1-128585787-blog-108762118.pc_relevant_default&depth_1-utm_source=distribute.pc_relevant.none-task-blog-2%7Edefault%7EOPENSEARCH%7ERate-1-128585787-blog-108762118.pc_relevant_default&utm_relevant_index=2)，https://github.com/mightydeveloper/Malloc-Lab/blob/master/mm.c，https://blog.csdn.net/weixin_43362650/article/details/122521356。

