/******************************
 *                            *
 *    name: 周雪振             *
 *                            *
 *    ID: 5141509091          *
 *                            *
 ******************************
*/


/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);


/* 32*32的矩阵转换 */
void trans32_32(int M, int N, int A[N][M], int B[M][N]);
/* 64*64的矩阵转换 */
void trans64_64(int M, int N, int A[N][M], int B[M][N]);
/* 61*67的矩阵转换 */
void trans61_67(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    /* 对三种测试用例使用不同的策略 */
    if(M==32 && N==32) trans32_32(M, N, A, B);
    else if(M==64 && N==64) trans64_64(M, N, A, B);
    else if(M==61 && N==67) trans61_67(M, N, A, B);
}

/*
 * 32*32矩阵转置
*/
void trans32_32(int M, int N, int A[N][M], int B[M][N]) { 
    int i, k, s, temp, temp1, temp2, temp3, temp4, temp5, temp6, temp7;
    for(k=0; k<N; k+=8){
        for(s=0; s<M; s+=8){
            for(i=k;i<k+8;i++){
                /*使用8*8的block，对转置进行优化，8个临时变量作为中间变量*/
                temp=A[i][s];
                temp1=A[i][s+1];
                temp2=A[i][s+2];
                temp3=A[i][s+3];
                temp4=A[i][s+4];
                temp5=A[i][s+5];
                temp6=A[i][s+6];
                temp7=A[i][s+7];
                B[s][i]=temp;
                B[s+1][i]=temp1;
                B[s+2][i]=temp2;
                B[s+3][i]=temp3;
                B[s+4][i]=temp4;
                B[s+5][i]=temp5;
                B[s+6][i]=temp6;
                B[s+7][i]=temp7;
            }
        }
    }
}

/* 64*64的矩阵转置 
 * 策略为使用4*4block的同时，以8*8的block为单位进行操作
 * 借助B矩阵的hit(4*4)block存放A矩阵的hit(4*4)block
 * 进而减少miss的次数: 一个8*8block的转置流程如下：
 *
 * A: 1 2 3 4|5 6 7 8|   B: 0 0 0 0|0 0 0 0|  
 *    1 2 3 4|5 6 7 8|      0 0 0 0|0 0 0 0|
 *    1 2 3 4|5 6 7 8|      0 0 0 0|0 0 0 0|
 *    1 2 3 4|5 6 7 8|      0 0 0 0|0 0 0 0|
 *    1 2 3 4 5 6 7 8       0 0 0 0 0 0 0 0
 *    1 2 3 4 5 6 7 8       0 0 0 0 0 0 0 0
 *    1 2 3 4 5 6 7 8       0 0 0 0 0 0 0 0
 *    1 2 3 4 5 6 7 8       0 0 0 0 0 0 0 0    
 *  ## B||之间的4*4block用来临时存储A||中的数据
 *  ## 第一步之后，访问B和A||之间的数据将都为hit的情况
 * 1. ===>>
 * B: 1 1 1 1|0 0 0 0|
 *    2 2 2 2|0 0 0 0|
 *    3 3 3 3|0 0 0 0|
 *    4 4 4 4|0 0 0 0|
 *    0 0 0 0 0 0 0 0
 *    0 0 0 0 0 0 0 0
 *    0 0 0 0 0 0 0 0
 *    0 0 0 0 0 0 0 0
 * 2. ===>> ##第二步不产生miss
 * B: 1 1 1 1|5 5 5 5|
 *    2 2 2 2|6 6 6 6|
 *    3 3 3 3|7 7 7 7|
 *    4 4 4 4|8 8 8 8|
 *    0 0 0 0 0 0 0 0
 *    0 0 0 0 0 0 0 0
 *    0 0 0 0 0 0 0 0
 *    0 0 0 0 0 0 0 0
 * 3. ===>> ##第三步转移B||的数据不产生多余的miss(除了每行store第一次访问时)
 *          ##第三步转置A左下4*4block不产生多余的miss(除了每行load第一次访问时)
 * B: 1 1 1 1 1 1 1 1
 *    2 2 2 2 2 2 2 2
 *    3 3 3 3 3 3 3 3
 *    4 4 4 4 4 4 4 4
 *    5 5 5 5 0 0 0 0
 *    6 6 6 6 0 0 0 0
 *    7 7 7 7 0 0 0 0
 *    8 8 8 8 0 0 0 0
 * 4. ===>> ##第四步不产生miss
 * B: 1 1 1 1 1 1 1 1
 *    2 2 2 2 2 2 2 2
 *    3 3 3 3 3 3 3 3
 *    4 4 4 4 4 4 4 4
 *    5 5 5 5 5 5 5 5
 *    6 6 6 6 6 6 6 6
 *    7 7 7 7 7 7 7 7
 *    8 8 8 8 8 8 8 8
*/
void trans64_64(int M, int N, int A[N][M], int B[M][N]){
    int i, k, s, temp, temp1, temp2, temp3, temp4, temp5, temp6, temp7;
    /* 以8*8block为单位进行操作 */
    for(k=0; k<N; k+=8){
        for(s=0; s<M; s+=8){ 
            /* 第一步 */
            for (i = k; i < k+4; i++) {
                temp=A[i][s];
                temp1=A[i][s+1];  
                temp2=A[i][s+2];
                temp3=A[i][s+3];
                B[s][i]=temp;
                B[s+1][i]=temp1;
                B[s+2][i]=temp2;
                B[s+3][i]=temp3;
            }
            /* 第二步 */
            for (i = k; i < k+4; i++) {
                temp=A[i][s+4];
                temp1=A[i][s+5];  
                temp2=A[i][s+6];
                temp3=A[i][s+7];
                B[s][i+4]=temp;
                B[s+1][i+4]=temp1;
                B[s+2][i+4]=temp2;
                B[s+3][i+4]=temp3;
            }  
            /* 第三步：借助额外的四个临时变量把B右上4*4block转移到左下*/
            for (i = s; i < s+4; i++) {
                temp4=B[i][k+4];
                temp5=B[i][k+5];
                temp6=B[i][k+6];
                temp7=B[i][k+7];
                temp=A[k+4][i];
                temp1=A[k+5][i];  
                temp2=A[k+6][i];
                temp3=A[k+7][i];
                B[i][k+4]=temp;
                B[i][k+5]=temp1;
                B[i][k+6]=temp2;
                B[i][k+7]=temp3;
                B[i+4][k]=temp4;
                B[i+4][k+1]=temp5;
                B[i+4][k+2]=temp6;
                B[i+4][k+3]=temp7;
            } 
            /* 第四步 */
            for (i = s+4; i < s+8; i++) {
                temp=A[k+4][i];
                temp1=A[k+5][i];  
                temp2=A[k+6][i];
                temp3=A[k+7][i];
                B[i][k+4]=temp;
                B[i][k+5]=temp1;
                B[i][k+6]=temp2;
                B[i][k+7]=temp3;
            }  
        } 
    }
}

