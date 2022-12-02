#include <unistd.h>
#include <stdio.h>
#include "pthread.h"

int main(int argc, const char *argv[])
{
  // hardcoded args :( - MAX_ARGS
  char* const path = "/usr/execforever.sweb";

  printf("calling exec now\n");

	// exec call
	int ret_exec = execv(path, NULL);
  printf("execv failed with return value %d!\n", ret_exec);
	return 0;
}