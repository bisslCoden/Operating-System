#include "stdio.h"
#include "time.h"
#include <stdio.h>
#include "pthread.h"
#include <unistd.h>

pthread_t tid;

int fastroutine()
{
//     int old;
//     printf("old val was %d\n", old);
//    // pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old);
    printf("graceful exit!\n");
    pthread_exit((void*)99);
    return 1;
}

int main(int argc, char *argv[]) {
  clock_t time_1 = clock();
  time_1 = clock();
  sleep(1);
  clock_t time_21 = clock();
  printf("SYSCALL CLOCK time1 %d\n", (time_1)/ CLOCKS_PER_SEC);
  printf("SYSCALL CLOCK time2 %d\n", (time_21)/ CLOCKS_PER_SEC);
  printf("SYSCALL CLOCK has been started and  the return value is: %d\n", (time_1) / CLOCKS_PER_SEC);
  int ret;
  int returner;
  pthread_create(&tid, NULL, (void* (*)(void*))&fastroutine, NULL);
  for(int i = 10000; i > 0; i--)
  {
    int j = 1 + i;
  }
  returner =  pthread_join(tid, (void**)&ret);
  printf("join returned: %d got val %d\n",returner, ret);
  clock_t time_2 = clock();
  printf("SYSCALL CLOCK has been called and the return value is: %d\n", (time_2) / CLOCKS_PER_SEC);
  printf("CLOCK TEST has been finished and the difference is: %d\n", ((time_2 - time_1)) / CLOCKS_PER_SEC);
  return 0;
}