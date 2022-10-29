#include "pthread.h"
#include <stdio.h>
#include "sched.h"
#include "assert.h"


#define NUM_THREADS1 50
#define NUM_THREADS2 30
#define NUM_THREADS3 2


pthread_t tids[NUM_THREADS1 + NUM_THREADS2 + NUM_THREADS3];
int returnvalues [NUM_THREADS1 + NUM_THREADS2 + NUM_THREADS3];
typedef struct args2
{
    int a;
    size_t b;
    float com;
    int* xy;
}args2;

pthread_attr_t attrdefault;
pthread_attr_t attrdetach;
pthread_attr_t uninitialzed;


void simple_routine()
{
  //int bef;
  //pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &bef);
  pthread_detach(pthread_self());
  size_t result = -98;
  for (size_t i = 0; i < 4000; i++)
  {
    //sched_yield();
    result+= i;
  }
  pthread_exit((void*) result);
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

  printf("Atrr retvals %d - %d - %d\n", 
  pthread_attr_init(&attrdefault),
  pthread_attr_init(&attrdefault),
  pthread_attr_init(&attrdetach)
  );
  pthread_attr_getdetachstate(&attrdefault, &ret);
  printf("attr_detachstate: %d\n", ret);
  pthread_attr_setdetachstate(&attrdetach, PTHREAD_CREATE_DETACHED);
  


  args2 paramsnow = {4, 4, 0.04f, &four};
  for(size_t i = 0; i < NUM_THREADS1; ++i){
    assert((ret = pthread_create(&tids[i], &attrdefault, (void* (*)(void*))&simple_routine, NULL)) == 0 && "couldnt create anymore threads daaamn!\n");
  }
  for(size_t i = NUM_THREADS1; i < NUM_THREADS2 + NUM_THREADS1; ++i){
    assert((ret = pthread_create(&tids[i], &attrdetach, (void* (*)(void*))&simple_routine2, (void*) &paramsnow)) == 0 && "couldnt create anymore threads daaamn!\n");
  }
  for(size_t i = NUM_THREADS1 + NUM_THREADS2; i < NUM_THREADS2 + NUM_THREADS1 + NUM_THREADS3; ++i){
    ret = pthread_create(&tids[i], &uninitialzed, (void* (*)(void*))&simple_routine2, (void*) &paramsnow);
    printf("trying to create with uninitialzied: %d\n", ret);
  }
  sched_yield();
 // int busy = 0;
  //int long_ = 0;
//   while (long_ < 4000000000000)
//   {
//     busy += long_;
//   
//   int cury = -10;
//   for(size_t i = 0; i < NUM_THREADS1 + NUM_THREADS2; ++i){
//     ret = pthread_cancel(tids[i]);
//     if (ret != cury)
//     {
//         printf("MAIN: cancel for thread [%ld] cancel returned me: %d\n", tids[i], ret);
//         cury = ret;
//     }
//   } 
//   sched_yield();


  int cury = -10;
  for(size_t i = 0; i < NUM_THREADS1 + NUM_THREADS2; ++i){
    ret = pthread_join(tids[i], (void**) &returnvalues[i]);
    if (ret != cury)
    {
        printf("new ret from join is = %d at iteration %ld\n", ret, i);
        cury = ret;
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