#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <wait.h>
#include "getpid.h"
// this test checks the basic functionality of wait_pid
// not working yet

int main()
{
    printf("Call to wait_pid!\n");
    int pid = fork();
    int pid_id = 0;
    if(pid != 0)
    {
  //    printf("pid before waitpid: %d\n", pid);
      pid_id = waitpid(pid, 0, 0);
    }
    if (pid_id)
    {
      printf("I waited and got %d from my child!\n", pid_id);
    }
    


  return 0;
}
