#include <pthread.h>
#include <stdio.h>

#define SIMPLE1 1
#define SIMPLE2 1
#define SIMPLE3 1
#define FORK 1

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

  // creates
  for(size_t i = 0; i < SIMPLE1; i++){
    ret = pthread_create(&tids[i], NULL, (void*)&simple_routine, NULL);
  }
  for(size_t i = SIMPLE1; i < (SIMPLE1 + SIMPLE2); i++)
  {
    ret = pthread_create(&tids[i], NULL, (void*)&simple_routine2, NULL);
  }
  for(size_t i = SIMPLE1 + SIMPLE2; i < (SIMPLE1 + SIMPLE2 + SIMPLE3); i++)
  {
    ret = pthread_create(&tids[i], NULL, (void*)&simple_routine3, NULL);
  }
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