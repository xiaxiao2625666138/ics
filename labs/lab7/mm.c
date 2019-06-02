/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 *
 * 使用分离适配的方法实现动态内存分配 
 * a free block is organizated like:
 * ___________________
 * |    header       |
 * |_________________|
 * |    prev         |
 * |_________________|
 * |    succ         | 
 * |_________________|
 * |                 |
 * |_________________|
 * |    footer       |
 * |_________________|
 * 
 * a allocated block is organizated like:
 * ___________________
 * |    header       |
 * |_________________|
 * |    payload      |
 * |    (and fill)   |
 * |_________________|
 * 
 * 其中：
 *  1. header为block的表头，记录了当前块的大小、
 *     当前块的f/a状态和前一个块的f/a状态，占4bytes。
 *  2. pred记录前序的同大小类型的空闲块儿的位置，
 *     记录为相对heap_listp的偏移量，占4bytes。
 *  3. succ记录后继的同大小类型的空闲块儿的位置，
 *     记录为相对heap_listp的偏移量，占4bytes。
 *  4. payload为有效载荷，fill为填充
 *  5. footer为header的副本，空闲块有footer，
 *     已分配块没有footer，占4bytes
 *  6. 根据free block的组织形式，最小的block大小为 16bytes
 *
 * 
 *  说明：
 *    1. 空闲链表采用多个大小类空闲链表，节点之间的指向存放在block块的prev和succ位置
 *    2. 每次把新增的空闲块插入相应大小类的头部
 *    3. 采用首次适配的方式分配空闲块，空闲块过大采取分割策略
 *    4. 只有当找不到合适空闲块并且NEED_COALESCE为1时才进行合并操作
 *       (melloc采取全体合并，realloc采取合并到第一次满足请求的大小为止)
*/


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


/* Basic constants and macros */
#define WSIZE 4             /* Word and header/footer size (bytes)*/
#define DSIZE 8             /* Double word and succ size (bytes) */
#define CHUNKSIZE (1<<9)    /* Extend heap by this amount (bytes)*/
#define PRE_ALLOC 2         /* 记录前一个block已分配 */
#define ALLOC 1             /* 记录当前块已分配 */

#define MAX(x,y) ((x)>(y)?(x):(y))

/* Pack a size and allocateds bit into a word */
#define PACK(size, pre_alloc, alloc) ((size)|(pre_alloc)|(alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *) (p))
#define PUT(p, val) (*(unsigned int *)(p)=(val))

/* Read the size ,allocated and pre-allocated fields from adderss p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & ALLOC)
#define GET_PRE_ALLOC(p) (GET(p) & PRE_ALLOC)

/* Given block ptr bp, compute adderss of its header */
#define HDRP(bp) ((char *)bp-WSIZE)

/* GIven block header ptr p, compute address of its payload */
#define BP(p) (void *)((char *)(p)+WSIZE)

/* Given block header pointer p, compute adderess of its footer */
#define FTRP(p) ((p)+GET_SIZE(p)-WSIZE)

/* Given block header pointer p, compute adderss of next or previous blocks header */
#define NEXTP(p) ((char *)(p) + GET_SIZE(p))
#define PREVP(p) ((char *)(p) - GET_SIZE((char *)(p)-WSIZE))

/* Given free block header ptr p, compute address of its pre and succ*/
#define PREP(p) ((char *)(p)+WSIZE)
#define SUCCP(p) ((char *)(p)+DSIZE)

/* Given free block header ptr p, compute address of
 next or pre free block */
#define NEXT_FREEP(p) (mem_start + *(unsigned int *)SUCCP(p))
#define PREV_FREEP(p) (mem_start + *(unsigned int *)PREP(p))

/* Given a free block header ptr p, turn it to be a pre or succ */
#define TURN_PRE_SUCC(p) ((unsigned int)((char *)(p)-mem_start))

/* heap_listp 初始化之后总是指向堆中的第一个 block 的 header*/
static char *heap_listp;

/* mem_start 初始化之后总是指向heap的开头 */
static char *mem_start;

/* NEED_COALESCE 用来记录上次合并之后有没有free()操作，1表示需要合并， 0表示不用合并 */
static int NEED_COALESCE=0;

