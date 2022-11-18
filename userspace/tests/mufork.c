#include "pthread.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include "sched.h"
#include "wait.h"
#include "getpid.h"

#define NUM_THREADS 2

pthread_mutex_t mutex;
pthread_t tids[NUM_THREADS]; 

int counter = 0;
int never_false = 1;


//
int simple_routine()
{
  int mutret = 0;
  printf("hi i ll try to get the mutex!\n");
  mutret = pthread_mutex_lock(&mutex);
  printf("lock returned %d.\n", mutret);
 // pid_t pid = fork();
 // printf("%s, [%d]\n", (pid == 0)? "i am child" : "i am parent", getpid());
  assert(never_false == 1  && "never false aint 1? whaat? - PARENT\n");
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
  //fork();

//  printf("unlocked it!\n");
  return 0;
}

int main()
{
  printf("[main] Hello!\n");
  pthread_mutex_init(&mutex, NULL);
  pid_t pid = fork();
  for (size_t i = 0; i < NUM_THREADS; i++)
  {
    assert(pthread_create(&tids[i], NULL, (void* (*)(void*)) &simple_routine, NULL) == 0);
  }
   for (size_t i = 0; i < NUM_THREADS; i++)
  {
    assert(pthread_join(tids[i], NULL) == 0);
  }
//   if (pid != 0)
//   {
//     waitpid(pid, 0 , 0);
//   }
  printf("[main] exit...\n");
  return 0;
}