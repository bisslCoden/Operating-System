#include "pthread.h"
#include "stdio.h"

#define NR_OF_ARGS 3

void setArrayVals(int* array, int start)
{
  for(int i = 0; i < NR_OF_ARGS; i++)
    array[i] = start + i;
}

void printArrayVals(int* array, pid_t pid)
{
  for(int i = 0; i < NR_OF_ARGS; i++)
  {
    if(pid > 0)
      printf("  parent array[%d] = %d\n", i, array[i]);
    else if(pid == 0)
      printf("  child array[%d] = %d\n", i, array[i]);
    else
      return;
  }
  printf("finished printArrayvals(pid = %ld)\n", pid);
}

int main()
{
  printf("main() starting..\n");
	int values[NR_OF_ARGS];
  setArrayVals(values, -1000);
  printArrayVals(values, -1000);

	pid_t pid = fork();
  
  if(pid == 0)
    setArrayVals(values, 0);
  else if(pid > 0)
    setArrayVals(values, 1000);
  else
    return -1;

  printArrayVals(values, pid);

	return 0;
}
