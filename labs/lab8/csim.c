/**************************
 *                        *
 *  name: 周雪振           *
 *                        *
 *  ID: 5141509091        *
 *                        *
 **************************
*/


#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define GETBIT(mask, adr) ((mask)&(adr))

/* RunMode为命令行参数的形式类型，分为HELP，VERBOSE， GENERAL， ERROR*/
typedef enum{HELP, VERBOSE, GENERAL, ERROR} RunMode;

/* cache中的行数据结构 */
typedef struct{
    long valid_bit;
    long tag_bit;
} cache_line;

/* s, E, b分别为cache的参数：
 * s为S=2**s， 中的s, S为cache的集合数
 * E为每个集合中的行数
 * b为每个block的大小
*/
static int s, E, b;
/* fp为trace文件指针 */
static FILE* fp;
/* hit_cnt, miss_cnt, eviction_cnt分别用来统计hit, miss, eviction的次数 */
static int hit_cnt, miss_cnt, eviction_cnt;
/* tag_mask, set_mask分别为tag_bit和set_bit的掩码 */
static long tag_mask, set_mask;

/* ParseArg(): 解析命令行参数 */
RunMode ParseArg(int argc, char **argv);

/* printHelp(): 查看命令行帮助信息 */
void printHelp();

/* init(): 初始化cache参数和trace文件 */
int init(char* sNum, char* ENum, char* bNum, char *file);

/* cache模拟 */
void cacheSim(RunMode);


int main(int argc, char ** argv)
{
    RunMode rm=ParseArg(argc, argv);
    if(rm==HELP){
        printHelp();
        return 0;
    }else if(rm==VERBOSE){
        if(init(argv[3], argv[5],argv[7], argv[9])==0){
            printf("%s: No such file or directory\n", argv[9]);
            return 0;
        }
    }else if(rm==GENERAL){
        if(init(argv[2], argv[4], argv[6], argv[8])==0){
            printf("%s: No such file or directory\n", argv[8]);
            return 0;
        }
    }else{
        printf("./csim: Missing required command line argument\n");
        printHelp();
        return 0;
    }
    cacheSim(rm);
    fclose(fp);
    printSummary(hit_cnt, miss_cnt, eviction_cnt);
    return 0;
}


/*
 * 初始化cache参数和trace文件
*/
int init(char* sNum, char* ENum, char* bNum, char *file){
    s=atoi(sNum);
    E=atoi(ENum);
    b=atoi(bNum);
    tag_mask=((long)(-1))<<(b+s);
    set_mask=(((long)(-1))<<b)^tag_mask;
    fp=fopen(file, "r");
    if(!fp) return 0;
    return 1;
}


/*
 * 解析命令行参数
*/
RunMode ParseArg(int argc, char **argv){
    if(argc==2){
        if(strcmp(argv[1], "-h")!=0)
            return ERROR;
        return HELP;
    }
    if(argc==10){
        if(strcmp(argv[1], "-v")!=0)
            return ERROR;
        if(strcmp(argv[2], "-s")!=0)
            return ERROR;
        if(strspn(argv[3], "0123456789")!=strlen(argv[3]))
            return ERROR;
        if(strcmp(argv[4], "-E")!=0)
            return ERROR;
        if(strspn(argv[5], "0123456789")!=strlen(argv[5]))
            return ERROR;
        if(strcmp(argv[6], "-b")!=0)
            return ERROR;
        if(strspn(argv[7], "0123456789")!=strlen(argv[7]))
            return ERROR;
        if(strcmp(argv[8], "-t")!=0)
            return ERROR;
        return VERBOSE;
    }
    if(argc==9){
        if(strcmp(argv[1], "-s")!=0)
            return ERROR;
        if(strspn(argv[2], "0123456789")!=strlen(argv[2]))
            return ERROR;
        if(strcmp(argv[3], "-E")!=0)
            return ERROR;
        if(strspn(argv[4], "0123456789")!=strlen(argv[4]))
            return ERROR;
        if(strcmp(argv[5], "-b")!=0)
            return ERROR;
        if(strspn(argv[6], "0123456789")!=strlen(argv[6]))
            return ERROR;
        if(strcmp(argv[7], "-t")!=0)
            return ERROR;
        return GENERAL;
    }
    return ERROR;
}

