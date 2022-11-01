#include "pthread.h"
#include "syscall.h"
#include "sched.h"
#include "../../../common/include/kernel/syscall-definitions.h"


//not rly that threadsafe i think...
pthread_mutex_t* firstmutex = (pthread_mutex_t*) 0;
pthread_spinlock_t mutexlist_lock;


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


size_t findStackStackStart(size_t inputadress){
  size_t outputadress = inputadress >> 12;
  outputadress = outputadress << 12;
  outputadress += PAGE_SIZE_US - sizeof(size_t);
  return outputadress;
}

int isWaitingHere(size_t identifier, pthread_mutex_t* mutex){
  pthread_spin_lock(&mutex->sleeperslist_lock_);
  if (mutex->firstsleeper_ == 0)
  {
    pthread_spin_unlock(&mutex->sleeperslist_lock_);
    return 0;
  }
  size_t* iter = mutex->firstsleeper_;
  printf("lock: %p: ", mutex);
  while (iter != 0)
  {
    if (findStackStackStart((size_t)iter) == identifier)
    {
      pthread_spin_unlock(&mutex->sleeperslist_lock_);
      return 1;
    }
    printf(" %p ", (size_t*)findStackStackStart((size_t) iter));
    iter = (size_t*) *iter;
  }
  printf("\n");
  pthread_spin_unlock(&mutex->sleeperslist_lock_);
  return 0;
}

//to be continued...
int detectThreadWaitingAnotherLock(size_t identifier, pthread_mutex_t* desired_lock){
  return -1;
}



int detectThreadWaiting(size_t waiter_identifier, pthread_mutex_t* lock_wanted){
  
  for (pthread_mutex_t* iter = firstmutex; iter->next_mutex_ != 0; iter = iter->next_mutex_)
  {
    if (iter == lock_wanted)
      continue;
    
    if (isWaitingHere(waiter_identifier, iter))
    {
      assert(0 && "how?? thread is waiting on another lock already??\n");
    }
  }
  return 0;
}
  


int detectCircularDeadlock(size_t waiter_identifier, pthread_mutex_t* lock_wanted){
  pthread_spin_lock(&lock_wanted->held_by_lock_);
  size_t held_by_cur = lock_wanted->held_by_;
  
  for (pthread_mutex_t* iter = firstmutex; iter->next_mutex_ != 0; iter = iter->next_mutex_)
  {
    printf("checking dl for mutex: %p held by %p\n", iter, (size_t*) iter->held_by_);
    if (iter == lock_wanted)
      continue;
    
    pthread_spin_lock(&iter->held_by_lock_);
    size_t held_by = iter->held_by_;
    if (held_by == waiter_identifier && isWaitingHere(held_by_cur, iter))
    {
      printf("CIRCULAR deadlock found!\n");
      int circ = 1;
      assert(circ == 0);
    }
    pthread_spin_unlock(&iter->held_by_lock_);           
    }
    pthread_spin_unlock(&lock_wanted->held_by_lock_);
    return 0;
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


int checkAdress(void* adress){
  if (adress < (void*) 0x1000 || (unsigned long long) adress > 0x0000800000000000ULL)
  {
    printf("invalid address!\n");
    return -1;
  }
  else 
    return 0;
}

//lock beforeee
int addToWaitersList(pthread_mutex_t* mutex, size_t** localvaradress)
{
  if(mutex->firstsleeper_ == 0)
  {
    mutex->firstsleeper_ = (size_t*) localvaradress;
    return 0;
  }

  size_t* iter = mutex->firstsleeper_;
  while (*iter != 0)
  {
    //printf("iter = %p and points to %p\n", iter, (size_t*) *iter);
    if ((size_t**)*iter == localvaradress)
    {
      return -1;
    }
    
    iter = (size_t*) *iter;
  }
  *iter = (size_t) localvaradress;
  return 0;
}

int addToCVWaitersList(pthread_cond_t* cond, size_t** localvaradress)
{
  if(cond->firstsleeper_ == 0)
  {
    cond->firstsleeper_ = (size_t*) localvaradress;
    return 0;
  }

  size_t* iter = cond->firstsleeper_;
  while (*iter != 0)
  {
    //printf("iter = %p and points to %p\n", iter, (size_t*) *iter);
    if ((size_t**)*iter == localvaradress)
    {
      return -1;
    }
    
    iter = (size_t*) *iter;
  }
  *iter = (size_t) localvaradress;
  return 0;
}




/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
  if(checkAdress((void*) thread) != 0 || checkAdress((void*)start_routine) != 0)
    return -1;
  if (attr != 0x0)
  {
    if (checkAdress((void*) attr) != 0 || attr->initialized_ != 1)
      return -1;
  }
  
  return __syscall(sc_pthread_create, (size_t)thread, (size_t)attr, (size_t)start_routine, (size_t)arg, (size_t)wrapper);
  // wrapper defined above
}




