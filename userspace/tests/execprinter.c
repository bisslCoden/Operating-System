#include <unistd.h>
#include <stdio.h>

#define MAX_ARGS 5

int main(int argc, const char *argv[])
{
	/* does argument passing in sweb not work??
  if (argc  > MAX_ARGS + 1)
	{
		printf(	"TOO MANY ARGUMENTS, WRITE AT MAX 5 WORDS!\n"
			"EXITING...\n");
		return 42;
	}
	
	char* const path = "printer.sweb";
	char* const arg1 = (char* const)(argv[1]);
	char* const arg2 = (char* const)(argv[2]);
	char* const arg3 = (char* const)(argv[3]);
	char* const arg4 = (char* const)(argv[4]);
	char* const arg5 = (char* const)(argv[5]);
  */
 
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
	int ret = execv(path, args);
  printf("execv failed with return value %d!\n", ret);
	return 42;
}