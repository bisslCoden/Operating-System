#include <stdio.h>
#include <unistd.h>
#include <assert.h>

// this test checks the basic functionality of fork
// maximal 6 working now, cuz at 8 max kernel heap size reached...
int main(int argc, char** argv)
{
  int pid = 10;
  int max_calls = 6;
  for (int i = 0; i < max_calls; i++) {
    int pid = fork();


  }
  if (pid == 0)
    printf("Hello from Child! %d\n" ,pid);
  else
    printf("Hello from Parent! %d\n",pid);
  printf("Test Done!\n");
  return 0;
}
