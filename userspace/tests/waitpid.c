#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <wait.h>
#include "getpid.h"
// this test checks the basic functionality of wait_pid
// not working yet

int main(int argc, char** argv)
{
    printf("Call to wait_pid!\n");
    int pid = fork();
    pid = fork();
    pid = fork();
    pid = fork();
    pid = fork();
    pid = fork();

    printf("pid : %d, and mine process id: %d\n", pid, getpid());
    if(pid != 0)
    {
      printf("pid before waitpid: %d\n", pid);
      int p_id = waitpid(pid, 0, 0);
      printf("pid before if: %d\n", pid);
      if(p_id < 0 || pid < 0)
      {
        printf("some error happend\n");
        return -1;
      }
      else
      {
       printf("Waited for child with pid: %d\n", p_id);    
      }
    printf("Finished the task, my pid: %d\n", getpid());
  }
  
  return 0;
}
