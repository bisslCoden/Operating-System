#include "pthread.h"
#include "wait.h"
#include <stdio.h>
#include "sched.h"
#include "assert.h"
#include "unistd.h"
#include "semaphore.h"

#define NUM_THREADS1 4
#define NUM_THREADS2 2
#define NUM_THREADS3 1
#define NUM_REC_FORKS 1
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
      printf(" ");
    }
    else 
    {
      printf("[recursive_routine] forking\n");
      fork();
      return recursive_routine(count-1); 
    }
    return 99999;
}

size_t large_routine(args2* params)
{
    int arr[ARRSIZE];
    pid_t pid = fork();
    for (size_t i = 0; i < ARRSIZE; i++)
    {
        arr[i] = 78;
    }
    if (pid != 0)
    {
        printf("[largeroutine] waiting for my child!\n");
        pid = waitpid(pid, 0, 0);
    }
    printf("[largeroutine] returning...\n");
    return arr[ARRSIZE -1];
}

void detached_routine()
{
    pthread_detach(pthread_self());
    for (size_t i = NUM_THREADS1 + NUM_THREADS2; i < NUM_THREADS1 + NUM_THREADS2 + NUM_THREADS3; i++)
    {
        assert(pthread_create(&tids[i], NULL, (void* (*)(void*))&recursive_routine, (void*) NUM_REC_FORKS) == 0 && "couldnt create anymore threads daaamn!\n");
    }
    for (size_t i = NUM_THREADS1 + NUM_THREADS2; i < NUM_THREADS1 + NUM_THREADS2 + NUM_THREADS3; i++)
    {
        assert(pthread_join(tids[i], (void**) &returnvalues[i]) == 0 && "couldnt join my child daaamn!\n");
        printf("[detached_routine] joined kid %ld and got val %d\n", i, returnvalues[i]);
    }
    return;
}





int main()
{
  printf("[main] starting pfork now...\n");
  pid_t child = fork();

  //inits... could be left out or fed with wrong adresses also
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

  int ret = 0;
  for(size_t i = 0; i < NUM_THREADS1 + NUM_THREADS2; ++i){
    ret = pthread_join(tids[i], (void**) &returnvalues[i]);
    if (ret != 0)
    {
      printf("[main]: could not join [%ld]... %s\n", i, (i >= NUM_THREADS1) ? "thats okay he detached" : "which is NOT okay!");
      returnvalues[i] = -1;
    }
    else
        printf("[main] joined [%ld] got %d\n", i , returnvalues[i]);
  }

  if (child != 0)
  {
    printf("[main] waiting for little other main to finish\n");
    child = waitpid(child, 0, 0);
  }

  printf("[main]: successfully exiting the programm!\n");
  return 0;
}