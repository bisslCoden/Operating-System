#include <stdio.h>
#include <unistd.h>
#include "pthread.h"

void* forker()
{
  fork();
  return NULL;
} 

int main()
{
  printf("main(): pthread_create(forker()).\n");
  pthread_t pthread_fork_tid = pthread_create(&pthread_fork_tid, 0,  &forker, NULL);

  printf("main(): fork().\n");
  fork();

  int evecv_retval = execv("/usr/pthreadfork.sweb", NULL);
  printf("main(): after execv() o_O. ret_val = %d\n", evecv_retval);

  return 0;
}