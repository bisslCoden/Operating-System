#include "pthread.h"
#include "wait.h"
#include <stdio.h>
#include "sched.h"
#include "assert.h"
#include "unistd.h"

#define NUM_THREADS1 4
#define NUM_THREADS2 4
// #define NUM_THREADS3 1
// #define NUM_REC_FORKS 1
#define ARRSIZE 16000

pthread_t tids[NUM_THREADS1 + NUM_THREADS2];
int returnvalues [NUM_THREADS1 + NUM_THREADS2];

char* const path1 = "/usr/printuntilnull.sweb";
char* const path2 = "/usr/simpleforktest.sweb";


pthread_mutex_t never_false_lock;
pthread_mutex_t threads3_lock;


//NOTE: what to do if gets cancelled async and has lock 

size_t with_argv(char* const args[]){
    pid_t forky = fork();
    if (forky == 0)
    {
        //printf("[exec argv] I ll start sth new...\n");
        assert(execv(args[0], args) == 0);
    }
    else
    {
        //printf("[no exec] I ll wait...\n");
        forky = waitpid(forky, 0, 0);
    }
    //printf("[no exec] return...\n");
    return 420;
}

size_t without_argv()
{
    int arr[ARRSIZE];
    pid_t pid = fork();
    for (size_t i = 0; i < ARRSIZE; i++)
    {
        arr[i] = 78;
    }
    if (pid == 0)
    {
        //printf("[exec NO argv] I ll start sth new...\n");
        assert(execv(path2, NULL) == 0);
    }
    else
    {
        //printf("[no exec] waiting for my child!\n");
        pid = waitpid(pid, 0, 0);
    }
    //printf("[no exec] returning...\n");
    return arr[ARRSIZE -1];
}



int main()
{
  printf("[main] starting pfork now...\n");
  //pid_t child = fork();

  char* const guys[NUM_THREADS1] = {"Hannes", "Thomas", "Dominik", "Jan"};

  for(size_t i = 0; i < NUM_THREADS1; ++i){
    char* const args_cur[] = {path1, "sexy", "sweet", "funny", "cool", guys[i], NULL};
    assert(pthread_create(&tids[i], NULL, (void* (*)(void*))&with_argv, (void*) args_cur) == 0 && "couldnt create anymore threads daaamn!\n");
  }

  for(size_t i = NUM_THREADS1; i < NUM_THREADS1 + NUM_THREADS2; ++i){
    assert(pthread_create(&tids[i], NULL, (void* (*)(void*))&without_argv, NULL) == 0 && "couldnt create anymore threads daaamn!\n");
  }

  sched_yield();
  sched_yield();
  sched_yield();

  int ret = 0;
  for(size_t i = 0; i < NUM_THREADS1 + NUM_THREADS2; ++i){
    ret = pthread_join(tids[i], (void**) &returnvalues[i]);
    if (ret != 0)
    {
      //printf("[main]: could not join [%ld]... %s\n", i, (i >= NUM_THREADS1) ? "thats okay he detached" : "which is NOT okay!");
      returnvalues[i] = -1;
    }
    else;
        //printf("[main] joined [%ld] got %d\n", i , returnvalues[i]);
  }
  printf("[main]: successfully exiting the programm!\n");
  return 0;
}