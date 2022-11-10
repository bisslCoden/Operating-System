#include <unistd.h>
#include <stdio.h>
#include <wait.h>


int main(void)
{

  pid_t first = 7;
  first = fork();

  if(first == 0)
  {
    printf("First child: %ld\n", first);
    return 1;
  }
  pid_t second = fork();
  if(second == 0)
  {
    printf("second child: %ld\n", second);
    //while(1);
    return 2;
  }
  pid_t third = fork();
  if(third == 0)
  {
    printf("third child: %ld\n", third);
    return 3;
  }
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
  return 123;
}
