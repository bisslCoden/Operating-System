#include "pthread.h"
#include "stdio.h"
#include "assert.h"
#include "sched.h"

#define NUM_THREADS 200

pthread_mutex_t mutex;
pthread_t tids[NUM_THREADS];

int never_false = 1;


//
int simple_routine()
{
  int mutret = 0;
  printf("hi i ll try to get the mutex!\n");
  mutret = pthread_mutex_lock(&mutex);
  printf("lock returned %d.\n", mutret);
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
  mutret = pthread_mutex_unlock(&mutex);
  printf("mutex UNLOCK returned me %d!\n", mutret);

//  printf("unlocked it!\n");
  return 0;
}

int main()
{
  int ret;
  int rets;
  printf("Hello!\n");
  pthread_mutex_init(&mutex, NULL);
  for (size_t i = 0; i < NUM_THREADS; i++)
  {
    assert(pthread_create(&tids[i], NULL, (void* (*)(void*)) &simple_routine, NULL) == 0 && "couldnt create thread!");
  }
  sched_yield();
   for (size_t i = 0; i < NUM_THREADS; i++)
  {
    ret = pthread_join(tids[i], (void**) &rets);
  }
  printf("%d %d joined all threads successfully and exiting as i should?\n",ret, rets);
  return 0;
}