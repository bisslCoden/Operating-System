#include <stdio.h>
#include <unistd.h>
#include <assert.h>

// this test checks the basic functionality of fork

int main(int argc, char** argv)
{
  printf("Call to Fork!\n");
  int pid = fork();
  printf("pid: %d\n", pid);
  if (!pid)
  {
    printf("[%d] I am the Child!\n", pid);
  }
  else if (pid > 0)
  {
    printf("[%d] I am the Parent!\n", pid);
    assert (pid != 0);  // hello
  }else
  {
    printf("Error while forking!");
  }
  return 0;
}
