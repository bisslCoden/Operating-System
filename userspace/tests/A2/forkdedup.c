#include "stdio.h"
#include "unistd.h"
#include "sched.h"

#define NUM_PROGS 10


int data_init = 17;
const char* yey = "yes!"; 
char* const pathdedup = "/usr/dedup.sweb";

int main()
{
  for (size_t i = 0; i < NUM_PROGS; i++)
  {
    int pid = fork();
    if (pid == 0)
    {
      execv(pathdedup, NULL);
    }
  }
  return 0;
}