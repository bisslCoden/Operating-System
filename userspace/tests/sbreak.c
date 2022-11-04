#include <stdio.h>

int global[2000] = {5};
int global2[2000];
int global3 = 8;

int main()
{
   // int stackvar;
    printf("global init is at: %p, uninit at %p, globalinit2: %p\n", &global, &global2, &global3);
    for (size_t i = 0; i < 2000; i++)
    {
        global2[i] = 12;
    }
    printf("global init is at: %p, uninit begins at %p, ends at %p;  globalinit2: %p\n", &global, &global2, &global2[1999], &global3);

    
    return 0;
}