#include "pthread.h"
#include "stdio.h"

#define PTHREAD_CALLS 100000
  pthread_t tid[PTHREAD_CALLS];
  size_t values[PTHREAD_CALLS];

void* subroutine(void* args)
{
  printf("\tTID: [%ld] called subroutine(): with arg %ld\n", (size_t)1234, *((size_t*)args));
  return 0;
} 

int main()
{
  printf("main(): seawas. will call a fuckload of threads.\n");

  for(size_t i = 0; i < PTHREAD_CALLS; i++)
  {
    values[i] = i;
  }

  // pthread calls
  for(size_t i = 0; i < PTHREAD_CALLS; i++)
  {
    printf("directly before pthread call i = %ld\n", i);
    assert(pthread_create(tid + i, 0,  &subroutine, (values + i)) == 0);
  }

  return 69;
}