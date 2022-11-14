#include <stdio.h>
#include <unistd.h>

int main()
{
  int pid = fork();
  for (int i = 0; i < 5; i++) {


    if (pid == 0)
    {
      pid = fork();
      printf("Hello from Child!\n");
    }
    else
    {
      printf("Hello from Parent!\n");
    }
  }
  printf("hello \n");
  return 0;
}
