#include "pthread.h"
#include "stdio.h"
#include "assert.h"

#define NUM_THREADS 1
#define OFFSET 8000

pthread_t tids[NUM_THREADS];

void PageRoutine(size_t main_adress)
{
    main_adress -= OFFSET;
    size_t* segfault = (size_t*) main_adress;
    *segfault = 999;
    printf("I wrote 999 at adress %p\n", segfault);
    return;   
}
//0x7dee4f99efe8
//0x7dee4f99d074
//0x7dee4f98eff8
int main()
{
  int ret;
  printf("1 - main: Hello!");
  //int ret = pthread_create(&tid, NULL, (void*(*)(void*))&simple_routine, NULL);
  for (size_t i = 0; i < NUM_THREADS; i++)
  {
    assert((ret = pthread_create(&tids[i], NULL, (void*)&PageRoutine, (void*) &ret)) == 0);
  }
  for (size_t i = 0; i < NUM_THREADS; i++)
  {
    assert((ret = pthread_join(tids[i], NULL)) == 0);
  }
  size_t addr = ((size_t)&ret) - OFFSET;
  printf("3 - main again: i did read %ld from %p\n", *((size_t*)addr), (size_t*) addr);
  return 0;
}