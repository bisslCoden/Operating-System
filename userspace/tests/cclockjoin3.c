#include "stdio.h"
#include "time.h"
#include <stdio.h>
#include "pthread.h"
#include <unistd.h>

//////////////////////////////////////////////////
// TEST WAS MADE WITH A MODIFIED SLEEP FUNCTION //
// WHERE SLEEPING THREAD CYCLES ARE COUNTED     //
//////////////////////////////////////////////////

pthread_t tid;

int fastroutine()
{
//     int old;
//     printf("old val was %d\n", old);
//    // pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old);
    printf("graceful exit!\n");
    for(int i_ = 10000; i_ > 0; i_--)
    {
      for(int k_ = 100000; k_ > 0; k_--)
      {
        int l_ = 1 + l_;
      }
    }
    for(int i_ = 10000; i_ > 0; i_--)
    {
      for(int k_ = 100000; k_ > 0; k_--)
      {
        int l_ = 1 + l_;
      }
    }
    pthread_exit((void*)99);
    return 1;
}

int main(int argc, char *argv[]) {
   for(int i_ = 10000; i_ > 0; i_--)
    {
      for(int k_ = 100000; k_ > 0; k_--)
      {
        int l_ = 1 + l_;
      }
    }
  size_t time_1 = clock();


  sleep(1);
  size_t time_21 = clock();
  printf("SYSCALL CLOCK time1 in ms%ld\n", (time_1));
  printf("SYSCALL CLOCK time2 in ms%ld\n", (time_21));
  int ret;
  int returner;
  pthread_create(&tid, NULL, (void* (*)(void*))&fastroutine, NULL);
  returner =  pthread_join(tid, (void**)&ret);
  pthread_create(&tid, NULL, (void* (*)(void*))&fastroutine, NULL);
  returner =  pthread_join(tid, (void**)&ret);
  pthread_create(&tid, NULL, (void* (*)(void*))&fastroutine, NULL);
  returner =  pthread_join(tid, (void**)&ret);
  pthread_create(&tid, NULL, (void* (*)(void*))&fastroutine, NULL);
  returner =  pthread_join(tid, (void**)&ret);
  pthread_create(&tid, NULL, (void* (*)(void*))&fastroutine, NULL);
  returner =  pthread_join(tid, (void**)&ret);
  pthread_create(&tid, NULL, (void* (*)(void*))&fastroutine, NULL);
  returner =  pthread_join(tid, (void**)&ret);
  printf("join returned: %d got val %d\n",returner, ret);
  size_t time_2 = clock();
  printf("SYSCALL CLOCK has been called and the return value is in ms: %ld\n", (time_2));
  printf("CLOCK TEST has been finished and the difference is in ms: %ld\n", ((time_2) - (time_1)));
  printf("CLOCK TEST has been finished and the difference is in s: %ld\n", (time_2)/ CLOCKS_PER_SEC - (time_1) / CLOCKS_PER_SEC);
  return 0;
}