#include "pthread.h"
#include <stdio.h>

int global = 4;

int main()
{
  printf("this is a VERRRY simple program to test p_exit!\n");
  pthread_exit((void*)9);
  return 1;
}