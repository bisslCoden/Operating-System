#include <stdio.h>
#include <pthread.h>

size_t p;

void* secondThread() {
  size_t ret_value;
  printf("WAITING - THREAD 2\n");
  ret_value = pthread_join(p, NULL);
  printf("%ld\n", ret_value);
  return NULL;
}


void* firstThread() {
  size_t ret_value, tid;
  printf("WAITING - THREAD 1\n");
  sleep(10);

  pthread_create(&tid, NULL, secondThread, NULL);
  ret_value = pthread_join(tid, NULL);
  printf("%ld\n", ret_value);
  return NULL;
}


int main() {
  p = pthread_self();
  size_t tid;
  pthread_create(&tid, NULL, firstThread, NULL);
  pthread_join(tid, NULL);

  return 0;
}