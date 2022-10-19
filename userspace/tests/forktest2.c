#include <stdio.h>
#include <unistd.h>
#include <assert.h>

// this test checks the basic functionality of fork

int main(int argc, char** argv)
{
  printf("Call to Fork!\n");
  int pid = fork();
  int pid_2 = fork();
  int pid_3 = fork();
  if (!pid)
  {
    printf("[%d] I am the Child!\n", pid);
  }
  if (!pid_2)
  {
    printf("[%d] I am the Child!\n", pid_2);
  }
  if (!pid_3)
  {
    printf("[%d] I am the Child!\n", pid_3);
  }
  if(pid > 0 || pid_2 > 0 || pid_3 > 0)
  {
    printf("[%d] I am the Parent!\n", pid_3);
  }
  printf("Call to Fork finished!\n");
  return 0;
}
