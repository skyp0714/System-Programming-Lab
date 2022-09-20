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

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
 // 8*8 block transpose function for 32*32 matrix
 // used local variables: 2(inherited) + 3 = 5
void a_transpose(int i, int j,int M, int N, int A[N][M], int B[M][N]){
    int row, col, tmp1;
    for(row = i; row<i+8; row++){
        for(col = j; col<j+8; col++){
            if(row!=col)    B[col][row] = A[row][col];
            else    tmp1 = A[row][col];
        }
        if(i == j)  B[row][row] = tmp1;
    }
}
// 4*4 block transpose function for 64*64 matrix
// used local variables: 2(inherited) + 8 = 10
void block_transpose(int row, int col,int M, int N, int A[N][M], int B[M][N]){
    int tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;
    tmp1 = A[row][col];
    tmp2 = A[row+1][col];
    tmp3 = A[row+2][col];
    tmp4 = A[row+3][col];
    
    tmp5 = A[row][col+1];
    tmp6 = A[row][col+2];
    
    tmp7 = A[row+3][col+1];
    tmp8 = A[row+3][col+2];
    
    B[col+3][row+3] = A[row+3][col+3];
    B[col+3][row+2] = A[row+2][col+3];
    B[col+3][row+1] = A[row+1][col+3];
    B[col+3][row] = A[row][col+3];

    B[col][row] = tmp1;
    B[col][row+1] = tmp2;
    B[col][row+2] = tmp3;
    B[col][row+3] = tmp4;
    
    tmp1 = A[row+1][col+1];
    tmp2 = A[row+1][col+2];

    tmp3 = A[row+2][col+1];
    tmp4 = A[row+2][col+2];

    B[col+1][row] = tmp5;
    B[col+1][row+1] = tmp1;
    B[col+1][row+2] = tmp3;
    B[col+1][row+3] = tmp7;

    B[col+2][row] = tmp6;
    B[col+2][row+1] = tmp2;
    B[col+2][row+2] = tmp4;
    B[col+2][row+3] = tmp8;
}
// 8*8 block transpose function for 61*67 matrix
 // used local variables: 2(inherited) + 5 = 7
void c_transpose(int i, int j,int M, int N, int A[N][M], int B[M][N]){
    int row, tmp1, tmp2, tmp3, tmp4;
    for(row=i;row<i+8 && row<N;row++){
        tmp1 = A[row][j];
        tmp2 = A[row][j+1];
        tmp3 = A[row][j+2];
        tmp4 = A[row][j+3];
        B[j][row] = tmp1;
        B[j+1][row] = tmp2;
        B[j+2][row] = tmp3;
        B[j+3][row] = tmp4;
    }
    for(row=i;row<i+8 && row<N;row++){
        tmp1 = A[row][j+4];
        if(j+5 < M) tmp2 = A[row][j+5];
        if(j+5 < M) tmp3 = A[row][j+6];
        if(j+5 < M) tmp4 = A[row][j+7];
        B[j+4][row] = tmp1;
        if(j+5 < M) B[j+5][row] = tmp2;
        if(j+5 < M) B[j+6][row] = tmp3;
        if(j+5 < M) B[j+7][row] = tmp4;
    }
}
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;
    if(N==32){
        for (i = 0; i < N; i=i+8) {
            for (j = 0; j < M; j=j+8) {
                a_transpose(i,j,M,N,A,B);
            }
        }    
    }else if(N==64){
        for (i = 0; i < N; i=i+8) {
            for (j = 0; j < M; j=j+8) {
                block_transpose(i, j+4, M, N, A, B);
                block_transpose(i, j, M, N, A, B);
                block_transpose(i+4, j, M, N, A, B);
                block_transpose(i+4, j+4, M, N, A, B);
            }
        }   
    }else{
        for (i = 0; i < N; i=i+8) {
            for (j = 0; j < M; j=j+8) {
                c_transpose(i,j,M,N,A,B);
            }
        }    
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

