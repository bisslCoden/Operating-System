#include <unistd.h>
#include <stdio.h>
#include "pthread.h"

#define MAX_ARGS 5
#define MAX_THREADS 10
#define MAX_LOOPS 1000

void simple_routine()
{
  size_t value = 123;
  for(int i = 0; i < MAX_LOOPS; i++)
    value *= i;
  if(value)
    printf("lalalala\n");
}

int main(int argc, const char *argv[])
{
	int ret_pthread[MAX_THREADS];
  //int retvals[MAX_THREADS];
  pthread_t tids[MAX_THREADS]; 
  for(size_t i = 0; i < MAX_THREADS; ++i)
    ret_pthread[i] = pthread_create(&tids[i], NULL, (void*)&simple_routine, NULL);
  // exec should be called after threads are destroyed.
  if((size_t)ret_pthread > 3)
    printf("ahelo\n");
 
  // hardcoded args :(
  char* const path = "/usr/printer.sweb";
	char* const arg1 = "Eier";
	char* const arg2 = "Mehl";
	char* const arg3 = "Butter";
	char* const arg4 = "Salz";
	char* const arg5 = "Mehl";

	// exec call
	char* const args[] = {path, arg1, arg2, arg3, arg4, arg5, NULL};
	printf("before exec\n");
	int ret_exec = execv(path, args);
  printf("execv failed with return value %d!\n", ret_exec);
	return 42;
}