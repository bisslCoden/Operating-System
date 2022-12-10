#include <unistd.h>
#include <stdio.h>
#include "pthread.h"

#define MAX_ARGS 5
#define PTHREAD_CALLS 150

void* subroutine()
{
  printf("issa thread\n");
  return 0;
} 

int main()
{
  printf("main(): seawas. will call a fuckload of threads.\n");
  
  // pthread calls
  pthread_t tid[PTHREAD_CALLS];
  for(size_t i = 0; i < PTHREAD_CALLS; i++)
    tid[i] = pthread_create(tid + i, NULL, &subroutine, NULL);
 
  // hardcoded args :(
  char* const path = "/usr/printuntiliisargc.sweb";
	char* const arg1 = "Eier";
	char* const arg2 = "Mehl";
	char* const arg3 = "Butter";
	char* const arg4 = "Salz";
	char* const arg5 = "Mehl";

	// exec call
	char* const args[] = {path, arg1, arg2, arg3, arg4, arg5, NULL};
	printf("before exec\n");
	int ret = execv(path, args);
  printf("execv failed with return value %d!\n", ret);
	return 42;
}