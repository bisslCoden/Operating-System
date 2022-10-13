#include <pthread.h>
#include <stdio.h>

void simple_routine()
{
  printf("2 - PTHREAD: FUNCTION CALL WORKES\n");
  int bef;
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &bef);
  int result = -98;
  for (size_t i = 0; i < 4000; i++)
  {
    result+= i - 98 * 2 /5 % 56;
  }
  pthread_exit((void*)9);
}

void simple_routine2()
{
  int old;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old);
  printf("not cancellable any more biiitch!\n");
  pthread_exit((void*)12);
}

void simple_routine3()
{ 
  int result = -98;
  for (size_t i = 0; i < 40000; i++)
  {
    result+= i - 98 * 2 /5 % 56;
  }
  printf("this should take some time :D\n");
  pthread_exit((void*) 4);
}

int main()
{
  printf("1 - main: Hello! simple_routine lies at %lx\n", (size_t)simple_routine);
  pthread_t tids[11]; 
  int retvals[11];

  //int ret = pthread_create(&tid, NULL, (void*(*)(void*))++i&simple_routine, NULL);
  for(size_t i = 0; i < 5; ++i){
    int ret = pthread_create(&tids[i], NULL, (void*)&simple_routine, NULL);
  }
  for(size_t i = 5; i < 9; ++i){
    int ret = pthread_create(&tids[i], NULL, (void*)&simple_routine2, NULL);
  }
  int ret = pthread_create(&tids[10], NULL, (void*)&simple_routine3, NULL);

  pthread_join(tids[10], (void**)&retvals[10]);
  printf("3 - main again: pthread_create() returned;\n");
  for(size_t i = 0; i < 10; ++i){
    pthread_cancel(tids[i]);
  }
  for(size_t i = 0; i < 10; ++i){
    pthread_join(tids[i],(void**)&retvals[i]);
  }
  for(size_t i = 0; i < 10; ++i){
    printf("thread %ld\treturns: %d\n", tids[i], retvals[i]);
  }
  return 0;
}