/*
* 打印帮助信息
*/
void printHelp(){
    printf("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("  -h         Print this help messae.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("  linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

/* initCache(): 初始化cache */
void initCache(cache_line (*)[E]);

/*cacheLoad(): 加载数据*/
void cacheOption(cache_line (*)[E], RunMode, long);

/* pow(): 计算2的幂次 */
int power(int);

void cacheSim(RunMode rm){
    cache_line (*cache)[E]=(cache_line (*)[E])malloc(sizeof(cache_line)*power(s)*E);
    initCache(cache);
    char op[2];
    long adr;
    int size;
    char ch;
    fscanf(fp, "%s", op);
    while(feof(fp)==0){
        fscanf(fp, "%lx%c%d",&adr, &ch, &size);
        if(strcmp(op,"I")!=0){
            if(rm==VERBOSE)printf("%s %lx,%d", op, adr, size);
            if(strcmp(op, "L")==0 || strcmp(op, "S")==0){
                cacheOption(cache, rm, adr);
                if(rm==VERBOSE)printf("\n");
            }
            else if(strcmp(op, "M")==0){
                cacheOption(cache, rm, adr);
                cacheOption(cache, rm, adr);
                if(rm==VERBOSE)printf("\n");
            }
        }
        fscanf(fp, "%s", op);
    }
    free(cache);
} 

/*
 * 初始化cache
*/
void initCache(cache_line (*cache) [E]){
    for(int i=0;i<s;i++){
        for(int j=0;j<E;j++){
           cache[i][j].valid_bit=0; 
        }
    }
}

/*
 * 计算幂次
*/
int power(int y){
    if(y==0)return 1;
    int t=power(y/2);
    if(y%2==0)return t*t;
    return t*t*2;
}

/*
 * accessCache(): 访问cache
*/
cache_line *accessCache(cache_line *cset, long tag_bit){
    for(int i=0; i<E; i++){
        if(cset[i].valid_bit>0 && cset[i].tag_bit==tag_bit)
            return &cset[i];
    }
    return NULL;
}

/*
 * setValidBit(): 设置一个cache set中的valid_bit位
*/
void setValidBit(cache_line *cset, cache_line *cl){
    cl->valid_bit=1;
    for(int i=0;i<E;i++){
        if(&cset[i]!=cl && cset[i].valid_bit>0){
            cset[i].valid_bit++;
        }
    }
}

/*
 * getFreeCacheLine(): 获取cache set中valid_bit为0的cache_line
*/
cache_line *getFreeCacheLine(cache_line *cset){
    for(int i=0;i<E;i++){
        if(cset[i].valid_bit==0)
            return &cset[i];
    }
    return NULL;
}

/*
 * evict(): 淘汰一个cache_line
*/
cache_line *evict(cache_line * cset){
    cache_line *cl=cset;
    for(int i=1;i<E;i++){
        if(cl->valid_bit<cset[i].valid_bit)
            cl=&cset[i];
    }
    return cl;
}

/*
 *加载数据
*/
void cacheOption(cache_line (*cache)[E], RunMode rm, long adr){
    long tag_bit=GETBIT(tag_mask, adr);
    long set_bit=GETBIT(set_mask, adr);
    int setNum=(int)(set_bit>>b);
    cache_line *cl;
    if((cl=accessCache(cache[setNum], tag_bit))!=NULL){
        if(rm==VERBOSE) printf(" hit");
        hit_cnt++;
        cl->tag_bit=tag_bit;
        setValidBit(cache[setNum], cl);
        return;
    }
    if(rm==VERBOSE) printf(" miss");
    miss_cnt++;
    cl=getFreeCacheLine(cache[setNum]);
    if(cl){
        cl->tag_bit=tag_bit;
        setValidBit(cache[setNum], cl);
        return;
    }
    if(rm==VERBOSE) printf(" eviction");
    eviction_cnt++;
    cl=evict(cache[setNum]);
    cl->tag_bit=tag_bit;
    setValidBit(cache[setNum], cl);
}






