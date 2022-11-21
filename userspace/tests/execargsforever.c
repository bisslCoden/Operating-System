#include <unistd.h>
#include <stdio.h>
#include "pthread.h"

#define MAX_ARGS 5

int main()
{
  // hardcoded args :(
  char* const path = "/usr/printargvloop.sweb";
	char* const arg1 = "Eier";
	char* const arg2 = "Mehl";
	char* const arg3 = "Butter";
	char* const arg4 = "Salz";
	char* const arg5 = "DAS ARGS SOLLTE NIE GEPRINTET WERDEN FUAAQQQQ";

	// exec call
	char* const args[] = {arg1, arg2, arg3, arg4, NULL, arg5, NULL};
	printf("before exec\n");
	int ret = execv(path, args);
  printf("execv failed with return value %d!\n", ret);
	return 0;
}