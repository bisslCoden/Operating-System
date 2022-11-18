#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <wait.h>


#define ARGUMENT 2022

void* pthreadExitFunc()
{
  pthread_exit(NULL);
  printf("non reachable\n");
  return NULL;
}

void* cancelFunc()
{
  while (1)
  {
    printf(" ");
  }
  return 0;
}

void* threadInThread()
{
  pthread_t exitThread;
  size_t pid = pthread_create(&exitThread,NULL,pthreadExitFunc(),NULL);
  assert(pid == 0 && "tid == 0");
  return NULL;
}

void* retzero()
{
  return 0;
}

size_t out(void *argument)
{
  assert((size_t)argument == ARGUMENT && "passed bad arg");
  return NULL;
}

int main(int argc, char *argv[])
{
  pthread_t thready_kruger;

  printf("1:pthread create\n");
  size_t output_thread_pid = pthread_create(&thready_kruger,NULL,(void*)out,(void*)ARGUMENT);
  assert(output_thread_pid == 0);
  printf("first test passed\n");

  printf("2: pthread exit\n");
  pthread_t thready_kruger_exit;
  size_t pthread_exit_thread_pid = pthread_create(&thready_kruger_exit,NULL,(void*)pthreadExitFunc,NULL);
  assert(pthread_exit_thread_pid == 0);
  printf("second test passed\n");


  printf("3: thread in a thread\n");
  pthread_t pthread_in_thread;
  size_t pthread_in_thread_pid = pthread_create(&pthread_in_thread,NULL,(void*)threadInThread,NULL);
  assert(pthread_in_thread_pid == 0);
  printf("third test passed\n");

  printf("4: creating 12k threads, chill\n");
  for(size_t i = 0;i<=12;i++)
  {
    pthread_t pthreadMax;
    size_t pthreadMax_pid = pthread_create(&pthreadMax,NULL,(void*)retzero,NULL);
    assert(pthreadMax_pid == 0);
  }
  printf("fourth test passed\n");


  printf("5: pthread cancel 2x\n");
  pthread_t pthread_cancel_test;
  size_t pthread_cancel_pid = pthread_create(&pthread_cancel_test,NULL,cancelFunc,NULL);
  assert(pthread_cancel_pid == 0);
  size_t  cancel_return = pthread_cancel(pthread_cancel_test);
  assert(cancel_return == 0);
  cancel_return = pthread_cancel(pthread_cancel_test);
  printf("fifth test passed\n");


  printf("6: fork\n");
  int status;
  int ret = fork();
  printf("fork returned %d\n", ret);
  printf("sixth test passed\n");

  printf("7: execv\n");

  if (ret == 0) {
    printf("child from fork call\n");
    execv("usr/forkinfork.sweb", NULL);
    assert(0 && "Execv failed\n");
  }
  waitpid(ret, &status, 0);
  printf("seventh test passed\n");

  printf("8: detach\n");
  int ret2 = fork();
  if (ret2 == 0) {
    execv("usr/detach.sweb", NULL);
    assert(0 && "Execv failed\n");
  }
  waitpid(ret2, &status, 0);
  printf("eight test passed\n");


  printf("all tests passed\n");
  return 0;
}
