#include <pthread.h>
#include <stdio.h>

void simple_routine()
{
  printf("2 - PTHREAD: FUNCTION CALL WORKES\n");
}

int main()
{
  printf("1 - main: Hello! simple_routine lies at %lx\n", (size_t)simple_routine);
  pthread_t tid; 
  //int ret = pthread_create(&tid, NULL, (void*(*)(void*))&simple_routine, NULL);
  int ret = pthread_create(&tid, NULL, (void*)&simple_routine, NULL);
  printf("3 - main again: pthread_create() returned %d and tid %ld\n", ret, tid);
  return 0;
}