#include "stdio.h"
#include "pthread.h"
#include "stdlib.h"
#include "assert.h"

pthread_t tid_1, tid_2;

// deadlock creation
void *thread1_deadlock_thread2() {
  sleep(2);
  int join1 = pthread_join(tid_2, NULL);
  printf("Join of thread1 trying to deadlock itself returns = %d\n", join1);

  return (void *) 111;
}

void *thread2_deadlock_thread1() {
  sleep(2);
  int join2 = pthread_join(tid_1, NULL);
  printf("Join of thread2 trying to deadlock itself returns = %d\n", join2);

  return (void *) 111;
}

int main(int argc, char *argv[]) {
  int create1_retval = pthread_create(&tid_1, NULL, thread1_deadlock_thread2, NULL);
  int create2_retval = pthread_create(&tid_2, NULL, thread2_deadlock_thread1, NULL);

  assert((create1_retval == 0 && create2_retval == 0) && "Thread1 didnt get created successfully!\n"); ///
  getchar();
  return 0;
}