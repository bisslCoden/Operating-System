#include "pthread.h"
#include <stdio.h>
#include "sched.h"
#include "assert.h"
#include "unistd.h"
#include "semaphore.h"

#define NUM_THREADS1 4
#define NUM_THREADS2 10
#define NUM_THREADS3 8
#define ARRSIZE 16000

pthread_t tids[NUM_THREADS1 + NUM_THREADS2 + NUM_THREADS3];
int returnvalues [NUM_THREADS1 + NUM_THREADS2 + NUM_THREADS3];
size_t never_false = 1;
size_t threads3 = NUM_THREADS3;

pthread_mutex_t never_false_lock;
pthread_mutex_t threads3_lock;


typedef struct args2
{
    size_t a;
    size_t b;
    float com;
    size_t* xy;
}args2;

//NOTE: what to do if gets cancelled async and has lock 

size_t recursive_routine(void* count){
    if ((size_t)count <= 0)
    {
      printf("exiting recursive_routine\n");
      assert(pthread_cancel(pthread_self()) == 0 && "couldnt cancel myself!");
      //leaving this out should make cancel stuff disappear
      printf("this print is just for cancel point and should not get through...\n");
    }
    else 
    {
      sched_yield();
      return recursive_routine(count-1); 
    }
    return 99999;
}

size_t large_routine(args2* params)
{
  printf("entering large_routine\n");
  int old;
  pthread_t mychild = 0;
  int bigarr[ARRSIZE];
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old);
  pthread_cancel(pthread_self());
  
  
  pthread_mutex_lock(&never_false_lock);
  assert(never_false == 1 && "never false false!");
  never_false = 0;
  for (int j = 0; j < ARRSIZE; j++)
  {
    bigarr[j] = ((params->a) * (params->b + j)) % ARRSIZE;
  }
  sched_yield();
  pthread_mutex_lock(&threads3_lock);
  if (*params->xy > 0)
  {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    assert(pthread_create(&tids[NUM_THREADS1 + NUM_THREADS2 + *(params->xy) -1], &attr, (void*(*)(void*))&recursive_routine, 
    (void*) 10) == 0 && "could not create thred");
    mychild = tids[NUM_THREADS1 + NUM_THREADS2 + *(params->xy) -1];
    *(params->xy) =  *(params->xy) - 1;
  }
  pthread_mutex_unlock(&threads3_lock);
  never_false = 1;
  pthread_mutex_unlock(&never_false_lock);
  
  long ret = 0;
  if(mychild != 0)
    assert(pthread_join(mychild, (void**) ret)  != 0 && "could join even though shouldnt!");
  printf("large: %ld exiting... bigarr at my val is: %d\n", (size_t)pthread_self(), bigarr[params->a]);
  return pthread_self();
}

void detached_routine()
{
  printf("entering detached_routine\n");
  pthread_detach(pthread_self());
  int bef;
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &bef);

  pthread_mutex_lock(&threads3_lock);
  sched_yield();
  pthread_mutex_lock(&never_false_lock);
  assert(never_false == 1 && "never false false!");
  never_false = 0;

  sched_yield();
  
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  assert(pthread_create(&tids[NUM_THREADS1 + NUM_THREADS2 + threads3 - 1], &attr, (void*(*)(void*))&recursive_routine, 
  (void*) 20) == 0 && "could not create thred");
  pthread_t mychild = tids[NUM_THREADS1 + NUM_THREADS2 + threads3 - 1];
  threads3--;

  never_false = 1;
  pthread_mutex_unlock(&threads3_lock);
  pthread_mutex_unlock(&never_false_lock);
  
  long ret = 0;
  assert(pthread_join(mychild, (void**) ret)  == 0 && "couldnt join?");
  assert((void*) ret == PTHREAD_CANCELED && "couldnt cancel itself?");
  
  printf("detached %ld exiting...", pthread_self());
  pthread_exit((void*) pthread_self);
}





int main()
{
  printf("[main] starting bigtest now...\n");

  //inits... could be left out or fed with wrong adresses also
  pthread_mutex_init(&never_false_lock, 0);
  pthread_mutex_init(&threads3_lock, 0);
  args2 paramsnow = {0, 0, 0.04f, &threads3};

  for(size_t i = 0; i < NUM_THREADS1; ++i){
    paramsnow.a = i;
    paramsnow.b = i + 2;
    assert(pthread_create(&tids[i], NULL, (void* (*)(void*))&large_routine, (void*) &paramsnow) == 0 && "couldnt create anymore threads daaamn!\n");
  }

  for(size_t i = NUM_THREADS1; i < NUM_THREADS1 + NUM_THREADS2; ++i){
    assert(pthread_create(&tids[i], NULL, (void* (*)(void*))&detached_routine, NULL) == 0 && "couldnt create anymore threads daaamn!\n");
  }

  sched_yield();
  sched_yield();
  sched_yield();


  printf("[main]:now trying to cancel everyone!\n");
  for(size_t i = 0; i < NUM_THREADS1 + NUM_THREADS2; ++i){
    pthread_cancel(tids[i]);
  } 
  sched_yield();

  int ret = -10;
  for(size_t i = 0; i < NUM_THREADS1 + NUM_THREADS2; ++i){
    ret = pthread_join(tids[i], (void**) &returnvalues[i]);
    if (ret != 0)
    {
      printf("[main]: could not join %ld\n", i);
      returnvalues[i] = -20;
    }
  }
 
  int curretval = 0;
  for(size_t i = 0; i < NUM_THREADS1 + NUM_THREADS2; ++i){
    if (returnvalues[i] != curretval)
    {
        printf("[main]: new retval starts at index [%ld] and now has value %d\n", i, returnvalues[i]);
        curretval = returnvalues[i];
    }
  }
  printf("[main]: successfully exiting the programm!\n");
  return 0;
}