#include "pthread.h"
#include "syscall.h"
#include "sched.h"
#include "../../../common/include/kernel/syscall-definitions.h"


//not rly that threadsafe i think...
//pthread_mutex_t* firstmutex = (pthread_mutex_t*) 0;
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

/**
 * @brief the userspace way to get to the sleepflag... used by mutex and CV to set thread sleeping AND as a unique identifier
 * for a thread to detect deadlocks
 * 
 * @param inputadress the adress of some stackvariable of a thread which is used to find its sleepflag
 * @return the flag location of the thread
 */
size_t findStackStackStart(size_t inputadress){
  size_t outputadress = inputadress >> 12;
  outputadress = outputadress << 12;
  outputadress += PAGE_SIZE_US - sizeof(size_t);
  if (*((size_t*)outputadress) != SLEEPING_US && *((size_t*)outputadress) != AWAKE_US)
  {
    return findStackStackStart(outputadress + 2* sizeof(size_t));
  }
  
  return outputadress;
}

void kernelsem_wait(){
  __syscall(sc_ks_wait, 0x0, 0x0, 0x0, 0x0, 0x0);
}
void kernelsem_post(){
  __syscall(sc_ks_post, 0x0, 0x0, 0x0, 0x0, 0x0);

}


/**
 * @brief also used for deadlock detection: checks if a thread is waiting on a lock
 * 
 * @param identifier the threads unique identifier as computed by the function above 
 * @param mutex the Mutex for which it should be checked wether the thread waits on it
 * @return 0 if the thread isnt waiting on the lock and 1 if it is
 */
int isWaitingHere(size_t identifier, pthread_mutex_t* mutex){
  pthread_spin_lock(&mutex->sleeperslist_lock_);
  if (mutex->firstsleeper_ == 0)
  {
    pthread_spin_unlock(&mutex->sleeperslist_lock_);
    return 0;
  }
  size_t* iter = mutex->firstsleeper_;
  //printf("lock: %p: ", mutex);
  while (iter != 0)
  {
    if (findStackStackStart((size_t)iter) == identifier)
    {
      pthread_spin_unlock(&mutex->sleeperslist_lock_);
      return 1;
    }
    //printf(" %p ", (size_t*)findStackStackStart((size_t) iter));
    iter = (size_t*) *iter;
  }
  //printf("\n");
  pthread_spin_unlock(&mutex->sleeperslist_lock_);
  return 0;
}

/**
 * @brief detects if a thread which is waiting on  a lock waits on one of the other locks. This should be VERY unlikely, as the 
 * thread must have been woken up by a different source than us...
 * 
 * @param waiter_identifier unique identifier as computed above
 * @param lock_wanted the lock which the thread wants to wait on
 * @return 0 if the thread is not waitin, otherwise it asserts anyways :D
 */
// int detectThreadWaiting(size_t waiter_identifier, pthread_mutex_t* lock_wanted){
//   pthread_spin_lock(&mutexlist_lock);
//   for (pthread_mutex_t* iter = firstmutex; iter->next_mutex_ != 0; iter = iter->next_mutex_)
//   {
//     if (iter == lock_wanted)
//       continue;
    
//     if (isWaitingHere(waiter_identifier, iter))
//     {
//       assert(0 && "how?? thread is waiting on another lock already??\n");
//     }
//   }
//   pthread_spin_unlock(&mutexlist_lock);
//   return 0;
// }
  

/**
 * @brief Circular deadlock detection: checks for a circular dependency: If the thread waiting on this lock also has a lock on which the thread
 * currently holding the lock wants to wait
 * 
 * @param waiter_identifier unique identifier as computed above
 * @param lock_wanted the lock which the thread wants to wait on
 * @return 0 if there was no circular deadlock and -1 if there was one!
 */
