#include <stdio.h>
#include <unistd.h>
#include <assert.h>

// this test checks the basic functionality of fork

int main(int argc, char** argv)
{
  printf("Call to Fork!\n");
  int pid = fork();
  int arr [2000];
  for (size_t i = 0; i < 2000; i++)
  {
    arr[i] = 9;
  }
  printf("arr: %d\n", arr[0]);
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
    printf("Error while forking!\n");
  }
  //printf("just again the pid: %d\n", pid);

  // if (!pid)
  // {
  //   printf("[%d] I am the Child!
  // else if (pid > 0)
  return 0;
}