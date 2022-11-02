#include <stdio.h>
#include <unistd.h>
#include <assert.h>

// this test checks the basic functionality of fork

int main(int argc, char** argv)
{
  printf("Call to Fork w 3!\n");
  int pid = 0;
  for(int i = 0; i < 1; i++)
    pid = fork();
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

  // if (!pid)
  // {
  //   printf("[%d] I am the Child!
  // else if (pid > 0)
  // {
  //   printf("[%d] I am the Parent!\n", pid);
  //   assert (pid != 0);  // hello
  // }else
  // {n", pid);
  // }
  //   printf("Error while forking!");
  // }

   /*printf("Call to Fork w 5!\n");
  for(int i = 0; i < 5; i++)
    pid = fork();
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

  printf("Call to Fork w 10!\n");
  for(int i = 0; i < 10; i++)
    pid = fork();
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
  printf("Call to Fork finished!\n");*/

  printf("main() returning\n");
  return 0;
}
