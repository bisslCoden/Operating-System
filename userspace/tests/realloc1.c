#include "stdlib.h"
#include "stdio.h"

#define NUM_DATA 5

int main()
{
  printf("starting realloc test 1\n");
  void* data [NUM_DATA];
  for (size_t i = 0; i < NUM_DATA; i++)
  {
    data[i] = malloc((i * 10000) - i *90 + 7);
    printf("pointer [%ld] - %p\n", i, data[i]);
  }
  
  data[3] = realloc(data[3], 2500);
  data[1] = realloc(data[1], 0);
  data[0] = realloc(data[0], 9914);
  data[4] = realloc(data[4], 50000);
  data[2] = realloc(data[2], 70000);

  for (size_t i = 0; i < NUM_DATA; i++)
  {
    if (data[i] == NULL)
    {
      data[i] = realloc(data[i], 14000);
      printf("pointer [%ld] was NULL so i got new mem at: %p\n", i, data[i]);
    }
    printf("pointer [%ld] - %p\n", i, data[i]);
    free(data[i]);
  }
  void* invalid = (void*) 0x56639029;
  printf("finally invalid realloc: returns %p\n", realloc(invalid, 2345553));
  printf("sucessfully exiting program!\n");

}