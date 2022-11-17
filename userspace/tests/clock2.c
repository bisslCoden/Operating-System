#include "stdio.h"
#include "time.h"
#include "getpid.h"
int main(int argc, char *argv[]) {
  pid_t pid = fork();
  pid = fork();
  pid = fork();
  pid = fork();
  clock_t time_1 = clock();
  //printf("SYSCALL CLOCK has been started and  the return value is in s:  %d\n", time_1/CLOCKS_PER_SEC);
  //printf("SYSCALL CLOCK has been started and  the return value is in ms: %d\n", time_1);
  if(pid == 0)
  {
    time_1 = clock();
    //printf("SYSCALL CLOCK has been started in fork and  the return value is in s:  %d\n", time_1/CLOCKS_PER_SEC);
    //printf("SYSCALL CLOCK has been started in fork and  the return value is in ms: %d\n", time_1);

    for(int x = 0; x < 10; x++)
    {
      x = x + getpid();
      x = x - getpid();
    }
    clock_t time_2 = clock();
    printf("DIFFERENCE FOR 10 GEPITD IN CLOCK IS IN MS: %d\n", time_2 - time_1) ;
  }
  if(pid != 0)
  {
    time_1 = clock();
   //printf("SYSCALL CLOCK has been started in parent and the return value is in s:  %d\n", time_1/CLOCKS_PER_SEC);
    //printf("SYSCALL CLOCK has been started in parent and  the return value is in ms: %d\n", time_1);
    time_1 = clock();
    //printf("SYSCALL CLOCK has been finished in parent and the return value is in s:  %d\n", time_1/CLOCKS_PER_SEC);
    //printf("SYSCALL CLOCK has been finished in parent and  the return value is in ms: %d\n", time_1);


  }

  return 0;
}