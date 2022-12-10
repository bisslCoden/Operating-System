#include "pthread.h"
#include "stdio.h"
#include "assert.h"
#include "sched.h"

#define NUM_THREADS1 10
#define NUM_THREADS2 10000


pthread_mutex_t mutexes [NUM_THREADS1];

pthread_t tids[NUM_THREADS1 + NUM_THREADS2]; 

int never_false = 1;


void normalthread()
{
    int result = 1;
    for (size_t i = 0; i < 100000; i++)
    {
        result *= i;
    }
    printf("%d\n", result);
    return;
}

//
int chainroutine(int my_index)
{
  //int mutret = 0;
  //printf("hi i ll try to get the mutex!\n");
  //mutret = 
  pthread_mutex_lock(&mutexes[my_index]);
  sched_yield();
  sched_yield();
  sched_yield();

  pthread_mutex_lock(&mutexes[my_index -1]);
  sched_yield();
  sched_yield();
  sched_yield();
  pthread_mutex_unlock(&mutexes[my_index]);
  pthread_mutex_unlock(&mutexes[my_index -1]);

  return 0;
}

//
int initroutine(){
    int ret;
    printf("[init] starting...\n");
    for (size_t i = 0; i < NUM_THREADS1; i++)
        pthread_mutex_init(&mutexes[i], NULL);
    
    pthread_mutex_lock(&mutexes[0]);    
    for (size_t i = 1; i < NUM_THREADS1; i++)
        assert(ret = pthread_create(&tids[i], NULL, (void* (*)(void*)) &chainroutine, (void*)i) == 0);
    
    sched_yield();
    sched_yield();
    sched_yield();

    pthread_mutex_lock(&mutexes[NUM_THREADS1 -1]);
    sched_yield();
    sched_yield();
    sched_yield();

    pthread_mutex_unlock(&mutexes[0]);    
    pthread_mutex_unlock(&mutexes[NUM_THREADS1 -1]);

    printf("[init] returning...\n");
    return 0;
}

int main()
{
    int ret;
    int rets;
    // printf("Hello!\n");

    assert(ret = pthread_create(&tids[0], NULL, (void* (*)(void*)) &initroutine, NULL) == 0);
    for (size_t i = NUM_THREADS1; i < NUM_THREADS2 + NUM_THREADS2; i++)
        assert(ret = pthread_create(&tids[i], NULL, (void* (*)(void*)) &normalthread, NULL) == 0);
    
    sched_yield();

    for (size_t i = 0; i < NUM_THREADS1 + NUM_THREADS2; i++)
    {
        assert(ret = pthread_join(tids[i], (void**) &rets) == 0);
    }
    printf("[main] joined all threads (there should have been at least 1 deadlock)\n");
    return 0;
}