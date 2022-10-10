#include <pthread.h>
#include <stdio.h>

int simple_routine()
{
  printf("2 - pthread: Hallo, ich bin's. Dein Pthread\n");
  return 0;
}

int main()
{
  printf("1 - main: Hello!\n");
  pthread_t tid; 
  int ret = pthread_create(&tid, NULL, (void*)simple_routine, NULL);
  printf("3 - main again: pthread_create() returned %d and tid %ld\n", ret, tid);
  return 0;
}