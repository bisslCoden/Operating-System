#include <stdio.h>
#include <unistd.h>
#include <assert.h>

// testing the correctness of cow

int main(int argc, char** argv)
{
  int test[10];
  int max_calls = 4;
  for (int i = 0; i < max_calls; i++)
  {
    test[i] = i + 5;
    printf("Test currently at i = %d\n",test[i]);
  }

  int pid = fork();

  if  (pid == 0){
    printf("Child\n");
    for (int i = 0; i < max_calls; i++)
    {
      printf("Test currently in Child at i = %d\n",test[i]);
    }
    test[0] = 0;
    printf("Test currently at i = %d\n",test[0]);
  }else{
    printf("Parent\n");
    for (int i = 0; i < max_calls; i++)
    {
      test[i] = i + 2;
      printf("Test currently in Parent at i = %d\n",test[i]);
    }
  }
  printf("Test finished!");
  return 0;
}
