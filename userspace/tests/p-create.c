#include <pthread.h>
#include <stdio.h>

int simple_routine()
{

  return 0;
}

int main()
{
  printf("Hello!\n");
  pthread_t tid; 
  int ret = pthread_create(&tid, NULL, (void*)simple_routine, NULL);
  printf("pthread_create() returned %d and tid %ld\n", ret, tid);
  return 0;
}