#include "pthread.h"
#include <stdio.h>

#define NUM_THREADS1 5000
#define NUM_THREADS2 5000
#define NUM_THREADS3 5000

pthread_t tids[NUM_THREADS1 + NUM_THREADS2+ NUM_THREADS3];
pthread_t rets[NUM_THREADS1 + NUM_THREADS2+ NUM_THREADS3];



void simple_routine()
{
  printf("2 - PTHREAD: FUNCTION CALL WORKES\n");
  int bef;
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &bef);
  int result = -98;
  for (size_t i = 0; i < 4000; i++)
  {
    result+= i - 98 * 2 /5 % 56;
  }
  pthread_exit((void*)9);
}

void simple_routine2()
{
  int old;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old);
  printf("not cancellable any more biiitch!\n");
  pthread_exit((void*)12);
}

void simple_routine3()
{ 
  int result = -98;
  for (size_t i = 0; i < 40000; i++)
  {
    result+= i - 98 * 2 /5 % 56;
  }
  printf("this should take some time :D\n");
  pthread_exit((void*) 4);
}

int main()
{
  printf("1 - main: Hello!\n");
  //int ret = pthread_create(&tid, NULL, (void*(*)(void*))++i&simple_routine, NULL);
  int ret = 0;
  for(size_t i = 0; i < NUM_THREADS1; ++i){
    ret = pthread_create(&tids[i], NULL, (void*)&simple_routine, NULL);
  }
  for(size_t i = NUM_THREADS1; i < NUM_THREADS1 + NUM_THREADS2; ++i){
    ret = pthread_create(&tids[i], NULL, (void*)&simple_routine2, NULL);
  }
  for (size_t i = NUM_THREADS1 + NUM_THREADS2; i < NUM_THREADS1 + NUM_THREADS2 + NUM_THREADS3; i++)
  {
    ret = pthread_create(&tids[10], NULL, (void*)&simple_routine3, NULL);
  }
  printf("3 - main again: pthread_create() returned;\n");
  

  for(size_t i = 0; i < NUM_THREADS1 + NUM_THREADS2 + NUM_THREADS3; ++i){
    pthread_cancel(tids[i]);
  }
  for(size_t i = 0; i < NUM_THREADS1 + NUM_THREADS2 + NUM_THREADS3; ++i){
    pthread_join(tids[i],(void**)&rets[i]);
  }
  for(size_t i = 0; i < NUM_THREADS1 + NUM_THREADS2 + NUM_THREADS3; ++i){
    printf("thread %ld\treturns: %ld\n", tids[i], rets[i]);
  }
  // anti warinign
  printf("%d\n", ret);
  return 0;
}