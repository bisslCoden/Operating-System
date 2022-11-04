#include "stdio.h"

#define SIZE 16000

int main(){
    printf("init...\n");
    int res = 0;
    int bigarr[SIZE];
    for (size_t i = 0; i < SIZE; i++)
    {
        bigarr[i] = i;
    }
    res += bigarr[78];
    printf("bigarr filled byebye...\n");
    return res;
}