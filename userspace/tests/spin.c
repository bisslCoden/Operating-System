#include "pthread.h"
#include <stdio.h>

pthread_spinlock_t spinny;
pthread_t tids[10]; 

int counter = 0;

//
int simple_routine()
{
  int spinret = 0;
  printf("hi i ll try to get the spin!\n");
     spinret = pthread_spin_lock(&spinny);
     printf("spinret LOCK returned me %d!\n", spinret);
for (size_t i = 0; i < 40000000; i++)
{
    int res = (i + i+1) % 365;
    if((counter + res) < 200000000)
        counter += res;
}
     spinret = pthread_spin_unlock(&spinny);
     printf("spinret UNLOCK returned me %d!\n", spinret);

  printf("unlocked it!\n");
  return 0;
}

int main()
{
  int ret;
  int rets;
  pthread_spin_init(&spinny, 0);
  printf("Hello!\n");
  for (size_t i = 0; i < 10; i++)
  {
    ret = pthread_create(&tids[i], NULL, (void* (*)(void*)) &simple_routine, NULL);
  }
   for (size_t i = 0; i < 10; i++)
  {
    ret = pthread_join(tids[i], (void**) &rets);
  }
  printf("%d %d joined all threads successfully and the counter is: %d and lower than 200000000?\n",ret, rets, counter);
  return 0;
}