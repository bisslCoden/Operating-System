#include "pthread.h"
#include "syscall.h"
#include "sched.h"
#include "../../../common/include/kernel/syscall-definitions.h"
#include "assert.h"
#include "stdio.h"


/**
 * @brief we are in userspace. now the start routine + arguments gets called and after it's done, pthread_exit is called.
 * 
 * @param start_routine the routine that pthread_create should start
 * @param args the args
 * @return void* nothing bruh
 */
void* wrapper(void* (*start_routine)(void*), void* args)
{
  pthread_exit(start_routine(args));
  return 0;
}
/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
  return __syscall(sc_pthread_create, (size_t)thread, (size_t)attr, (size_t)start_routine, (size_t)arg, (size_t)wrapper);
  // wrapper defined above
}

size_t atomic_exchange_0(size_t *lock){
  size_t myval = 0;
  asm("xchg %0, %1\n\t" : "=r" (myval) : "m" (*lock), "0" (myval) : "memory" );
  return myval;
};

size_t atomic_exchange_1(size_t* lock){
  size_t myval = 1;
  asm("xchg %0, %1\n\t" : "=r" (myval) : "m" (*lock), "0" (myval) : "memory" );
  return myval;
}



/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
  pthread_spin_init(&mutex->sleeperslist_lock_, 0);
  mutex->lock_ = 1;
  mutex->my_attr_ = (attr == 0) ? (pthread_mutexattr_t) 0 : *attr;
  mutex->firstsleeper_ = 0;
  mutex->initialized_ = 1;
  return -1;
}

size_t findStackStackStart(size_t inputadress){
  size_t outputadress = inputadress >> 12;
  outputadress = outputadress << 12;
  outputadress += PAGE_SIZE - sizeof(size_t);
  return outputadress;
}

//lock beforeee
void addToWaitersList(pthread_mutex_t* mutex, size_t** localvaradress)
{
  if(mutex->firstsleeper_ == 0)
  {
    mutex->firstsleeper_ = (size_t*) localvaradress;
    return;
  }

  size_t* iter = mutex->firstsleeper_;
  while (*iter != 0)
  {
    //printf("iter = %p and points to %p\n", iter, (size_t*) *iter);
    iter = (size_t*) *iter;
  }
  *iter = (size_t) localvaradress;
  return;
}

int pthread_setcancelstate(int state, int *oldstate){
  return __syscall(sc_pthread_setcancelstate, (size_t) state, (size_t) oldstate, 0x0, 0x0, 0x0);
}
int pthread_setcanceltype(int type, int *oldtype){
  return __syscall(sc_pthread_setcanceltype, (size_t) type, (size_t) oldtype, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
void pthread_exit(void *value_ptr)
{
  __syscall(sc_pthread_exit, (size_t) value_ptr, 0x0, 0x0, 0x0, 0x0);
  return;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cancel(pthread_t thread)
{
  return __syscall(sc_pthread_cancel, (size_t) thread, 0x0, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_join(pthread_t thread, void **value_ptr)
{
  return __syscall(sc_pthread_join, (size_t) thread, (size_t) value_ptr, 0x0, 0x0, 0x0);
  
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_detach(pthread_t thread)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
  return -1;
}

//userstack:
//0x672bfffffff0
//flagadress:
//0x672c00000000

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
  if(mutex->initialized_ != 1)
    __syscall(sc_exit, (size_t) -1, 0x0, 0x0, 0x0, 0x0);

  pthread_spin_lock(&mutex->sleeperslist_lock_);
  if(mutex->firstsleeper_ == 0)
  {
    if (atomic_exchange_0(&mutex->lock_))
    {
      pthread_spin_unlock(&mutex->sleeperslist_lock_);
      return 0;
    }
  }  //didnt get the lock now i ll sleep
  
  size_t* next = 0;
  addToWaitersList(mutex, &next);
  size_t setsleep = findStackStackStart((size_t) &next);
  assert(atomic_exchange_1((size_t*) setsleep) == 0 && "tried to sleep but was alreafy?\n");
  //printf("added myself to sleep list, setting sleep at %p and there now is %ld\n", (size_t*) setsleep, *(size_t*)setsleep);
  pthread_spin_unlock(&mutex->sleeperslist_lock_);
  sched_yield();

  pthread_spin_lock(&mutex->sleeperslist_lock_);
  //printf("flag is now agein free? %ld\n", mutex->lock_);
  assert(atomic_exchange_0(&mutex->lock_) == 1 && "huh? got woken up but still cant get a lock!\n");
  mutex->firstsleeper_ = next;
  pthread_spin_unlock(&mutex->sleeperslist_lock_);
  return 0;
}

//0x685bfffffff0
//0x685c00000000
/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
  //size_t adress = 0x685c00000000 - 8;
  if(mutex->initialized_ != 1)
    __syscall(sc_exit, (size_t) -1, 0x0, 0x0, 0x0, 0x0);

  pthread_spin_lock(&mutex->sleeperslist_lock_);
  if(mutex->firstsleeper_ != 0){
  
    size_t setwake = findStackStackStart((size_t) mutex->firstsleeper_);
    assert(atomic_exchange_0((size_t*) setwake) == 1 && "tried to wake but was alreay woke?\n");
    // mutex->firstsleeper_ = (size_t*) *mutex->firstsleeper_;
  }
  assert(atomic_exchange_1(&mutex->lock_) == 0 && "whaat? lock was free when unlocking\n");
  //printf("flag is now again unlocked: %ld\n", mutex->lock_);
  pthread_spin_unlock(&mutex->sleeperslist_lock_);
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_destroy(pthread_cond_t *cond)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_signal(pthread_cond_t *cond)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_broadcast(pthread_cond_t *cond)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_destroy(pthread_spinlock_t *lock)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_init(pthread_spinlock_t *lock, int pshared)
{
  lock->mylock_ = 1;
  lock->initialized_ = 1;
  lock->pshared_ = pshared;
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_lock(pthread_spinlock_t *lock)
{
  if (lock->initialized_ != 1)
  {
    __syscall(sc_exit, (size_t) -1, 0x0, 0x0, 0x0, 0x0);
  }
  
  while (!atomic_exchange_0(&lock->mylock_))
  {
    //not suuper safe but okay for now
    sched_yield();
  }
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_trylock(pthread_spinlock_t *lock)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_unlock(pthread_spinlock_t *lock)
{
  if(atomic_exchange_1(&lock->mylock_) != 0)
    return -1;
  return 0;
}

