#include <stdio.h>
#include <unistd.h>

// 7 forks is max
#define FORKS 9

int main(int argc, char** argv)
{
  pid_t pid = 42;
  for(int i = 0; i < FORKS; i++)
  {
    pid = fork();
  }

  for(int i = 0; i < 10000; i++)
  {
    if((i % 5000) == 0)
      printf("counted half way. pid: %ld\n", pid);
  }
}