int pthread_attr_init(pthread_attr_t *attr){
  if(checkAdress((void*) attr) != 0)
    return -1;
  if (attr->initialized_ == 1)
    return -1;
  
  attr->detach_state_ = PTHREAD_CREATE_JOINABLE;
  attr->guard_size_ = PAGE_SIZE_US;
  __syscall(sc_pthread_attr_init, (size_t)&attr->stackaddress_, (size_t)&attr->stacksize_, 0x0, 0x0, 0x0);
  attr->initialized_ = 1;
  return 0;
}
int pthread_attr_destroy(pthread_attr_t *attr){
  if(checkAdress((void*) attr) != 0)
    return -1;
  if (attr->initialized_ != 1)
    return -1;
  attr->initialized_ = 0;
  return 0;
  
};

pthread_t pthread_self(void){
  return (pthread_t) __syscall(sc_pthread_self, 0x0, 0x0, 0x0, 0x0, 0x0);
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate){
  
  if(checkAdress((void*) attr) != 0)
    return -1;
  if (detachstate != PTHREAD_CREATE_JOINABLE && detachstate != PTHREAD_CREATE_DETACHED)
    return -1;
  
  attr->detach_state_ = detachstate;
  return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate){
  
  if(checkAdress((void*) attr) != 0 || checkAdress((void*) detachstate)!= 0)
    return -1;
  
  *detachstate = attr->detach_state_;
  return 0;
}