/* 
* 空闲列表链表，以大小类的方式组织，每一个指针指向一个链表头
* 插入空闲链表时，插入到相应大小类的头部
*/
static unsigned int * free_list0;      /* [16, 32) 大小类 */
static unsigned int * free_list1;      /* [32, 64) 大小类 */
static unsigned int * free_list2;      /* [64, 128) 大小类 */   
static unsigned int * free_list3;      /* [128, 256) 大小类 */    
static unsigned int * free_list4;      /* [256, 512) 大小类 */   
static unsigned int * free_list5;      /* [512, 1024) 大小类 */   
static unsigned int * free_list6;      /* [1024, 2048) 大小类 */  
static unsigned int * free_list7;      /* [2048, 4096) 大小类 */  
static unsigned int * free_list8;      /* [4096, 8192) 大小类 */   
static unsigned int * free_list9;      /* [8192, 16384) 大小类 */    
static unsigned int * free_list10;     /* [16384, ～) 大小类 */    




/****************************************************************
 *****************malloc free realloc 代码区**********************
 ****************************************************************
*/


/* logInt(n) 返回n的2的对数并向下取整 */
static int logInt(int n){
    int i=0;
    for(int m=1; m<=n;i++){
        m*=2;
    }
    return i-1;
}

/* free_list(index) 返回对应大小类的空闲链表 */
static unsigned int* * free_list(int index){
    switch(index){
        case 0:return &free_list0;
        case 1:return &free_list1;
        case 2:return &free_list2;
        case 3:return &free_list3;
        case 4:return &free_list4;
        case 5:return &free_list5;
        case 6:return &free_list6;
        case 7:return &free_list7;
        case 8:return &free_list8;
        case 9:return &free_list9;
        default:return &free_list10;
    }
}


/* 
 * initFreeList() 初始化空闲链表 
*/
static void initFreeList(){
    free_list0 = NULL;
    free_list1 = NULL;
    free_list2 = NULL;
    free_list3 = NULL;
    free_list4 = NULL;
    free_list5 = NULL;
    free_list6 = NULL;
    free_list7 = NULL;
    free_list8 = NULL;
    free_list9 = NULL;
    free_list10 = NULL;
}


/*
 * addToFreeList(p) 在空闲链表中加入空闲块
*/
static void addToFreeList(char *p){
    /* 获得指定大小类的空闲链表 */
    int size = GET_SIZE(p);
    int index = (int)(logInt(size)) - 4;
    unsigned int **flist = free_list(index);
    /* 链表空，表头指向当前块 */
    if(*flist==NULL){
        *flist = (unsigned int *)p;
        PUT(PREP(p), 0);
        PUT(SUCCP(p), 0);
    /* 链表非空，当前块插入表头 */
    }else{
        PUT(PREP(*flist), TURN_PRE_SUCC(p));
        PUT(SUCCP(p), TURN_PRE_SUCC(*flist));
        PUT(PREP(p), 0);
        *flist =(unsigned int *)p;
    }
}

/*
 * extend_heap(words) 扩展heap
*/
 static char *extend_heap(size_t words){
    void *bp;
    size_t size;
    /* Allocate an even number of words to maintain alignment */
    size=(words %2)? (words+1)*WSIZE : words*WSIZE;
    if((long)(bp=mem_sbrk(size)) ==-1)return NULL;
    /* Initialize free block header/footer and the epilogue header */
    char *p=HDRP(bp);
    PUT(p, size);
    PUT(FTRP(p), size);
    PUT(NEXTP(p), 1);    /* 1 表示 PACK(0, 0, ALLOC) */
    /* 新扩展来的空间加入空闲链表 */
    addToFreeList(p);
    return p;
}

/* 
 * mm_init - initialize the malloc package，初始化为：
 * _________________________________________________________________
 * |         |   8/1/0   |     8/1/0   |    .....   |      0/0/1    |
 * —————————————————————————————————————————————————————————————————
 *   4Bytes     4Bytes        4Bytes     1<<9 Bytes     4Bytes
 */
int mm_init(void){
    if((heap_listp=(char *)mem_sbrk(4*WSIZE)) == (char *)-1)
        return -1; 
    PUT(heap_listp, 0);
    PUT(heap_listp+ 4, 9);    /* 9 表示 PACK(8, 0, ALLOC) */
    PUT(heap_listp+ 8, 9);    /* 9 表示 PACK(8, 0, ALLOC) */
    PUT(heap_listp+ 12, 3);   /* 3 表示 PACK(0, PRE_ALLOC, ALLOC) */
    mem_start=heap_listp;
    heap_listp += 12;         /* 12表示 3*WSIDE */
    initFreeList();
    if (extend_heap(CHUNKSIZE/WSIZE)== NULL)
        return -1; 
    return 0;
}

