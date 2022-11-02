#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <wait.h>
#include "getpid.h"
// this test checks the basic functionality of wait_pid
// not working yet

int main(int argc, char** argv)
{
  printf("Started the task, my pid: %d\n", getpid());
  int pid = fork();
  pid = fork();
  pid = fork();
  pid = fork();
  printf("1\n");
  int p_id = waitpid(-1, NULL, NULL);
  printf("Return_value %d last pid: %d \n", p_id, pid);

  printf("2\n");
  p_id = waitpid(NULL, (int*) -1, NULL);
  printf("Return_value %d last pid: %d \n", p_id, pid);

  printf("3\n");
  p_id = waitpid(NULL, NULL, -1);
  printf("Return_value %d last pid: %d \n", p_id, pid);

  printf("4\n");
  p_id = waitpid(NULL, NULL, NULL);
  printf("Return_value %d last pid: %d \n", p_id, pid);

  printf("5\n");
  p_id = waitpid(0, 0, 0);
  printf("Return_value %d last pid: %d \n", p_id, pid);

  printf("6\n");
  p_id = waitpid(-1, (int*)-1, -1);
  printf("Return_value %d last pid: %d \n", p_id, pid);

  printf("7\n");
  p_id = waitpid(47264, (int*)1234124, 1234123);
  printf("Return_value %d last pid: %d \n", p_id, pid);
  printf("Finished the task, my pid: %d\n", getpid());
  return 0;
}
