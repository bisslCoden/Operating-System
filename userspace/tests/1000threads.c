#include "pthread.h"
#include "stdio.h"
#include "assert.h"

#define NUM_THREADS 1500
#define ARR_SIZE 4500

pthread_t tids[NUM_THREADS];

int PageRoutine()
{
    int arr[ARR_SIZE];
    for (size_t i = 0; i < ARR_SIZE; i++)
    {
        arr[i]  = 90;
    }
    return arr[ARR_SIZE];   
}

int main()
{
    int ret;
  printf("1 - main: Hello!");
  //int ret = pthread_create(&tid, NULL, (void*(*)(void*))&simple_routine, NULL);
  for (size_t i = 0; i < NUM_THREADS; i++)
  {
    assert(ret = pthread_create(&tids[i], NULL, (void*)&PageRoutine, NULL) == 0);
    printf("current no: %ld\n", i);
  }
  for (size_t i = 0; i < NUM_THREADS; i++)
  {
    assert(ret = pthread_join(tids[i], NULL) == 0);
  }

  printf("3 - main again: exit...\n");
  return 0;
}