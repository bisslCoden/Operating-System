#include <stdio.h>
#include <unistd.h>
#include <assert.h>

// will be improved as new features are added, or dev_dominik knows wtf hannes and thomas are doing with the posix things

int main(int argc, char** argv)
{
  int max_calls = 1;
  int pid = fork();
  for (int i = 0; i < max_calls; i++)
  {
    if  (pid == 0){
      pid = fork();
      printf("Child\n");
    }else{
      printf("Parent\n");
    }
  }
  printf("Test finished!\n");
  return 0;
}
