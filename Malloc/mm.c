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


/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Tianjin University",
    /* First member's full name */
    "Biemenghan",
    /* First member's email address */
    "zbz_tianj2024@tju.edu.cn",
    /* Second member's full name */
    "None",
    /* Second member's email address */
    "None",
};


/* 单字4字节或双字8字节对齐 */
#define ALIGNMENT 8

/* 向上舍入到 ALIGNMENT 的最近倍数 */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 基本常量 */
#define WSIZE 4 /* 字和头部脚部大小 */
#define DSIZE 8 /* 双字大小 */
#define CHUNKSIZE (1 << 12) /* 按此大小扩展堆 */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* 将大小和已分配位打包到一个字中 */
#define PACK(size, alloc) ((size) | (alloc))

/* 在地址 p 处读取和写入一个字 */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* 从地址 p 读取大小和已分配字段 */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* 给定块指针 bp，计算其地址 */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* 给定块指针 bp，计算下一个和上一个块的地址 */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void *coalesce(void *bp);

static char *heap_listp;
static char *pre_listp;


static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* 分配字以保持对齐 */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* 初始化空闲块和结尾块 */

    PUT(HDRP(bp), PACK(size, 0)); /* 空闲块头部 */
    PUT(FTRP(bp), PACK(size, 0)); /* 空闲块脚部 */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* 新的结尾块头部 */

    /* 如果前一个块是空闲的，则合并 */
    return coalesce(bp);
}


/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* 创建初始空堆 */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0); /* 对齐填充 */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    heap_listp += (2 * WSIZE);
    pre_listp = heap_listp;
    /* 用 CHUNKSIZE 字节的空闲块扩展堆 */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}


static void *find_fit(size_t asize)
{
    char *bp = pre_listp;
    size_t alloc;
    size_t size;
    while (GET_SIZE(HDRP(NEXT_BLKP(bp))) > 0) {
        bp = NEXT_BLKP(bp);
        alloc = GET_ALLOC(HDRP(bp));
        if (alloc) continue;
        size = GET_SIZE(HDRP(bp));
        if (size < asize) continue;
        return bp;
    }
    bp = heap_listp;
    while (bp != pre_listp) {
        bp = NEXT_BLKP(bp);
        alloc = GET_ALLOC(HDRP(bp));
        if (alloc) continue;
        size = GET_SIZE(HDRP(bp));
        if (size < asize) continue;
        return bp;
    }
    return NULL;
}


static void place(void *bp, size_t asize)
{
    size_t size = GET_SIZE(HDRP(bp));

    if ((size - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(size - asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size - asize, 0));
    }

    else {
        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));
    }
    pre_listp = bp;
}


/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; /* 调整后的块大小 */
    size_t extendsize; /* 如果没有合适的块则扩展堆的数量 */
    char *bp;

    /* 忽略虚假请求 */
    if (size == 0)
        return NULL;

    /* 调整块大小 */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    /* 在空闲列表中搜索合适的块 */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* 未找到合适的块，获取更多内存并放置块 */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    //int isPre = (pre_listp == bp);
    if (prev_alloc && next_alloc) {
        pre_listp = bp;
        return bp;
    }

    if (prev_alloc && !next_alloc) {
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
    pre_listp = bp;
    return bp;
}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
       return mm_malloc(size);
    if (size == 0)
       mm_free(ptr);

    void *newptr;
    size_t copySize;
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    size = GET_SIZE(HDRP(ptr));
    copySize = GET_SIZE(HDRP(newptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, ptr, copySize - WSIZE);
    mm_free(ptr);
    return newptr;
}



/* below code if for check heap invarints */

/*
static void printblock(void *bp)
{
    long int hsize, halloc, fsize, falloc;

    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));

    if (hsize == 0) {
        printf("%p: EOL\n", bp);
        return;
    }

    printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp,
           hsize, (halloc ? 'a' : 'f'),
           fsize, (falloc ? 'a' : 'f'));
}

static int checkblock(void *bp)
{
    //area is aligned
    if ((size_t)bp % 8)
        printf("Error: %p is not doubleword aligned\n", bp);
    //header and footer match
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
        printf("Error: header does not match footer\n");
    size_t size = GET_SIZE(HDRP(bp));
    //size is valid
    if (size % 8)
       printf("Error: %p payload size is not doubleword aligned\n", bp);
    return GET_ALLOC(HDRP(bp));
}

static void printlist(void *i, long size)
{
    long int hsize, halloc;

    for(;i != NULL;i = SUCC(i))
    {
        hsize = GET_SIZE(HDRP(i));
        halloc = GET_ALLOC(HDRP(i));
        printf("[listnode %ld] %p: header: [%ld:%c] prev: [%p]  next: [%p]\n",
           size, i,
           hsize, (halloc ? 'a' : 'f'),
           PRED(i), SUCC(i));
    }
}

static void checklist(void *i, size_t tar)
{
    void *pre = NULL;
    long int hsize, halloc;
    for(;i != NULL;i = SUCC(i))
    {
        if (PRED(i) != pre) printf("Error: pred point error\n");
        if (pre != NULL && SUCC(pre) != i) printf("Error: succ point error\n");
        hsize = GET_SIZE(HDRP(i));
        halloc = GET_ALLOC(HDRP(i));
        if (halloc) printf("Error: list node should be free\n");
        if (pre != NULL && (GET_SIZE(HDRP(pre)) > hsize))
           printf("Error: list size order error\n");
        if (hsize < tar || ((tar != (1<<15)) && (hsize > (tar << 1)-1)))
           printf("Error: list node size error\n");
        pre = i;
    }
} */


/*
 * mm_checkheap - Check the heap for correctness
 */

/*
void mm_checkheap(int verbose)
{
    checkheap(verbose);
}

void checkheap(int verbose)
{
    char *bp = heap_listp;

    if (verbose)
        printf("Heap (%p):\n", heap_listp);

    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
        printf("Bad prologue header\n");
    // block level
    checkblock(heap_listp);
    int pre_free = 0;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (verbose)
            printblock(bp);
        int cur_free = checkblock(bp);
        //no contiguous free blocks
        if (pre_free && cur_free) {
            printf("Contiguous free blocks\n");
        }

    }
    //list level
    int i = 0, tarsize = 1;
    for (; i < LISTMAX; i++) {
        if (verbose)
            printlist(seg_free_lists[i], tarsize);
        checklist(seg_free_lists[i],tarsize);
        tarsize <<= 1;
    }

    if (verbose)
        printblock(bp);
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
        printf("Bad epilogue header\n");
} */