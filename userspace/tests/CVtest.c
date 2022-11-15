#include "pthread.h"
#include "stdio.h"
#include "assert.h"
#include "sched.h"

#define NUM_THREADS 5

pthread_mutex_t mutex;
pthread_cond_t condition;
pthread_t tids[NUM_THREADS]; 

int counter = 0;
int never_false = 1;


//
int simple_routine()
{
  int condret = 0;
  assert(pthread_mutex_lock(&mutex) == 0 && "this seems to be a mutex error!\n");
  condret = pthread_cond_wait(&condition, &mutex);
  printf("cond returned %d.\n", condret);
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
  pthread_mutex_unlock(&mutex);
  condret = pthread_cond_signal(&condition);
  printf("condret returned me %d. Bye Bye!\n", condret);
  return 0;
}

int main()
{
  int ret;  
  int rets;
  printf("Hello!\n");
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&condition, NULL);
  for (size_t i = 0; i < NUM_THREADS; i++)
  {
    assert((ret = pthread_create(&tids[i], NULL, (void* (*)(void*)) &simple_routine, NULL)) == 0 && 
    "something went wrong in thread creation\n");
  }
  sched_yield();
  sched_yield();

  printf("until now there should be no prints...\n");
  pthread_cond_signal(&condition);
  for (size_t i = 0; i < NUM_THREADS; i++)
  {
       assert((ret = pthread_join(tids[i], (void**) &rets)) == 0 && 
    "something went wrong in thread joining\n");
  }
  printf("joined all threads successfully... exiting\n");
  return 0;
}