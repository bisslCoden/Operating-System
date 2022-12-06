#include "stdio.h"
#include "unistd.h"
#include "sched.h"

int data_init = 17;
const char* yey = "yes!"; 

int main()
{
    printf("hey! i am dedup proc\n");
    sleep(10);
    printf("%d %s\n", data_init, yey);
    return 0;
}