#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

void *thready()
{
  printf("thready lives in your dreams\n");
  return (void*) 5;
}

int main(void)
{
  int i = 0;

  printf("pthread_detach\n");

  pthread_t tid;
  pthread_create(&tid, NULL, thready, NULL);

  int detached = 20;
  detached = pthread_detach(tid);
  printf("thread id detached: %d\n", detached);

  int ret;
  size_t join_ret_val = pthread_join(tid, (void*) &ret);
  printf("retval of undetached thread is: %d\n", ret);
  printf("retval of detached thread is: %ld\n", join_ret_val);
  assert(join_ret_val == -1 && "Error, join still possible!\n");

  for (i = 0; i < 100; i++);

  printf("Test done!\n");
  return 0;
}