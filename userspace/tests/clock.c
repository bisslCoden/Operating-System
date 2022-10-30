#include "stdio.h"
#include "time.h"

int main(int argc, char *argv[]) {
  clock_t time_1 = clock();
  printf("SYSCALL CLOCK has been started and the return value is: %d", (time_1/CLOCKS_PER_SEC));
  int x = 100000;
  for(int i = 0; i < 1000; i++)
  {
    x += 2;
    x /= 2;
    x *= 2;
  }
  clock_t time_2 = clock();
  printf("SYSCALL CLOCK has been called and the return value is: %d", (time_2/CLOCKS_PER_SEC));
  printf("CLOCK TEST has been finished and the difference is: %d", ((time_2 - time_1) / CLOCKS_PER_SEC));
  return 0;
}