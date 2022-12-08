#include "stdio.h"
#include "unistd.h"
#include "sched.h"

int data_init = 17;
const char* yey = "yes!"; 

int main()
{
    printf("hey! i am dedup proc\n");
    for (size_t i = 0; i < 40; i++)
    {
        sched_yield();
    }
    printf("%d %s\n", data_init, yey);
    return 0;
}