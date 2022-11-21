#include <unistd.h>
#include <stdio.h>
#include "pthread.h"

int main(int argc, char* const argv[])
{
  printf(  "|----------------------------------------\n"
           "|---your beatutifully passed arguments---\n"
           "|----------------------------------------\n"
           "| \n");

  if(argv && argv[0])
  {
    for (int i = 0; argv[i] && i < argc; i++) 
      printf("| - argv[%d] = '%s'\n", i, argv[i]);
  }
    
  printf(  "|\n"
           "|----------------------------------------\n"
           "|----finished printing your arguments----\n"
           "|----------------------------------------\n");

  // char* const path = "/usr/printuntilnull.sweb"; // use this if loop not working.
  char* const path = "/usr/printargvloop.sweb";
	int ret_exec = execv(path, argv);

  printf("execv failed with return value %d!\n", ret_exec);
	return 0;
}