/*
 * deleteFreeBlock(p)把空闲块从空闲链表中删除
*/
static void deleteFreeBlock(char *p){
    /* 获取当前空前块所在链表的前一空闲块和后一空闲块 */
    char *prev_freep = (char *)PREV_FREEP(p);
    char *next_freep = (char *)NEXT_FREEP(p);
    unsigned int succ = GET(SUCCP(p));
    unsigned int pre = GET(PREP(p));
    /* 当前块之前还有空闲块，更改前空闲块的succ */
    if(prev_freep!=mem_start)
        PUT(SUCCP(prev_freep), succ);
    /* 当前块之后还有空闲块，更改后空闲块的pre */
    if (next_freep != mem_start)
        PUT(PREP(next_freep), pre);
    /* 当前链表仅有一个空闲块，表头设为NULL，清空空闲链表 */
    if(prev_freep==mem_start && next_freep==mem_start){
        int index = (int)(logInt(GET_SIZE(p))) - 4;
        *free_list(index) = NULL;
    /* 当前块为链表头，表头设为后一空闲块 */
    }else if(prev_freep==mem_start){
        int index = (int)(logInt(GET_SIZE(p))) - 4;
        *free_list(index) = (unsigned int *)next_freep;
    }
}


/*
 * setBlock(p,size) 把空闲块设置为分配块
*/
static void setBlock(char *p, int size){
    /* 把块从空闲链表中删除 */
    deleteFreeBlock(p);
    /* 设置当前块的内容 */
    int oldsize = GET_SIZE(p);
    int header=GET(p);
    if (size + 16 <= oldsize){/* 需要分割空闲块的情况 */
        /* 设置当前块的header 和 footer */
        int pre_alloc = GET_PRE_ALLOC(p);
        header = PACK(size, pre_alloc, 1);
        PUT(p, header);
        PUT(FTRP(p), header);
        /* 设置分割下来的空闲块的header和footer */
        char *nextp = NEXTP(p); 
        int nextb_size = oldsize - size;
        header = PACK(nextb_size, PRE_ALLOC, 0);
        PUT(nextp, header);
        PUT(FTRP(nextp), header);
        /* 分割下来的空闲块插入空闲链*/
        addToFreeList(nextp);    
    }else{ /* 不需要分割的情况 */
        /* 设置当前块的header 和 footer */
        header = header | 0x1;
        PUT(p, header);
        PUT(FTRP(p), header);
        /* 设置下一个块的header和footer */
        char *nextp = NEXTP(p);
        header = GET(nextp) | PRE_ALLOC;
        PUT(nextp,header);
        if(GET_SIZE(nextp)!=0)
            PUT(FTRP(nextp), header);
    }
}


/*
 * coalesce(size) 合并空闲链表，第一次合满足size的要求，便停止合并，返回满足要求的空闲块
*/
static char * coalesce(int size){
    /* 从头开始向后合并 */
    char *p = heap_listp;
    /* end为尾部 */
    char *end =((char *)mem_heap_hi()-3);
    char *nextp;
    int newsize, oldsize;
    /* 默认合并到尾部 */
    while (p != end){
        /* 跳过非空闲块 */
        while(p!=end && GET_ALLOC(p)==1){
            p = NEXTP(p);
        }
        if(p==end)
            break;
        newsize = oldsize = GET_SIZE(p);
        nextp = NEXTP(p);
        /* 从空闲链表中去除相邻的空闲块 */
        while(GET_ALLOC(nextp)==0){
            deleteFreeBlock(nextp);
            newsize += GET_SIZE(nextp);
            nextp = NEXTP(nextp);
        }
        /* 合并 */
        if(newsize!=oldsize){
            /* 把当前块从空闲链表中去除 */
            deleteFreeBlock(p);
            /* 设置合并后的空闲块的header和footer */
            int header = PACK(newsize, PRE_ALLOC, 0);
            PUT(p, header);
            PUT(FTRP(p), header);
            /* 把合并后的空闲块加入空闲链表 */
            addToFreeList(p);
            /* 如果已经合并到了heap的尾部，记录不需要再次和并 */
            if(nextp==end) NEED_COALESCE=0;
            /* 如果合并后满足尺寸需求，不再进行合并 */
            if(newsize>size) return p;
        }
        p = nextp;
    }
    /* 已经合并到了heap的尾部，记录不需要再次和并 */
    NEED_COALESCE=0;
    return NULL;
}

