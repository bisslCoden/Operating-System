#include "stdio.h"
#include "pthread.h"
#include "stdlib.h"
#include "assert.h"


#define USER_BREAK 0x0000F00000000000ULL

void *foo(void* arg) {
  printf("foo\n");
  return (void *) 123;
}


int main(int argc, char *argv[]) {
  int init = 3;


  int pcreate_retval;

  switch (init){
    case 1:
      //pass NULLs
      pcreate_retval = pthread_create(NULL, NULL, NULL, NULL);
      assert(pcreate_retval == 0 && "failed, cannot pass NULL as args\n");
      printf("Thread created!\n");
      break;
    case 2:
      //pass the USERBREAK
      pcreate_retval = pthread_create((pthread_t *) USER_BREAK, (const pthread_attr_t *) USER_BREAK,(void *(*)(void *)) USER_BREAK, (void *) USER_BREAK);
      assert(pcreate_retval == 0 && "failed, cannot create thread above USER_BREAK\n");
      printf("Thread created!\n");
      break;
    case 3:
      //pass address of number
      pcreate_retval = pthread_create((pthread_t *) 3, (const pthread_attr_t *) 3,(void *(*)(void *)) 3, (void *) 3);
      assert(pcreate_retval == 0 && "failed, cannot pass the address of a number\n");
      printf("Thread created!\n");
  }

  getchar();

  return 0;
}