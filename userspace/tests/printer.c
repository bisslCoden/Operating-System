#include "pthread.h"
#include <stdio.h>


int main(int argc, const char *argv[])
{
  printf("|----------------------------------------\n"
         "|---your beatutifully passed arguments---\n"
         "|----------------------------------------\n"
         "| \n");

  for (int i = 0; i < argc; i++) 
    printf("| - argv[%d] = '%s'\n|  ", i, argv[i]);
    
  printf("|----------------------------------------\n"
         "|----finished printing your arguments----\n"
         "|----------------------------------------\n");
  return 0;
}