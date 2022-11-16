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
  printf("waitpid without fork\n");
  printf("0\n");
  int p_id = waitpid(1, NULL, NULL);
  printf("Return_value %d last pid: %d \n", p_id, getpid());
  int pid = fork();
  pid = fork();
  printf("1\n");
  p_id = waitpid(pid, NULL, NULL);
  printf("Return_value %d last pid: %d \n", p_id, getpid());

  printf("2\n");
  p_id = waitpid(NULL, (int*) -1, NULL);
  printf("Return_value %d last pid: %d \n", p_id, getpid());

  printf("3\n");
  p_id = waitpid(NULL, NULL, -1);
  printf("Return_value %d last pid: %d \n", p_id, getpid());

  printf("4\n");
  p_id = waitpid(NULL, NULL, NULL);
  printf("Return_value %d last pid: %d \n", p_id, getpid());

  printf("5\n");
  p_id = waitpid(0, 0, 0);
  printf("Return_value %d last pid: %d \n", p_id, getpid());

  printf("6\n");
  p_id = waitpid(-1, (int*)-1, -1);
  printf("Return_value %d last pid: %d \n", p_id, getpid());

  printf("7\n");
  p_id = waitpid(-1123, (int*)4, 123);
  printf("Return_value %d last pid: %d \n", p_id, getpid());

  printf("8\n");
  p_id = waitpid(142, (int*)-400, 232);
  printf("Return_value %d last pid: %d \n", p_id, getpid());

  printf("9\n");
  p_id = waitpid(142, (int*)400, -232);
  printf("Return_value %d last pid: %d \n", p_id, getpid());

  printf("10\n");
  p_id = waitpid(47264, (int*)1234124, 1234123);
  printf("Return_value %d last pid: %d \n", p_id, pid);
  printf("Finished the task, my pid: %d\n", getpid());
  return 0;
}

