#include "stdio.h"
#include "pthread.h"
#include "sched.h"

#define SIZE 800000


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
//0x70148c772ff0
//0x70148c465bc0
//0x70148c762ff8