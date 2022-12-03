#include <stdio.h>
#include "pthread.h"
#include "unistd.h"
#include "sched.h"

#define NUM_THREADS 6
#define WITHOUT_MAIN 1

pthread_t tids[NUM_THREADS];

int deadroutine(pthread_t tid)
{
    size_t old;
    int ret = 0;
   // pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old);
    printf("[%ld]trying to join [%ld]!\n",pthread_self(), tid);
    ret = pthread_join(tid, (void**)&old);
    printf("join returned %d and i got retval %ld\n", ret, old);
    return 9;
}

int initroutine(){
    size_t old;
    int ret = 0;

    pthread_create(&tids[1], NULL, (void* (*)(void*))&deadroutine, (void*) pthread_self());
    for (size_t i = 2; i < NUM_THREADS; i++)
    {
        pthread_create(&tids[i], NULL, (void* (*)(void*))&deadroutine, (void*) tids[i -1]);
    }
    printf("[initroutine] trying to join %ld!\n", tids[NUM_THREADS - 1]);
    ret = pthread_join(tids[NUM_THREADS - 1], (void**)&old);
    printf("[initroutine] join returned %d and i got retval %ld\n", ret, old);
    return 0;
}


int main()
{
    size_t old;
    fork();
    fork();
    int ret = 0;
    if (WITHOUT_MAIN)
    {
        pthread_create(&tids[0], NULL, (void* (*)(void*))&initroutine, NULL);
        sched_yield();
        pthread_join(tids[0], (void**) &ret);
        for (size_t i = 0; i < 10; i++)
            sched_yield();
    }
    else
    {
        printf("[main] init...\n");
        pthread_create(&tids[0], NULL, (void* (*)(void*))&deadroutine, (void*) pthread_self());
        for (size_t i = 1; i < NUM_THREADS; i++)
        {
            pthread_create(&tids[i], NULL, (void* (*)(void*))&deadroutine, (void*) tids[i -1]);
        }
        printf("[main] trying to join %ld!\n", tids[NUM_THREADS - 1]);
        ret = pthread_join(tids[NUM_THREADS - 1], (void**)&old);
        printf("[main] join returned %d and i got retval %ld\n", ret, old);
        sched_yield();
    }
    
    
   // pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old);
    printf("exit main...\n");
    return 0;
}