#include "pthread.h"
#include <stdio.h>
#include "sched.h"
#include "assert.h"
#include "unistd.h"
#include "semaphore.h"

#define NUM_THREADS1 100
#define NUM_THREADS2 100
#define NUM_THREADS3 4
#define MAX_RES 5000
#define MATSIZE 2000

pthread_t tids[NUM_THREADS1 + NUM_THREADS2 + NUM_THREADS3];
int returnvalues [NUM_THREADS1 + NUM_THREADS2 + NUM_THREADS3];
size_t never_false = 1;
size_t result = 0;

pthread_mutex_t never_false_lock;
pthread_mutex_t result_lock;
pthread_mutex_t CV_mutex;
pthread_cond_t condition;
sem_t semy;

typedef struct args2
{
    int a;
    size_t b;
    float com;
    int* xy;
}args2;

//NOTE: what to do if gets cancelled async and has lock 
void simple_routine()
{
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
  pthread_mutex_lock(&CV_mutex);
  pthread_cond_wait(&condition, &CV_mutex);
  pthread_mutex_unlock(&CV_mutex);
  return;
}


size_t simple_routine2(args2* params)
{
  int old;
  int mat[MATSIZE][MATSIZE];
  sem_wait(&semy);
  sched_yield();
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old);
  assert(never_false == 1 && "never false false!");
  pthread_mutex_lock(&never_false_lock);
  never_false = 0;
  for (int i = 0; i < MATSIZE; i++)
  {
    for (int j = 0; j < MATSIZE; i++)
    {
      mat[i][j] = ((params->a) * (params->b + i)) % MATSIZE;
    }
  }
  pthread_mutex_lock(&result_lock);
  
  for (int i = 0; i < MATSIZE; i++)
  {
    for (int j = 0; j < MATSIZE; i++)
    {
      if(mat[i][j] < (MATSIZE / 10))
      {
        if(result + mat[i][j] < MAX_RES)
          result += mat[i][j];
      }
    }
  }
  pthread_mutex_unlock(&result_lock);
  never_false = 1;
  pthread_mutex_unlock(&never_false_lock);
  sem_post(&semy);
}

int simple_routine3(int count){
    if (count <= 0)
    {
        return;
    }
    else return simple_routine3(count-1); 
    
}



int main()
{
  printf("starting bigtest now...\n");
  //int ret = pthread_create(&tid, NULL, (void*(*)(void*))++i&simple_routine, NULL);
  int ret = 0;
  int four = 4;
  args2 paramsnow = {4, 4, 0.04f, &four};
  for(size_t i = 0; i < NUM_THREADS1; ++i){
    assert((ret = pthread_create(&tids[i], NULL, (void* (*)(void*))&simple_routine, NULL)) == 0 && "couldnt create anymore threads daaamn!\n");
  }
  for(size_t i = NUM_THREADS1; i < NUM_THREADS2 + NUM_THREADS1; ++i){
    assert((ret = pthread_create(&tids[i], NULL, (void* (*)(void*))&simple_routine2, (void*) &paramsnow)) == 0 && "couldnt create anymore threads daaamn!\n");
  }
  sched_yield();
 // int busy = 0;
  //int long_ = 0;
//   while (long_ < 4000000000000)
//   {
//     busy += long_;
//   
  int cury = -10;
  for(size_t i = 0; i < NUM_THREADS1 + NUM_THREADS2; ++i){
    ret = pthread_cancel(tids[i]);
    if (ret != cury)
    {
        printf("MAIN: cancel for thread [%ld] cancel returned me: %d\n", tids[i], ret);
        cury = ret;
    }
  } 
  sched_yield();

  for(size_t i = 0; i < NUM_THREADS1 + NUM_THREADS2; ++i){
    assert((ret = pthread_join(tids[i], (void**) &returnvalues[i])) == 0 && "join was not successful? this shouldnt happen\n");
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