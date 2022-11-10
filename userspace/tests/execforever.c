#include <unistd.h>
#include <stdio.h>
#include "pthread.h"

int main(int argc, const char *argv[])
{
  // hardcoded args :( - MAX_ARGS
  char* const path = "/usr/execforever.sweb";
	char* const arg1 = "Eier";
	char* const arg2 = "Mehl";
	char* const arg3 = "Butter";
	char* const arg4 = "Salz";
	char* const arg5 = "Mehl";

  printf("calling exec now\n");

	// exec call
	char* const args[] = {path, arg1, arg2, arg3, arg4, arg5, NULL};
	int ret_exec = execv(path, args);
  printf("execv failed with return value %d!\n", ret_exec);
	return 42;
}