#include "pthread.h"
#include "stdio.h"


int initialized[1000] = {44};
int uninitialized[4000];

int main()
{
  pthread_self();
  return 0;
}