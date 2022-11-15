#include <stdio.h>
#include <stdlib.h>
#include "assert.h"

// Test for fork with exec

int main(void) {
  char* const path = "/usr/simpleforktest.sweb";
  char* const args[] = {NULL};

  printf("Start test!\n");
  int pid = fork();

  if (pid == 0) {
    printf("Hello from Child!\n");
    int ret_exec = execv(path, args);
    printf("[FAIL] If this is being printed, something is wrong.\n");
    assert(0 && "Execv failed\n");
  }
  else {
    printf("Hello from Parent!\n");
  }

  printf("END\n");
}