/*
 * 67*61的矩阵转置， 采用和32*32的矩阵相同的策略
 * 8*8block策略
 * 每个行处理都要处理额外的8*5的block
 * 最后处理多出来的3*61的block，同样是分解为多个3*8block 和一个3*5block
*/
void trans61_67(int M, int N, int A[N][M], int B[M][N]){
    int i, k, s, temp, temp1, temp2, temp3, temp4, temp5, temp6, temp7;
    for(k=0; k<64; k+=8){
        for(s=0; s<56; s+=8){
            /* 处理8*8block */
            for(i=k;i<k+8;i++){
                temp=A[i][s];
                temp1=A[i][s+1];
                temp2=A[i][s+2];
                temp3=A[i][s+3];
                temp4=A[i][s+4];
                temp5=A[i][s+5];
                temp6=A[i][s+6];
                temp7=A[i][s+7];
                B[s][i]=temp;
                B[s+1][i]=temp1;
                B[s+2][i]=temp2;
                B[s+3][i]=temp3;
                B[s+4][i]=temp4;
                B[s+5][i]=temp5;
                B[s+6][i]=temp6;
                B[s+7][i]=temp7;
            }
        }
        /* 处理每个行处理末尾额外的的8*5block */
        for(i=k;i<k+8;i++){
            temp=A[i][s];
            temp1=A[i][s+1];
            temp2=A[i][s+2];
            temp3=A[i][s+3];
            temp4=A[i][s+4];
            B[s][i]=temp;
            B[s+1][i]=temp1;
            B[s+2][i]=temp2;
            B[s+3][i]=temp3;
            B[s+4][i]=temp4;
        }
    }
    /* 处理多出来的3*61block */
        /* 处理多个3*8block */
    for(s=0;s<56;s+=8){
        for(i=64;i<67;i++){
            temp=A[i][s];
            temp1=A[i][s+1];
            temp2=A[i][s+2];
            temp3=A[i][s+3];
            temp4=A[i][s+4];
            temp5=A[i][s+5];
            temp6=A[i][s+6];
            temp7=A[i][s+7];
            B[s][i]=temp;
            B[s+1][i]=temp1;
            B[s+2][i]=temp2;
            B[s+3][i]=temp3;
            B[s+4][i]=temp4;
            B[s+5][i]=temp5;
            B[s+6][i]=temp6;
            B[s+7][i]=temp7;
        }
    }
        /* 处理单个3*5block */
    for(i=64;i<67;i++){
        temp=A[i][56];
        temp1=A[i][57];
        temp2=A[i][58];
        temp3=A[i][59];
        temp4=A[i][60];
        B[56][i]=temp;
        B[57][i]=temp1;
        B[58][i]=temp2;
        B[59][i]=temp3;
        B[60][i]=temp4;
    }
    
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