/*
 * find_fit(size) 在空闲链表中首次适配查找空闲块
*/
static char* find_fit(size_t size){
    /* 获得指定大小类索引 */
    int index = (int)(logInt(size)) - 4;
    unsigned int **flist;
    char *p;
    /* 从最佳大小类开始，向更大的大小类方向查找，首次适配返回空闲块位置 */
    for (int i = 0; i < 11 - index;i++){
        flist = free_list(index+i);
        if(*flist!=NULL){
            p = (char*)(*flist);
            while (p!=mem_start){
                if(GET_SIZE(p)>=size)
                    return p;
                p = NEXT_FREEP(p);
            }
        }
    }
    /* 没有满足要求的空闲块，返回NULL */
    return NULL;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size){
    /* 0大小的空间请求，返回NULL */
    if(size<=0)
        return NULL;
    /* 8Bytes对齐 */    
    size_t newsize = ALIGN(size + DSIZE);

    /* 在空闲列表中找合适的位置 */
   char *p= find_fit(newsize); 
   if(p){
       setBlock(p, newsize);
       return BP(p);
   }
   /* 空闲链表中没有合适的块，合并空闲列表 */
   if(NEED_COALESCE){
       coalesce(1<<25);
       NEED_COALESCE=0;
       p=find_fit(newsize);
       if(p){
          setBlock(p, newsize);
          return BP(p);
       }
   }
   /* 合并空闲列表后仍然没有合适的空闲块，扩展heap */
   size_t extendsize = MAX(newsize, CHUNKSIZE);
   if((p=extend_heap(extendsize/WSIZE))==NULL)
       return NULL;
   setBlock(p, newsize);
   return BP(p);
}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp){
    /* 设置块儿的header和footer，alloc位为0 */
    char *p= HDRP(bp);
    int header = GET(p);
    PUT(p, header & ~0x1);
    PUT(FTRP(p), header & ~0x1);
    /* 设置相邻下一块儿的header和footer，pre_alloc位为0 */
    char *nextp=NEXTP(p);
    PUT(nextp, GET(nextp) & ~0x2);
    if(nextp != (char *)mem_heap_hi()-3){
       PUT(FTRP(nextp), GET(nextp));
    }
    /* 把当前块加入空闲链表 */
    addToFreeList(p);
    if(GET_ALLOC(nextp)!=ALLOC || GET_PRE_ALLOC(p)!=PRE_ALLOC)
        NEED_COALESCE=1;
}


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 * 如下注释，分为三个情况A\B\C:
 */
void *mm_realloc(void *ptr, size_t size)
{
    char *p=HDRP(ptr);
    /* 把size对齐，获取ptr原来的尺寸 */
    size_t newsize = ALIGN(size + DSIZE);
    size_t oldsize=GET_SIZE(p);
    unsigned int header;
    /* A. 需要的尺寸比原来小，并且不需要分割 */
    if(newsize <= oldsize && newsize+16>oldsize){
        return ptr;
    /* B. 需要的尺寸比原来大 */
    }else if(newsize > oldsize){
        /* 记录下原来ptr在succ和pre位置上的数据 */
        unsigned int word1=GET(ptr);
        unsigned int word2=GET((char*)ptr+4);
        /* 释放ptr内存，除了succ位和pre位之外，其他的数据不会变动 */
        mm_free(ptr);
        /* 有内存可以合并 */
        if(NEED_COALESCE)
            p=coalesce(newsize);
        /* 合并之后没有获得满足尺寸的内存块，扩展内存 */
        if(!p){
            p=extend_heap(newsize/WSIZE);
        }
        /* 超出heap最大容量，返回空指针 */
        if(!p) return NULL;
        /* 获得了合适的内存块 */
        void *bp=BP(p);
        unsigned int data;
        /* copy原地址的内存数据 */
        for(int offset=8;offset<oldsize-8;offset+=4){
             data=GET((char *)ptr+offset);
             PUT((char *)bp+offset, data);
        }
        /* 设置内存为alloc */
        setBlock(p, newsize);
        /* copy原内存succ位和pre位的内容 */
        PUT(bp, word1);
        PUT((char *)bp+4, word2);
        return bp;
    /* C. 需要的尺寸比原来小，且需要划分 */
    }else{
        /* 重新设置header和footer */
        header=PACK(newsize, GET_PRE_ALLOC(p), ALLOC);
        PUT(p, header);
        PUT(FTRP(p), header);
        /* 设置划分下来的块儿的header和footer */
        char *nextp=NEXTP(p);
        header=PACK(oldsize-newsize, PRE_ALLOC, 0);
        PUT(nextp, header);
        PUT(FTRP(nextp), header);
        /* 把划分下来的块放入空闲链表 */
        addToFreeList(nextp);
        return ptr;
    }
}



