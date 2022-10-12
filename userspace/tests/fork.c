#include <stdio.h>
#include <unistd.h>
#include <assert.h>

// this test checks the basic functionality of fork

int main(int argc, char** argv)
{
  int pid = fork();
  printf("Hello there\n");
  printf("pid: %d\n", pid);
  if (pid == 0)
  {
    printf("[%d] Hello from Child!\n", pid);
  }
  else
  {
    printf("[%d] Hello from Parent!\n", pid);
    assert (pid != 0);  // hello
  }
  return 0;
}
