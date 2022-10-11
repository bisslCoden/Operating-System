#include "stdio.h"
#include "pthread.h"
int main(int argc, char *argv[]) {
  printf("Threadcount: %d", get_thread_count());
  return 0;
}