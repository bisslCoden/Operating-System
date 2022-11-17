#include "pthread.h"
#include "wait.h"
#include <stdio.h>
#include "sched.h"
#include "assert.h"
#include "unistd.h"

#define PTHREADS 30

pthread_t tids[PTHREADS];
char* const path1 = "/usr/pcreate.sweb";

void forkThenExec(char* const args[]){
  pid_t pid = fork();
  if (pid == 0)
    assert(execv(args[0], args) < 1);
}

int main()
{
  printf("[main] starting pfork now...\n");

  for(size_t i = 0; i < PTHREADS; ++i)
  {
    char* const args_cur[] = {path1, "one", "two", "thee", "quack", NULL};
    assert(pthread_create(&tids[i], NULL, (void*(*)(void *))&forkThenExec, (void*)args_cur) == 0 && "couldnt create anymore threads daaamn!\n");
  }

  printf("[main]: successfully exiting the programm!\n");
  return 0;
}