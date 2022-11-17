#include <unistd.h>
#include <stdio.h>
#include "pthread.h"
#include "time.h"

#define MAX_ARGS 5

int main(int argc, const char *argv[])
{
  size_t time_1 = clock();
  size_t time_21 = clock();
  printf("SYSCALL CLOCK time1 in ms%ld\n", (time_1));
  printf("SYSCALL CLOCK time2 in ms%ld\n", (time_21));
  char* const path = "/usr/printuntilnull.sweb";
  char* const args[] = {NULL};
  
  printf("before exec\n");
  int variable_with_name = execv(path, args);
  time_1 = clock();
  time_21 = clock();
  printf("SYSCALL CLOCK time1 in ms%ld\n", (time_1));
  printf("SYSCALL CLOCK time2 in ms%ld\n", (time_21));
  printf("execv failed with return value %d!\n", variable_with_name);
  return 0;
}