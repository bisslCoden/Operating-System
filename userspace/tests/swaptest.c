#include "stdlib.h"
#include "stdio.h"

#define PAGES 1000


int main()
{
  printf("starting now...\n");
  char* large_array = (char*) malloc(PAGES * 4096);
  size_t index = 0;
  for (size_t i = 0; i < PAGES; i++)
  {
    large_array[index] = 1;
    index += 4096;
  }
  printf("successful exit now...\n");
  return 0;
}