/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
  if(checkAdress((void*) mutex) != 0)
    return -1;
  if (mutex->initialized_ == 1)
    return -1;
  
  pthread_spin_init(&mutex->sleeperslist_lock_, 0);
  pthread_spin_init(&mutex->held_by_lock_, 0);
  mutex->lock_ = 1;
  mutex->my_attr_ = (attr == 0) ? (pthread_mutexattr_t) 0 : ((checkAdress((void*)attr) == 0) ? 
  *attr : (pthread_mutexattr_t)-1);
  if (mutex->my_attr_ == (pthread_mutexattr_t) -1)
    return -1;
  
  mutex->firstsleeper_ = 0;
  mutex->initialized_ = 1;
  mutex->held_by_ = 0;
  if (firstmutex == 0)
  {
    pthread_spin_init(&mutexlist_lock, 0);
    firstmutex = mutex;
  }
  else{
    pthread_spin_lock(&mutexlist_lock);
    pthread_mutex_t* iter = firstmutex;
    for(; iter->next_mutex_ != 0; iter = iter->next_mutex_);
    iter->next_mutex_ = mutex;
    pthread_spin_unlock(&mutexlist_lock);
  } 
  
  return 0;
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
  if(checkAdress((void*) value_ptr) != 0)
    return -1;
  return __syscall(sc_pthread_join, (size_t) thread, (size_t) value_ptr, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_detach(pthread_t thread)
{
  return __syscall(sc_pthread_detach, (size_t) thread, 0x0, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
  if(checkAdress((void*) mutex) != 0)
    return -1;
  if (mutex->initialized_ != 1)
    return -1;
  
  mutex->initialized_ = 0;
  return 0;
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
  if(checkAdress((void*) mutex) != 0)
    return -1;
  if(mutex->initialized_ != 1)
  {
    printf("tried to lock uninitialized mutex!\n");
    return -1;
  } 

  size_t findstart;
  size_t my_identifier = findStackStackStart((size_t)&findstart);
  pthread_spin_lock(&mutex->held_by_lock_);
  if (mutex->held_by_ == my_identifier)
  {
    pthread_spin_unlock(&mutex->held_by_lock_);
    printf("DEADLOCK! you already have this lock!\n");
    return -1;
  }
  pthread_spin_unlock(&mutex->held_by_lock_);


  while (!atomic_exchange_0(&mutex->lock_))
  {
    pthread_spin_lock(&mutex->sleeperslist_lock_);
    size_t* next = 0;
    if(addToWaitersList(mutex, &next) != 0)
    {
      pthread_spin_unlock(&mutex->sleeperslist_lock_);
      printf("DEADLOCK! you already wait on this lock!\n");
      return -1;
    }
    size_t setsleep = findStackStackStart((size_t) &next);
    assert(atomic_exchange_1((size_t*) setsleep) == 0 && "tried to sleep but was alreafy?\n");
    //printf("going to sleep now...\n");
    pthread_spin_unlock(&mutex->sleeperslist_lock_);
    detectThreadWaiting(my_identifier, mutex);
    detectCircularDeadlock(my_identifier, mutex);
    sched_yield();
  }

  pthread_spin_lock(&mutex->held_by_lock_);
  mutex->held_by_ = my_identifier;
  pthread_spin_unlock(&mutex->held_by_lock_);
  /*
  if(mutex->firstsleeper_ == 0)
  {
    if (atomic_exchange_0(&mutex->lock_))
    {
      pthread_spin_lock(&mutex->held_by_lock_);
      mutex->held_by_ = my_identifier;
      pthread_spin_unlock(&mutex->held_by_lock_);
      pthread_spin_unlock(&mutex->sleeperslist_lock_);
      return 0;
    }
  }  //didnt get the lock now i ll sleep
*/
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
  if(checkAdress((void*) mutex) != 0)
    return -1;
  if(mutex->initialized_ != 1)
  {
    printf("tried to unlock uninitialized mutex!\n");
    return -1;
  } 

  size_t findstart;
  size_t my_identifier = findStackStackStart((size_t)&findstart);
  pthread_spin_lock(&mutex->held_by_lock_);
  if (mutex->held_by_ != my_identifier)
  {
    pthread_spin_unlock(&mutex->held_by_lock_);
    printf("What are you trying to unlock here? you dont have this lock\n");
    return -1;
  }
  mutex->held_by_ = 0;
  pthread_spin_unlock(&mutex->held_by_lock_);


  pthread_spin_lock(&mutex->sleeperslist_lock_);
  if(mutex->firstsleeper_ != 0){
    printf("first sleeper ist %p\n", mutex->firstsleeper_);
    size_t setwake = findStackStackStart((size_t) mutex->firstsleeper_);
    assert(atomic_exchange_0((size_t*) setwake) == 1 && "tried to wake but was alreay woke?\n");
    // mutex->firstsleeper_ = (size_t*) *mutex->firstsleeper_;
  }
  mutex->firstsleeper_ = mutex->firstsleeper_ == 0 ? (size_t*) 0 : (size_t*) *mutex->firstsleeper_;
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
  if(checkAdress((void*) cond) != 0)
    return -1;
  if(cond->initialized_ == 1)
    return -1;
  
  pthread_spin_init(&cond->CV_sleeperslist_lock_, 0);
  cond->firstsleeper_ = 0;
  cond->my_attr_ = (attr == 0) ? (pthread_condattr_t) 0 : (checkAdress((void*)attr) == 0 ? *attr : (pthread_condattr_t) -1);
 
  if (cond->my_attr_ == (pthread_condattr_t) -1)
    return -1;
  
  cond->threads_waiting_ = 0;
  cond->lock_ = 1;
  cond->initialized_ = 1;
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_destroy(pthread_cond_t *cond)
{
  if(checkAdress((void*) cond) != 0)
    return -1;
  if (cond->initialized_ != 1)
  {
    return -1;
  }  
  cond->initialized_ = 0;
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_signal(pthread_cond_t *cond)
{
  if(checkAdress((void*) cond) != 0)
    return -1;
  if(cond->initialized_ != 1)
  {
    printf("tried to signal on uninit cond!\n");
    return -1;
  }

  pthread_spin_lock(&cond->CV_sleeperslist_lock_);
  if(cond->firstsleeper_ != 0){
  
    size_t setwake = findStackStackStart((size_t) cond->firstsleeper_);
    assert(atomic_exchange_0((size_t*) setwake) == 1 && "CV: tried to wake but was alreay woke?\n");
    // mutex->firstsleeper_ = (size_t*) *mutex->firstsleeper_;
  }
  //printf("flag is now again unlocked: %ld\n", mutex->lock_);
  cond->threads_waiting_--;
  cond->firstsleeper_ = cond->firstsleeper_ == 0 ? (size_t*) 0 : (size_t*) *cond->firstsleeper_;
  pthread_spin_unlock(&cond->CV_sleeperslist_lock_);
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_broadcast(pthread_cond_t *cond)
{
  if(checkAdress((void*) cond) != 0)
    return -1;
  if(cond->initialized_ != 1)
  {
    printf("tried to broadcast on uninit cond!\n");
    return -1;
  }
  pthread_spin_lock(&cond->CV_sleeperslist_lock_);
  if(cond->firstsleeper_ != 0){
    size_t* iter = cond->firstsleeper_;
    for (size_t i = 0; i < cond->threads_waiting_; i++)
    {
      size_t setwake = findStackStackStart((size_t) iter);
      assert(atomic_exchange_0((size_t*) setwake) == 1 && "CV: tried to wake but was alreay woke?\n");  
      iter = (size_t*)*iter;
    }
  }
  cond->firstsleeper_ = 0;
  cond->threads_waiting_ = 0;
  pthread_spin_unlock(&cond->CV_sleeperslist_lock_);
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  if(checkAdress((void*) cond) != 0 || checkAdress((void*) mutex) != 0)
    return -1;
  if(cond->initialized_ != 1 || mutex->initialized_ != 1)
  {
    printf("tried to wait on uninit cond or mutex!\n");
    return -1;
  }

  size_t findstart;
  size_t my_identifier = findStackStackStart((size_t)&findstart);
  pthread_spin_lock(&mutex->held_by_lock_);
  if (mutex->held_by_ != my_identifier)
  {
    pthread_spin_unlock(&mutex->held_by_lock_);
    printf("You do not even have the mutex! Aborting...\n");
    return -1;
  }
  pthread_spin_unlock(&mutex->held_by_lock_);

  pthread_spin_lock(&cond->CV_sleeperslist_lock_);
 
  size_t* next = 0;
  assert(addToCVWaitersList(cond, &next) == 0 && "how? you managed to wait twice?\n");
  cond->threads_waiting_++;
  pthread_spin_unlock(&cond->CV_sleeperslist_lock_);
  //sleep well little thread...
  pthread_mutex_unlock(mutex);
  printf("now going to sleep on condition\n");
  size_t setsleep = findStackStackStart((size_t) &next);
  assert(atomic_exchange_1((size_t*) setsleep) == 0 && "tried to sleep but was alreafy?\n");
  sched_yield();

  pthread_mutex_lock(mutex);
  
  return 0;
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
  if(checkAdress((void*) lock) != 0)
    return -1;

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
  if(checkAdress((void*) lock) != 0)
    return -1;
  
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
  if(checkAdress((void*) lock) != 0)
    return -1;
  
  if(atomic_exchange_1(&lock->mylock_) != 0)
    return -1;
  return 0;
}

