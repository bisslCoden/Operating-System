#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <wait.h>

// this test checks the basic functionality of wait_pid
// not working yet

int main(int argc, char** argv)
{
  printf("Call to wait_pid!\n");
  int p_id = waitpid((pid_t) 15,(int*) 1222,22222);
  printf("pid = %d\n", p_id);
  printf("Call to wait_pid finished!\n");
  return 0;
}
