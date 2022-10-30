#include "stdio.h"
#include "time.h"

int main(int argc, char *argv[]) {
  clock_t time = clock();
  printf("SYSCALL CLOCK has been started and the return value is: %d", time);
  return 0;
}