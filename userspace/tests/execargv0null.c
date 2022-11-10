#include <unistd.h>
#include <stdio.h>
#include "pthread.h"

#define MAX_ARGS 5

int main(int argc, const char *argv[])
{
  char* const path = "/usr/printuntilnull.sweb";
	char* const args[] = {NULL};
  
	printf("before exec\n");
	int ret_exec = execv(path, NULL);
  printf("execv failed with return value %d!\n", ret_exec);
  return 0;
}