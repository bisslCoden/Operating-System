#include "stdio.h"
#include "pthread.h"
#include "stdlib.h"
#include "assert.h"

pthread_t tid_1, tid_2;
pthread_mutex_t mutex1;
pthread_mutex_t mutex2;
// deadlock creation
void *thread1_deadlock_thread2() {
  pthread_mutex_lock(&mutex1);
  sleep(2);
  pthread_mutex_lock(&mutex2);
  pthread_mutex_unlock(&mutex2);
  pthread_mutex_lock(&mutex1);
  return (void *) 111;
}

void *thread2_deadlock_thread1() {
  pthread_mutex_lock(&mutex2);
  sleep(2);
  pthread_mutex_lock(&mutex1);
  pthread_mutex_lock(&mutex1);
  pthread_mutex_lock(&mutex2);
  return (void *) 111;
}

int main(int argc, char *argv[]) {

  pthread_mutex_init(&mutex1, NULL);
  pthread_mutex_init(&mutex2, NULL);
  int create1_retval = pthread_create(&tid_1, NULL, thread1_deadlock_thread2, NULL);
  int create2_retval = pthread_create(&tid_2, NULL, thread2_deadlock_thread1, NULL);

  int join1 = pthread_join(tid_2, NULL);
  printf("Join of thread1 trying to deadlock itself returns = %d\n", join1);
  int join2 = pthread_join(tid_1, NULL);
  printf("Join of thread2 trying to deadlock itself returns = %d\n", join2);


  assert((create1_retval == 0 && create2_retval == 0) && "Thread1 didnt get created successfully!\n"); ///
  getchar();
  return 0;
}