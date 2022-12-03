#include "stdio.h"
#include "unistd.h"

int data_init = 17;
const char* yey = "yes!"; 

int main()
{
    sleep(10);
    printf("%d %s\n", data_init, yey);
    return 0;
}