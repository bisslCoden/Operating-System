#include <stdio.h>
#include <unistd.h>

// 6 forks is max that works consistenty 
#define FORKS 6

int main(int argc, char** argv)
{
  pid_t pid = 42;
  for(int i = 0; i < FORKS; i++)
  {
    pid = fork();
  }

  /*for(int i = 0; i < 10000; i++)
  {
    if((i % 5000) == 0)
      printf("counted half way. pid: %ld\n", pid);
  }*/

  if(pid)
    printf("parent from child [%ld] here\n", pid);
  else  
    printf("some child returning...\n");
  return 0;
}
