#include "pthread.h"
#include "stdio.h"

int main()
{
  printf("will call fork now\n");
  int ret = fork();
  printf("called fork, returned %d\n", ret);

  printf("forked, now will call exec(forkthenexec)\n");
  execv("usr/forkcow.sweb", NULL);

  printf("ERROR AFTER EXEC??");
  return 0;
}
