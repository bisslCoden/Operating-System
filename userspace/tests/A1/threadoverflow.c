#include "pthread.h"
#include <stdio.h>
#include "sched.h"
#include "assert.h"

#define SIZE 7000000
#define NUM_THREADS1 2
#define NUM_THREADS2 2

pthread_t tids[NUM_THREADS1 + NUM_THREADS2];
int returnvalues [NUM_THREADS1 + NUM_THREADS2];
typedef struct args2
{
    int a;
    size_t b;
    float com;
    int* xy;
}args2;


void simple_routine()
{
  int result = -98;
  int big[SIZE];
  for (size_t i = 0; i < SIZE; i++)
  {
    big[i] = i * result;
  }
  printf("%d", (result + big[0]));
}

size_t simple_routine2(args2* params)
{
  int old;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old);
  size_t result;
  result = params->a + params->b + (*(params->xy));
  //printf("ROUTINE 2 returning %ld and float is: %f\n", result, params->com);
  return result;
}

// void simple_routine3()
// { 
//   int result = -98;
//   for (size_t i = 0; i < 40000; i++)
//   {
//     result+= i - 98 * 2 /5 % 56;
//   }
//   printf("this should take some time :D\n");
//   pthread_exit((void*) 4);
// }

int main()
{
  printf("starting main now...\n");
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