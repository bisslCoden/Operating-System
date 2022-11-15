#include "stdio.h"
#include "time.h"


//////////////////////////////////////////////////
// TEST WAS MADE WITH A MODIFIED SLEEP FUNCTION //
//////////////////////////////////////////////////

int main(int argc, char *argv[]) {
  int pid = fork();
  if(pid == 0)
  {clock_t time_1 = clock();
  printf("SYSCALL CLOCK has been started and  the return value is: %d\n", time_1/CLOCKS_PER_SEC);

  sleep(10);
  clock_t time_2 = clock();
  printf("This is the time now after 10s sleep: %d\n",time_2/CLOCKS_PER_SEC);
  }

  return 0;
}