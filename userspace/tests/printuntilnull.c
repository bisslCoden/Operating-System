#include "pthread.h"
#include <stdio.h>


int main(int argc, char* const argv[])
{
  printf(  "|----------------------------------------\n"
           "|---your beatutifully passed arguments---\n"
           "|----------------------------------------\n"
           "| \n");

  if(argv && argv[0])
  {
    for (int i = 0; argv[i]; i++) 
      printf("| - argv[%d] = '%s'\n", i, argv[i]);
  }
    
  printf(  "|\n"
           "|----------------------------------------\n"
           "|----finished printing your arguments----\n"
           "|----------------------------------------\n");
           
  return 0;
}