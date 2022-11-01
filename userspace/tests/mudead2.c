#include "pthread.h"
#include "stdio.h"
#include "assert.h"
#include "sched.h"

#define NUM_THREADS1 2
#define NUM_THREADS2 2


pthread_mutex_t mutex1;
pthread_mutex_t mutex2;

pthread_t tids[NUM_THREADS1 + NUM_THREADS2]; 

int never_false = 1;
int never_false2 = 1;


//
int simple_routine()
{
  //int mutret = 0;
  //printf("hi i ll try to get the mutex!\n");
  //mutret = 
  pthread_mutex_lock(&mutex1);
  sched_yield();
  //printf("lock1 returned %d.\n", mutret);
  //mutret =
  pthread_mutex_lock(&mutex2);
  //printf("lock2 returned %d\n", mutret);
  //assert(never_false == 1 && "never false aint 1? whaat?\n");
  never_false = 0;
  never_false2 = 0;
  sched_yield();
  never_false = 1;
  never_false2 = 1;
  //mutret =
   pthread_mutex_unlock(&mutex1);
  //mutret = 
  pthread_mutex_unlock(&mutex2);
  //printf("mutex UNLOCK returned me %d!\n", mutret);

//  printf("unlocked it!\n");
  return 0;
}

//
int simple_routine2()
{
  int mutret = 0;
  //printf("hi i ll try to get the mutex!\n");
  mutret = pthread_mutex_lock(&mutex2);
  sched_yield();
  //printf("lock1 returned %d.\n", mutret);
  mutret = pthread_mutex_lock(&mutex1);
  //printf("lock2 returned %d\n", mutret);
  //assert(never_false == 1 && "never false aint 1? whaat?\n");
  never_false = 0;
  never_false2 = 0;
  sched_yield();
  never_false = 1;
  never_false2 = 1;
  mutret = pthread_mutex_unlock(&mutex2);
  mutret = pthread_mutex_unlock(&mutex1);
  printf("mutex UNLOCK returned me %d!\n", mutret);
  return 0;
}

int main()
{
  int ret;
  int rets;
  printf("Hello!\n");
  pthread_mutex_init(&mutex1, NULL);
  pthread_mutex_init(&mutex2, NULL);

  for (size_t i = 0; i < NUM_THREADS1; i++)
  {
    assert(ret = pthread_create(&tids[i], NULL, (void* (*)(void*)) &simple_routine, NULL) == 0);
  }
  for (size_t i = NUM_THREADS1; i < NUM_THREADS1 + NUM_THREADS2; i++)
  {
    assert(ret = pthread_create(&tids[i], NULL, (void* (*)(void*)) &simple_routine2, NULL) == 0);
  }
  sched_yield();
   for (size_t i = 0; i < NUM_THREADS1 + NUM_THREADS2; i++)
  {
    assert(ret = pthread_join(tids[i], (void**) &rets) == 0);
  }
  printf("successfully exiting... hmmm -.-\n");
  return 0;
}