/*********************************************************
 ************** mm_check区      **************************
 *********************************************************
*/

/*
 * mm_check() 检查heap结构是否正确, 输出错误信息
 * 若有错误返回0， 无错误返回1
 * 检查的项目包括：分别对应于 mm_checkA ~ mm_checkF
 * A. Is every block in the free list marked as free?
 * B. Are there any contiguous free blocks that somehow escaped coalescing?
 * C. Is every free block actually in the free list?
 * D. Do the pointers in the free list point to valid free blocks?
 * E. Do any allocated blocks overlap?
*/

/* A. Is every block in the free list marked as free? */
static int mm_checkA(){
    unsigned int * flist;
    char *end =((char *)mem_heap_hi()-3);
    for(int index=0;index<11;index++){
        flist=*free_list(index);
        while(flist != (unsigned int *)end){
            if(GET_ALLOC(flist)!=0){
                printf("blocks in the free list donot be marked as free ( %p )\n", (void *)flist);
                return 0;
            }
        }
    }
    return 1;  
}

/* B. Are there any contiguous free blocks that somehow escaped coalescing? */
static int mm_checkB(){
    char *p=heap_listp;
    char *nextp;
    char *end=((char *)mem_heap_hi()-3);
    int alloc, nextAlloc;
    while(p!=end){
        nextp=NEXTP(p);
        alloc=GET_ALLOC(p);
        nextAlloc=GET_ALLOC(nextp);
        if(alloc==0  && nextAlloc==0){
            printf("there are some contiguous free blocks that somehow escaped coalescing ( %p, %p )\n", p, nextp);
            return 0;
        }
        while(GET_ALLOC(nextp)==1 && nextp!=end){
            nextp=NEXTP(nextp);
        }
        p=nextp;
    }
    return 1;
}

/* C. Is every free block actually in the free list? */ 
static int mm_checkC(){
    char *p=heap_listp;
    char *end=((char *)mem_heap_hi()-3);
    int index;
    unsigned int *flist;
    while(p!=end){
        if(GET_ALLOC(p)==0){
            index=(int)(logInt(GET_SIZE(p))) - 4;
            flist=*free_list(index);
            int contain=0;
            while(flist!=(unsigned int *)mem_start){
                if(flist==(unsigned int *)p){
                    contain=1;
                    break;
                }
                flist=(unsigned int *)NEXT_FREEP(flist);
            }
            if(!contain){
                printf("Not every free block actually in the free list! ( %p )\n", p);
                return 0;
            }
        }
        p=NEXTP(p);
    }
    return 1;
}

/* D. Do the pointers in the free list point to valid free blocks? */
static int mm_checkD(){
    unsigned int * flist;
    unsigned int * prev_freep, * succ_freep;
    for(int index=0;index<11;index++){
        flist=*free_list(index);
        while(flist!=(unsigned int *)mem_start){
            prev_freep=(unsigned int *)PREV_FREEP(flist);
            succ_freep=(unsigned int *)NEXT_FREEP(flist);
            if(prev_freep!=(unsigned int *)mem_start){
                 if(GET(prev_freep)!=GET(FTRP(prev_freep))){
                     printf("the pre pointers in the free list donot point to valid free blocks ( %p )\n",(char *)flist);
                     return 0;
                 }
            }
            if(prev_freep!=(unsigned int *)mem_start){
                 if(GET(succ_freep)!=GET(FTRP(succ_freep))){
                     printf("the succ pointers in the free list donot point to valid free blocks ( %p )\n",(char *)flist);
                     return 0;
                 }
            }
            flist=(unsigned int *)NEXT_FREEP(flist);
        }
    }
    return 1;
}

/* E. Do any allocated blocks overlap? */
static int mm_checkE(){
    char *p=heap_listp;
    char *end=((char *)mem_heap_hi()-3);
    while(p!=end){
        if(GET_ALLOC(p)==1){
            if(GET(p)==GET(FTRP(p)) && FTRP(p)<end){
                p=NEXTP(p);
            }else{
                printf("allocated blocks overlap? ( %p )\n",p);
                return 0;
            }
        }
        p=NEXTP(p);
    }
    return 1;
}

/* mm_check() */
static int mm_check(){
    if(!mm_checkA()) return 0;
    if(!mm_checkB()) return 0;
    if(!mm_checkC()) return 0; 
    if(!mm_checkD()) return 0;
    if(!mm_checkE()) return 0;
    return 1;
}














