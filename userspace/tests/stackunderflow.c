#include "stdio.h"

#define SIZE 4000

int main(){
    printf("init...\n");
    int local;
    int* localpt = &local;
    for (size_t i = 0; i < 20; i++)
    {
        localpt++;
    }
    *localpt = 5;
    
    // int bigarr[SIZE];
    // for (size_t i = 0; i < SIZE; i++)
    // {
    //     bigarr[i] = 6;
    // }
    printf("we should have gotten an underflow... this should never be printed\n");
    return 0;
}