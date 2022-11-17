#include <unistd.h>
#include <stdio.h>
#include "pthread.h"

#define MAX_ARGS 5

int main(int argc, const char *argv[])
{
	printf("before exec\n");
  char* const path = "/usr/execforever.sweb";
	char* const arg1 = "Eijfdlsajflödjslafködjklsajöfljkdlösjaölfjkdsjaläöfjeirowqgjorj3qogöjöorjoerEijfdlsajflödjslafködjklsajöfljkdlösjaölfjkdsjaläöfjeirowqgjorj3qogöjöorjoerEijfdlsajflödjslafködjklsajöfljkdlösjaölfjkdsjaläöfjeirowqgjorj3qogöjöorjoer";
	char* const arg2 = "Mehl";
	char* const arg3 = "Butter";
	char* const arg4 = "Salz";
	char* const arg5 = "Mehl";

  printf("calling exec now\n");

	// exec call
	char* const args[] = {path, arg1, arg2, arg3, arg4, arg5, NULL};
	printf("before exec\n");
	int ret = execv(path, args);
  printf("execv failed with return value %d!\n", ret);
	return 42;
}