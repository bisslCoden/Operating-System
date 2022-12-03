#include <unistd.h>
#include <stdio.h>
#include <wait.h>


int main(void)
{

  pid_t first = fork();

  pid_t second = fork();
  if(second == 0){
    int i = 5;
    while(i){
      sleep(1);
      i--;
    };
  }

  pid_t third = fork();

  if(first !=0 && second != 0 && third != 0)
  {
    int status;
    printf("Waiting...\n");
    waitpid(first, &status, 0);
    printf("First pid: %ld, child done: %d\n",first, status);
    waitpid(second, &status, 0);
    printf("Second  pid: %ld, child done : %d\n",second, status);
    waitpid(third, &status, 0);
    printf("Third pid: %ld,  child done : %d\n", third, status);
  }
  return 1;
}
