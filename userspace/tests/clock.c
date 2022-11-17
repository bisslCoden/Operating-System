#include "stdio.h"
#include "time.h"

int main(int argc, char *argv[]) {
  int pid = fork();
  if(pid == 0)
  {
    clock_t time_1 = clock();
    printf("[CHILD] CLOCK has been started and the return value is in sec: %d\n", time_1/CLOCKS_PER_SEC);
    printf("[CHILD] CLOCK has been started and in micro sec:               %d\n", time_1);

    sleep(10);
    clock_t time_2 = clock();
    printf("[CHILD] This is the time now after 10s in mikrosec: %d\n",time_2);
    printf("[CHILD] This is the time now after 10s sleep:       %d\n",time_2/CLOCKS_PER_SEC);
  }
  if(pid != 0)
  {
    clock_t time_1 = clock();
    printf("[PARENT] CLOCK has been started and the return value is in sec: %d\n", time_1/CLOCKS_PER_SEC);
    printf("[PARENT] CLOCK has been started and in micro sec:               %d\n", time_1);

    sleep(10);
    clock_t time_2 = clock();
    printf("[PARENT] is the time now after 10s in mikrosec: %d\n",time_2);
    printf("[PARENT] is the time now after 10s sleep:       %d\n",time_2/CLOCKS_PER_SEC);
  }

  return 0;
}