int detectCircularDeadlock(size_t waiter_identifier, pthread_mutex_t* lock_wanted){
  
  pthread_spin_lock(&mutexlist_lock);
  size_t held_by_cur = lock_wanted->held_by_;
  if (held_by_cur == waiter_identifier)
  {
    pthread_spin_unlock(&mutexlist_lock);
    printf("You already have this lock!\n");
    return -1;
  }
  
  size_t* firstcheck = (size_t*)(held_by_cur - sizeof(size_t));
  if (*firstcheck == (size_t)NO_LOCK_US)
  {
    pthread_spin_unlock(&mutexlist_lock);
    return 0;
  }
  else
  {
    pthread_mutex_t* current_mutex = (pthread_mutex_t*) *firstcheck;
    while ((size_t)current_mutex != (size_t) NO_LOCK_US)
    {
      if (current_mutex->held_by_ == waiter_identifier)
      {
        pthread_spin_unlock(&mutexlist_lock);
        printf("Circular deadlock found! Returning only -1 for now...\n");
        return -1;
      }
      else
      {
        current_mutex = *((pthread_mutex_t**)(current_mutex->held_by_ - sizeof(size_t)));
      }
    }
    pthread_spin_unlock(&mutexlist_lock);
    return 0;
  }
  
  //pthread_spin_lock(&mutexlist_lock);
  
  // for (pthread_mutex_t* iter = firstmutex; iter != 0; iter = iter->next_mutex_)
  // {
  //   //printf("checking dl for mutex: %p held by %p\n", iter, (size_t*) iter->held_by_);
  //   if (iter == lock_wanted)
  //     continue;
    
  //   pthread_spin_lock(&iter->held_by_lock_);
  //   size_t held_by = iter->held_by_;
  //   if (held_by == waiter_identifier && isWaitingHere(held_by_cur, iter))
  //   {
  //     assert(0 && "CIRCULAR deadlock found!\n");
  //   }
  //   pthread_spin_unlock(&iter->held_by_lock_);           

  // }
  //   pthread_spin_unlock(&mutexlist_lock);
  //   pthread_spin_unlock(&lock_wanted->held_by_lock_);
  //   return 0;
}
  
size_t atomic_exchange_sleep(size_t *flag){
  size_t myval = SLEEPING_US;
  asm("xchg %0, %1\n\t" : "=r" (myval) : "m" (*flag), "0" (myval) : "memory" );
  return myval;
};

size_t atomic_exchange_wake(size_t* flag){
  size_t myval = AWAKE_US;
  asm("xchg %0, %1\n\t" : "=r" (myval) : "m" (*flag), "0" (myval) : "memory" );
  return myval;
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
 * @brief checks if an adress is valid or not, Caution: this one also detects NULL pointers and marks them as invalid... sometimes they are okay 
 * though... furthermore detects kernel adresses
 * 
 * @param adress the adress to be checkede
 * @return -1 if adress is invalid 0 if its okay
 */
int checkAdress(void* adress, int null_ok){
  if ((unsigned long long) adress > 0x0000800000000000ULL)
    goto invalid;
  else if (!null_ok && adress < (void*) 0x1000) 
    goto invalid;
  return 0;

invalid:
  printf("invalid address!\n");
  return -1;
}


/**
 * @brief Adds a thread to the waiterslist of a mutex... lock the List before using this (sleeperslist_)
 * 
 * @param mutex the mutex tho which's list the thread shall be added
 * @param localvaradress adress of the size_t* next varibale pointing to the next thread on the waiters list. NOT the unique identifier!!
 * @return 0 if the thread could be added sucessfully -1 if it already was on the list
 */
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


/**
 * @brief literally the same function as above for condition variables... Unfortunately C has no inheritance!
 * 
 * @param cond the condition var
 * @param lock_wanted the next var
 * @return 0 if sucess -1 if alreafy on list
 */
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
  if(checkAdress((void*) thread, 0) != 0 || checkAdress((void*)start_routine, 0) != 0)
    return -1;
  if (attr != 0x0)
  {
    if (checkAdress((void*) attr, 0) != 0 || attr->initialized_ != 1)
      return -1;
  }
  
  return __syscall(sc_pthread_create, (size_t)thread, (size_t)attr, (size_t)start_routine, (size_t)arg, (size_t)wrapper);
  // wrapper defined above
}


