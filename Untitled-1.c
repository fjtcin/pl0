#include<stdio.h>

# define N 2
# define M 2

int main() {
    int i, j, k;
    int g[][2][2] = {{0},{1},2,3,{4,5},{6},{7},8,{9}};
    for (i = 0; i < sizeof(g)/4/M/N; i++)
        for(j = 0; j < M; j++)
            for(k = 0; k < N; k++)
            {
                if(g[i][j][k] != 0)
                    printf("g[%d][%d][%d]: %d\n",i,j,k,g[i][j][k]);
            }
}