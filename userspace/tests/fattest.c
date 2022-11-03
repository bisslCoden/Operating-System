#include "pthread.h"
#include <stdio.h>

// everything on 2 or we run out of memory
#define SIMPLE1 2
#define SIMPLE2 2
#define SIMPLE3 2
#define FORKS 2

void simple_routine()
{
  pid_t pid = fork();
  int bef;
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &bef);
  int result = -98;
  for (size_t i = 0; i < 4000; i++)
  {
    result+= i - 98 * 2 /5 % 56;
  }
  if(pid)
    printf("[1 parent: cancel async!] ");
  else
    printf("[1 child: cancel async] ");
  pthread_exit((void*)9);
}
//hui
void simple_routine2()
{
  int old;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old);
  pid_t pid = fork();
  if(pid)
    printf("[2 parent: child ]");
  else
    printf("[2 child 0 ]");
  pthread_exit((void*)12);
}

int simple_routine3()
{ 
  int result = -98;
  for (size_t i = 0; i < 40000; i++)
  {
    result+= i - 98 * 2 /5 % 56;
  }
  printf("[3 yeah %d] ", result);
  pthread_exit((void*) 4);
  return result; 
}

int main()
{
  pthread_t tids[SIMPLE1 + SIMPLE2 + SIMPLE3]; 
  int retvals[SIMPLE1 + SIMPLE2 + SIMPLE3];
  int ret = 0;
  pid_t pid = fork();
  size_t fork_i = FORKS;

  // creates
  fork_i--;
  if(fork_i > 0) fork();
  for(size_t i = 0; i < SIMPLE1; i++){
    ret = pthread_create(&tids[i], NULL, (void*)&simple_routine, NULL);
  }
  fork_i--;
  if(fork_i > 0) fork();
  for(size_t i = SIMPLE1; i < (SIMPLE1 + SIMPLE2); i++)
  {
    ret = pthread_create(&tids[i], NULL, (void*)&simple_routine2, NULL);
  }
  fork_i--;
  if(fork_i > 0) fork();
  for(size_t i = SIMPLE1 + SIMPLE2; i < (SIMPLE1 + SIMPLE2 + SIMPLE3); i++)
  {
    ret = pthread_create(&tids[i], NULL, (void*)&simple_routine3, NULL);
  }
  fork_i--;
  if(fork_i > 0) fork();
  ret = pthread_create(&tids[10], NULL, (void*)&simple_routine3, NULL);

  // join/cancels
  pthread_join(tids[10], (void**)&retvals[10]);
  for(size_t i = 0; i < (SIMPLE1 + SIMPLE2); ++i)
  {
    pthread_cancel(tids[i]);
  }
  for(size_t i = 0; i < (SIMPLE1 + SIMPLE2 + SIMPLE3); ++i)
  {
    pthread_join(tids[i],(void**)&retvals[i]);
  }
  for(size_t i = 0; i < (SIMPLE1 + SIMPLE2 + SIMPLE3); ++i)
  {
    if(ret)
      printf("thread %ld\treturns: %d\n", tids[i], retvals[i]);
  }

  printf("\nmain pid: %ld\n", pid);
  return 0;
}