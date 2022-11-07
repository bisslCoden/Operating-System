#include "pthread.h"
#include <stdio.h>
#include "sched.h"
#include "assert.h"
#include "unistd.h"
#include "semaphore.h"

#define NUM_THREADS1 4
#define NUM_THREADS2 10
#define NUM_THREADS3 8
#define MAX_RES 5000
#define MATSIZE 2000

pthread_t tids[NUM_THREADS1 + NUM_THREADS2 + NUM_THREADS3];
int returnvalues [NUM_THREADS1 + NUM_THREADS2 + NUM_THREADS3];
size_t never_false = 1;
size_t result = 0;
size_t threads3 = NUM_THREADS3;

pthread_mutex_t never_false_lock;
pthread_mutex_t result_lock;
pthread_mutex_t threads3_lock;
pthread_mutex_t CV_mutex;
pthread_cond_t condition;
sem_t semy;

typedef struct args2
{
    size_t a;
    size_t b;
    float com;
    size_t* xy;
}args2;

//NOTE: what to do if gets cancelled async and has lock 

size_t simple_routine3(void* count){
    if ((size_t)count <= 0)
    {
      printf("exiting simple_routine3\n");
      pthread_cond_signal(&condition);
      assert(pthread_cancel(pthread_self()) == 0 && "couldnt cancel myself!");
      //leaving this out should make cancel stuff disappear
      printf("this print is just for cancel point and should not get through...\n");
    }
    else 
    {
      sched_yield();
      return simple_routine3(count-1); 
    }
    return 99999;
}

size_t simple_routine2(args2* params)
{
  printf("entering simple_routine2\n");
  int old;
  pthread_t mychild = 0;
  //int mat[MATSIZE][MATSIZE];
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old);
  
  sem_wait(&semy);
  sched_yield();
  assert(never_false == 1 && "never false false!");
  assert(pthread_cancel(pthread_self()) != 0 && "how could i cancel myself?\n");
  
  pthread_mutex_lock(&never_false_lock);
  never_false = 0;
  // for (int i = 0; i < MATSIZE; i++)
  // {
  //   for (int j = 0; j < MATSIZE; j++)
  //   {
  //     mat[i][j] = ((params->a) * (params->b + i)) % MATSIZE;
  //   }
  // }
  pthread_mutex_lock(&threads3_lock);
  printf("after matfill\n");
  if (*params->xy > 0)
  {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    assert(pthread_create(&tids[NUM_THREADS1 + NUM_THREADS2 + *params->xy -1], &attr, (void*(*)(void*))&simple_routine3, 
    (void*) 10) == 0 && "could not vreate thred");
    mychild = tids[NUM_THREADS1 + NUM_THREADS2 + *params->xy -1];
    *(params->xy) =  *(params->xy) - 1;
  }
  pthread_mutex_unlock(&threads3_lock);
  
  pthread_mutex_lock(&result_lock);
  
  // for (int i = 0; i < MATSIZE; i++)
  // {
  //   for (int j = 0; j < MATSIZE; j++)
  //   {
  //     if(mat[i][j] < (MATSIZE / 10))
  //     {
  //       if(result + mat[i][j] < MAX_RES)
  //         result += mat[i][j];
  //     }
  //   }
  // }

  pthread_mutex_unlock(&result_lock);
  never_false = 1;
  pthread_mutex_unlock(&never_false_lock);
  
  //waiting for detached child to signal
  pthread_mutex_lock(&CV_mutex);
  pthread_cond_wait(&condition, &CV_mutex);
  pthread_mutex_unlock(&CV_mutex);
  
  long ret = 0;
  if(mychild != 0)
    assert(pthread_join(mychild, (void**) ret)  != 0 && "could join even though shouldnt!");
  sem_post(&semy);
  return (size_t)pthread_self();
}

void simple_routine()
{
  printf("entering simple_routine1\n");
  int bef;
  pthread_detach(pthread_self());
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &bef);
  pthread_mutex_lock(&result_lock);
  sched_yield();
  for (size_t i = 0; i < 10; i++)
  {
    assert(never_false == 1 && "never false false!");
    pthread_mutex_lock(&never_false_lock);
    never_false = 0;
    if (result + 1 < MAX_RES)
      result++;
    
    sched_yield();
    never_false = 1;
    pthread_mutex_unlock(&never_false_lock);
  }
  pthread_mutex_unlock(&result_lock);
  
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  long ret = 0;
  
  pthread_mutex_lock(&threads3_lock);
  assert(pthread_create(&tids[NUM_THREADS1 + NUM_THREADS2 + threads3 - 1], &attr, (void*(*)(void*))&simple_routine3, 
  (void*) 20) == 0 && "could not create thred");
  pthread_t mychild = tids[NUM_THREADS1 + NUM_THREADS2 + threads3 - 1];
  threads3--;
  pthread_mutex_unlock(&threads3_lock);
  assert(pthread_join(mychild, (void**) ret)  == 0 && "couldnt join?");
  if ((void*) ret == PTHREAD_CANCELED)
    printf("my child got canceled %ld!\n", ret);
  
  pthread_exit((void*) pthread_self);
}





int main()
{
  printf("starting bigtest now...\n");

  //inits... could be left out or fed with wrong adresses also
  pthread_mutex_init(&never_false_lock, 0);
  pthread_mutex_init(&result_lock, 0);
  pthread_mutex_init(&threads3_lock, 0);
  pthread_mutex_init(&CV_mutex, 0);
  pthread_cond_init(&condition, 0);
  sem_init(&semy, 0, 3);
  args2 paramsnow = {0, 0, 0.04f, &threads3};

  for(size_t i = NUM_THREADS1; i < NUM_THREADS2 + NUM_THREADS1; ++i){
    paramsnow.a = i;
    paramsnow.b = i + 2;
    printf("now creating 2\n");
    assert(pthread_create(&tids[i], NULL, (void* (*)(void*))&simple_routine2, (void*) &paramsnow) == 0 && "couldnt create anymore threads daaamn!\n");
  }

  for(size_t i = 0; i < NUM_THREADS1; ++i){
    assert(pthread_create(&tids[i], NULL, (void* (*)(void*))&simple_routine, NULL) == 0 && "couldnt create anymore threads daaamn!\n");
  }

  sched_yield();
  sched_yield();

  for(size_t i = NUM_THREADS1; i < NUM_THREADS1 + NUM_THREADS2; ++i){
    assert(pthread_cancel(tids[i]) != -1 && "was able to cancel a disabled thread!");
  } 
  sched_yield();

  int ret = -10;
  for(size_t i = 0; i < NUM_THREADS1 + NUM_THREADS2; ++i){
    ret = pthread_join(tids[i], (void**) &returnvalues[i]);
    if (ret != 0)
    {
      printf("could not join %ld\n", i);
      returnvalues[i] = -20;
    }
  }
 
  int curretval = 0;
  for(size_t i = 0; i < NUM_THREADS1 + NUM_THREADS2; ++i){
    if (returnvalues[i] != curretval)
    {
        printf("new retval starts at index [%ld] and now has value %d\n", i, returnvalues[i]);
        curretval = returnvalues[i];
    }
  }
  printf("successfully exiting the programm!\n");
  return 0;
}