#include "pthread.h"
#include <stdio.h>


int main(int argc, char* const argv[])
{
  printf(  "|----------------------------------------\n"
           "|---your beatutifully passed arguments---\n"
           "|----------------------------------------\n"
           "| \n");

  for (int i = 0; i < argc; i++) 
    printf("| - argv[%d] = '%s'\n", i, argv[i]);
    
  printf(  "|\n"
           "|----------------------------------------\n"
           "|----finished printing your arguments----\n"
           "|----------------------------------------\n");

  int ret_exec = execv("usr/printuntilnull.sweb", argv);
  printf("execv failed with return value %d!\n", ret_exec);
  return 0;
}