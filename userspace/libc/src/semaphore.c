#include "semaphore.h"


/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int sem_wait(sem_t *sem)
{
  if(checkAdress((void*) sem, 0) != 0)
    return -1;
  
  if (sem->initialized_ != 1)
  {
    printf("tried to wait on uninitialized sem!\n");
    return -1;
  }
  assert(pthread_mutex_lock(&sem->counter_lock_) == 0);
  if (sem->counter_ > 0)
  {
    sem->counter_--;
    assert(pthread_mutex_unlock(&sem->counter_lock_) == 0);
  }
  else
  {
    while (sem->counter_ == 0)
    {
      assert(pthread_mutex_unlock(&sem->counter_lock_) == 0);
      assert(pthread_mutex_lock(&sem->condition_lock_) == 0);
      assert(pthread_cond_wait(&sem->sem_cond_,&sem->condition_lock_)== 0);
      printf("got woken up\n");
      assert(pthread_mutex_unlock(&sem->condition_lock_) == 0);
      assert(pthread_mutex_lock(&sem->counter_lock_) == 0);
    }
    sem->counter_--;
    assert(pthread_mutex_unlock(&sem->counter_lock_) == 0);
  }
  return 0;  
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int sem_trywait(sem_t *sem)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int sem_init(sem_t *sem, int pshared, unsigned value)
{
  if(checkAdress((void*) sem, 0) != 0)
    return -1;
  if (sem->initialized_ == 1)
  {
    return -1;
  }
  assert(pthread_cond_init(&sem->sem_cond_, 0) == 0);
  assert(pthread_mutex_init(&sem->counter_lock_, 0) == 0);
  assert(pthread_mutex_init(&sem->condition_lock_, 0) == 0);
  sem->counter_ = value;
  sem->pshared_ = pshared;
  sem->initialized_ = 1;
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int sem_destroy(sem_t *sem)
{
  if(checkAdress((void*) sem, 0) != 0)
    return -1;
  if (sem->initialized_ != 1)
  {
    return -1;
  }
  sem->initialized_ = 0;
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int sem_post(sem_t *sem)
{
  if(checkAdress((void*) sem, 0) != 0)
    return -1;
  if (sem->initialized_ != 1)
  {
    printf("tried to post on uninitialized sem!\n");
    return -1;
  }

  assert(pthread_mutex_lock(&sem->counter_lock_) == 0);
  sem->counter_++;
  assert(pthread_mutex_unlock(&sem->counter_lock_) == 0);
  assert(pthread_cond_signal(&sem->sem_cond_) == 0);
  return 0;
    
}