/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_attr_init(pthread_attr_t *attr){
  if(checkAdress((void*) attr, 0) != 0)
    return -1;
  if (attr->initialized_ == 1)
    return -1;
  
  attr->detach_state_ = PTHREAD_CREATE_JOINABLE;
  attr->guard_size_ = PAGE_SIZE_US;
  __syscall(sc_pthread_attr_init, (size_t)&attr->stackaddress_, (size_t)&attr->stacksize_, 0x0, 0x0, 0x0);
  attr->initialized_ = 1;
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_attr_destroy(pthread_attr_t *attr){
  if(checkAdress((void*) attr, 0) != 0)
    return -1;
  if (attr->initialized_ != 1)
    return -1;
  attr->initialized_ = 0;
  return 0;
  
};


/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
pthread_t pthread_self(void){
  return (pthread_t) __syscall(sc_pthread_self, 0x0, 0x0, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate){
  
  if(checkAdress((void*) attr, 0) != 0)
    return -1;
  if (detachstate != PTHREAD_CREATE_JOINABLE && detachstate != PTHREAD_CREATE_DETACHED)
    return -1;
  
  attr->detach_state_ = detachstate;
  return 0;
}


/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate){
  
  if(checkAdress((void*) attr, 0) != 0 || checkAdress((void*) detachstate, 0)!= 0)
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
  if(checkAdress((void*) mutex, 0) != 0 || checkAdress((void*) attr, 1) != 0)
    return -1;
  if (mutex->initialized_ == 1)
    return -1;
  
  pthread_spin_init(&mutex->sleeperslist_lock_, 0);
  mutex->lock_ = 1;
  mutex->my_attr_ = ((size_t)attr <= 0x1000) ? (pthread_mutexattr_t) 0 :  *attr;
  
  mutex->firstsleeper_ = 0;
  mutex->initialized_ = 1;
  mutex->held_by_ = 0;

  pthread_spin_init(&mutexlist_lock, 0);
    
  return 0;
}



/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_setcancelstate(int state, int *oldstate){
  return __syscall(sc_pthread_setcancelstate, (size_t) state, (size_t) oldstate, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
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
  if(checkAdress((void*) value_ptr, 1) != 0)
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
  if(checkAdress((void*) mutex, 0) != 0)
    return -1;
  if (mutex->initialized_ != 1)
    return -1;
  
  mutex->initialized_ = 0;
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
  if(checkAdress((void*) mutex, 0) != 0)
    return -1;
  if(mutex->initialized_ != 1)
  {
    printf("tried to lock uninitialized mutex!\n");
    return -1;
  } 

  size_t findstart;
  size_t my_identifier = findStackStackStart((size_t)&findstart);

  
  if (!atomic_exchange_0(&mutex->lock_))
  {
    
    printf("circdeadlock detection running now\n");
    //detectThreadWaiting(my_identifier, mutex);
    if(detectCircularDeadlock(my_identifier, mutex) != 0)
      return -1;
    pthread_spin_lock(&mutex->sleeperslist_lock_);
    size_t* next = 0;
    if(addToWaitersList(mutex, &next) != 0)
    {
      pthread_spin_unlock(&mutex->sleeperslist_lock_);
      printf("DEADLOCK! you already wait on this lock!\n");
      return -1;
    }
    size_t setsleep = findStackStackStart((size_t) &next);
    size_t* waiting_on = (size_t*)(setsleep - sizeof(size_t));
    *waiting_on = (size_t) mutex;
    pthread_spin_unlock(&mutex->sleeperslist_lock_);
    assert(atomic_exchange_sleep((size_t*) setsleep) == AWAKE_US && "tried to sleep but was alreafy?\n");
    printf("sleeping now\n");
    //printf("going to sleep now...\n");
    sched_yield();
    *waiting_on = NO_LOCK_US;
  }

  pthread_spin_lock(&mutexlist_lock);
  mutex->held_by_ = my_identifier;
  pthread_spin_unlock(&mutexlist_lock);
  
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
  if(checkAdress((void*) mutex, 0) != 0)
    return -1;
  if(mutex->initialized_ != 1)
  {
    printf("tried to unlock uninitialized mutex!\n");
    return -1;
  } 

  size_t findstart;
  size_t my_identifier = findStackStackStart((size_t)&findstart);
  if (mutex->held_by_ != my_identifier)
  {
    printf("What are you trying to unlock here? you dont have this lock\n");
    return -1;
  }


  pthread_spin_lock(&mutex->sleeperslist_lock_);
  if(mutex->firstsleeper_ != 0)
  {
    printf("first sleeper ist %p lets wake him\n", mutex->firstsleeper_);
    size_t setwake = findStackStackStart((size_t) mutex->firstsleeper_);
    assert(atomic_exchange_wake((size_t*) setwake) == SLEEPING_US && "tried to wake but was alreay woke?\n");
    mutex->firstsleeper_ = (size_t*) *mutex->firstsleeper_;
    // mutex->firstsleeper_ = (size_t*) *mutex->firstsleeper_;
  }
  else
  {
    assert(atomic_exchange_1(&mutex->lock_) == 0 && "whaat? lock was free when unlocking\n");
  }
  pthread_spin_unlock(&mutex->sleeperslist_lock_);
  //printf("flag is now again unlocked: %ld\n", mutex->lock_);
  pthread_spin_lock(&mutexlist_lock);
  mutex->held_by_ = 0;
  pthread_spin_unlock(&mutexlist_lock);

  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
  if(checkAdress((void*) cond, 0) != 0 || checkAdress((void*) attr, 1) != 0)
    return -1;
  if(cond->initialized_ == 1)
    return -1;
  
  pthread_spin_init(&cond->CV_sleeperslist_lock_, 0);
  cond->firstsleeper_ = 0;
  cond->my_attr_ = ((size_t)attr <= 0x1000) ? (pthread_condattr_t) 0 :  *attr;
  
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
  if(checkAdress((void*) cond, 0) != 0)
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
  if(checkAdress((void*) cond, 0) != 0)
    return -1;
  if(cond->initialized_ != 1)
  {
    printf("tried to signal on uninit cond!\n");
    return -1;
  }

  pthread_spin_lock(&cond->CV_sleeperslist_lock_);
  if(cond->firstsleeper_ != 0)
  {
    size_t setwake = findStackStackStart((size_t) cond->firstsleeper_);
    assert(atomic_exchange_wake((size_t*) setwake) == SLEEPING_US && "CV: tried to wake but was alreay woke?\n");
    // mutex->firstsleeper_ = (size_t*) *mutex->firstsleeper_;
  }
  //printf("flag is now again unlocked: %ld\n", mutex->lock_);
  cond->threads_waiting_--;
  cond->firstsleeper_ = cond->firstsleeper_ == 0 ? (size_t*) 0 : (size_t*) *cond->firstsleeper_;
  pthread_spin_unlock(&cond->CV_sleeperslist_lock_);
  printf("signaled cond!\n");
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_broadcast(pthread_cond_t *cond)
{
  if(checkAdress((void*) cond, 0) != 0)
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
      assert(atomic_exchange_wake((size_t*) setwake) == SLEEPING_US && "CV: tried to wake but was alreay woke?\n");  
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
  if(checkAdress((void*) cond, 0) != 0 || checkAdress((void*) mutex, 0) != 0)
    return -1;
  if(cond->initialized_ != 1 || mutex->initialized_ != 1)
  {
    printf("tried to wait on uninit cond or mutex!\n");
    return -1;
  }

  size_t findstart;
  size_t my_identifier = findStackStackStart((size_t)&findstart);
  if (mutex->held_by_ != my_identifier)
  {
    printf("You do not even have the mutex! Aborting...\n");
    return -1;
  }

  pthread_spin_lock(&cond->CV_sleeperslist_lock_);
 
  size_t* next = 0;
  assert(addToCVWaitersList(cond, &next) == 0 && "how? you managed to wait twice?\n");
  cond->threads_waiting_++;
  pthread_spin_unlock(&cond->CV_sleeperslist_lock_);
  //sleep well little thread...
  pthread_mutex_unlock(mutex);
  printf("now going to sleep on condition\n");
  size_t setsleep = findStackStackStart((size_t) &next);
  assert(atomic_exchange_sleep((size_t*) setsleep) == AWAKE_US && "tried to sleep but was alreafy?\n");
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
  if (checkAdress((void*)lock, 0) != 0)
    return -1;
  if (lock->initialized_ != 1)
    return -1;

  lock->initialized_ = 0;
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_init(pthread_spinlock_t *lock, int pshared)
{
  if(checkAdress((void*) lock, 0) != 0)
    return -1;

  if (lock->initialized_ == 1)
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
  if(checkAdress((void*) lock, 0) != 0)
    return -1;
  
  if (lock->initialized_ != 1)
    return -1;
  
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
  if(checkAdress((void*) lock, 0) != 0)
    return -1;
  
  if (lock->initialized_ != 1)
  return -1;
  
  if (!atomic_exchange_0(&lock->mylock_))
  {
    //not suuper safe but okay for now
    return -1;
  }
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_unlock(pthread_spinlock_t *lock)
{
  if(checkAdress((void*) lock, 0) != 0)
    return -1;
  
  if(atomic_exchange_1(&lock->mylock_) != 0)
    return -1;
  return 0;
}