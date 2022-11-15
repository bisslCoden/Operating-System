#include <stdio.h>
#include <unistd.h>

#define FORKS 5

int main(int argc, char** argv)
{
  pid_t pid = 42;
  for(int i = 0; i < FORKS; i++)
  {
    pid = fork();
      for(int i = 0; i < 10000; i++)
      {
        if((i % 1000) == 0)
          printf("counted to 1000, pid: %ld\n", pid);
      }
  }
}
