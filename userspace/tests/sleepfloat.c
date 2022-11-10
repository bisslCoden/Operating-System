#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include "unistd.h"
#include "time.h"
// this test checks the basic functionality of sleep
// not working yet

int main(int argc, char** argv)
{
  printf("Sleep test started!\n"); // clock return RDTSC in this case
  int x = sleep(5.3);
  printf("Sleep test enden!\n"); // clock return RDTSC in this case
  return x;
}
