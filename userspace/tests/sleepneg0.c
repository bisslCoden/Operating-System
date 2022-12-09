#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include "unistd.h"
#include "time.h"
// this test checks the basic functionality of sleep
// not working yet

int main(int argc, char** argv)
{
  printf("Sleep test started with 0!\n"); // clock return RDTSC in this case
  int x = sleep(0);
  printf("Sleep test end with 0!\n"); // clock return RDTSC in this case
  printf("Sleep test started with -1!\n"); // clock return RDTSC in this case
  x = sleep(-1);
  printf("Sleep test end with -1!\n"); // clock return RDTSC in this case
  return x;
}
