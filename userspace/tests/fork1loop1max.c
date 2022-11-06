#include <stdio.h>
#include <unistd.h>
#include <assert.h>

// this test checks the basic functionality of fork

int main(int argc, char** argv)
{
  int max_calls = 6;
  for (int i = 0; i < max_calls; i++)
  {
    int pid = fork();
    if  (pid == 0){
      printf("Child\n");
    }else{
      printf("Parent\n");
    }
  }
  printf("Test finished!\n");
  return 0;
}
