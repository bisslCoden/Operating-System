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
  printf("forking\n");
  pid_t pid = fork();
  int ret = execv(args[0], args);
  printf("exec returned.. ret = %d\n", ret);
}

int main()
{
  printf("[main] starting pexec now...\n");
  pthread_t ret;

  for(size_t i = 0; i < PTHREADS; ++i)
  {
    char* const args_cur[] = {path1, "one", "two", "thee", "quack", NULL};
    ret = pthread_create(&tids[i], NULL, (void*(*)(void *))&forkThenExec, (void*)args_cur) == 0 && "couldnt create anymore threads daaamn!\n";
  }

  printf("[main]: successfully exiting the programm! pcreate ret = %ld\n", ret);
  return 0;
}