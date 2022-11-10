#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <wait.h>

// this test checks the basic functionality of wait_pid
// not working yet

int main(int argc, char** argv)
{
  printf("Call to wait_pid!\n");
  int pid = fork();
  if(pid != 0)
  {
    int p_id = waitpid((pid_t) -1,(int*) 1222,22222);
    if(p_id < 0)
    {
      printf("some error happend\n");
      return -1;
    }
    else
      printf("Waited for child with pid: %d\n", p_id);
  }
  printf("Finished the task, my pid: %d\n", pid);
  return 0;
}
