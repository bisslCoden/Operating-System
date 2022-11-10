#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include "unistd.h"
// this test checks the basic functionality of sleep
// not working yet

int main(int argc, char** argv)
{
  printf("Sleep test started!\n");
  int x = sleep(50);
  printf("Sleep test enden!\n");
  return x;
}
