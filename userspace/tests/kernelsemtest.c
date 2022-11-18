#include "stdio.h"
#include "semaphore.h"
#include "assert.h"
#include "sched.h"

#define NUM_THREADS 25

pthread_t tids[NUM_THREADS]; 

int counter = 0;
int never_false = 1;


//
int simple_routine()
{
  kernelsem_wait();
  assert(never_false == 1 && "never false aint 1? whaat?\n");
  never_false = 0;
  sched_yield();
  never_false = 1;
 
  // for (size_t i = 0; i < 4000000; i++)
  // {
  //   int res = (i + i+1) % 365;
  //   if((counter + res) < 20000000)
  //       counter += res;
  // }
  kernelsem_post();
  return 0;
}

int main()
{
  int ret;  
  int rets;
  printf("Hello!\n");
  kernelsem_post();
  for (size_t i = 0; i < NUM_THREADS; i++)
  {
    assert((ret = pthread_create(&tids[i], NULL, (void* (*)(void*)) &simple_routine, NULL)) == 0 && 
    "something went wrong in thread creation\n");
  }

  for (size_t i = 0; i < NUM_THREADS; i++)
  {
       assert((ret = pthread_join(tids[i], (void**) &rets)) == 0 && 
    "something went wrong in thread joining\n");
  }
  printf("joined all threads successfully... exiting\n");
  return 0;
}