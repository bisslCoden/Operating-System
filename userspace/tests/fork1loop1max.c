#include <stdio.h>
#include <unistd.h>
#include <assert.h>

// this test checks the basic functionality of fork
// maximal 7 working now, cuz at 8 max kernel heap size reached...
int main(int argc, char** argv)
{
  int max_calls = 7;
  for (int i = 0; i < max_calls; i++) {
    int pid = fork();

    if (pid == 0)
      printf("Hello from Child!\n");
    else
      printf("Hello from Parent!\n");
  }
  printf("Test Done!\n");
  return 0;
}
