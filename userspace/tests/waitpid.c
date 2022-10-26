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
  for(int i = 0; i < 10; i++)
  {
    int pid_c = fork();
    if(pid_c == 0)
    {
      int p_id = waitpid(3, 0, 0);
      if(p_id < 0)
      {
        printf("some error happend\n");
        return -1;
      }
      else
        printf("Waited for child with pid: %d\n", p_id);
    }     
  }
  printf("Finished the task, my pid: %d\n", pid);
  return 0;
}
