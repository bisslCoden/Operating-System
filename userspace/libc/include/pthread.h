#pragma once

#include "types.h"
#include "assert.h"
#include "stdio.h"
#include "sys/syscall.h"
#include "../../../common/include/kernel/syscall-definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

//pthread typedefs
typedef size_t pthread_t;

#define SLEEPING_US 0x46334234
#define AWAKE_US 0x54321432
#define NO_LOCK_US 0x12246079
//#define ONE_MORE_TIME_US 0x87345123


enum cancelstate {
    PTHREAD_CANCEL_ENABLE,
    PTHREAD_CANCEL_DISABLE
};

enum canceltype {
    PTHREAD_CANCEL_DEFERRED, 
    PTHREAD_CANCEL_ASYNCHRONOUS
};

enum joinbale {
    PTHREAD_CREATE_JOINABLE, 
    PTHREAD_CREATE_DETACHED
};

//not yet implemented

//pthread mutex typedefs
typedef unsigned int pthread_mutexattr_t;


#define PTHREAD_CANCELED ((void *) -1)

//pthread spinlock typedefs

//pthread cond typedefs
typedef unsigned int pthread_condattr_t;


typedef struct spinlock{
    size_t mylock_;
    size_t initialized_;
    int pshared_;
}pthread_spinlock_t;

typedef struct UserMutex{
    size_t initialized_;
    pthread_spinlock_t sleeperslist_lock_;
    size_t* firstsleeper_;
    pthread_mutexattr_t my_attr_;
    size_t lock_;
    size_t held_by_;
    struct UserMutex* next_mutex_;
}pthread_mutex_t;


typedef struct UserCV
{
    size_t initialized_;
    pthread_spinlock_t CV_sleeperslist_lock_;
    size_t* firstsleeper_;
    pthread_condattr_t my_attr_;
    size_t lock_;
    size_t threads_waiting_;
} pthread_cond_t;


typedef struct threadattribute
{
    int initialized_;
    int detach_state_;
    size_t guard_size_; 
    size_t* stackaddress_;
    size_t  stacksize_;
} pthread_attr_t;


#define PAGE_SIZE_US 4096

extern int pthread_create(pthread_t *thread,
         const pthread_attr_t *attr, void *(*start_routine)(void *),
         void *arg);
         

//setters for cancelflags
extern int pthread_setcancelstate(int state, int *oldstate);
extern int pthread_setcanceltype(int type, int *oldtype);

extern void pthread_exit(void *value_ptr);

//spinlocks
extern int pthread_spin_init(pthread_spinlock_t *lock, int pshared);
extern int pthread_spin_lock(pthread_spinlock_t *lock);
extern int pthread_spin_unlock(pthread_spinlock_t *lock);

extern int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
extern int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate);

extern int checkAdress(void* adress, int null_ok);

extern pthread_t pthread_self(void);

// extern void kernelsem_wait();
// extern void kernelsem_post();


extern int pthread_cancel(pthread_t thread);

extern int pthread_join(pthread_t thread, void **value_ptr);

extern int pthread_detach(pthread_t thread);

/**
 * @brief initializes the mutex and all the locks etc. it needs 
 * 
 * @param mutex pointer to mutex in the program
 * @param attr mutexattribute
 * 
 * @return 0 if mutex wasnt init -1 if something went wrong (already init or params trash)
 */
extern int pthread_mutex_init(pthread_mutex_t *mutex,
                              const pthread_mutexattr_t *attr);


/**
 * @brief marks mutex as uninitialized
 * 
 * @param mutex pointer to mutex in the program
 * 
 * @return 0 if mutex was init -1 if something went wrong (not init or params trash)
 */
extern int pthread_mutex_destroy(pthread_mutex_t *mutex);


/**
 * @brief locks the mutex and sets calling thread to sleep if mutex is not available. Furthermore provides
 * a (fairly) advanced deadlock detection
 * 
 * @param mutex pointer to mutex in the program
 * 
 * @return 0 if mutex could be locked and thread got woken up again -1 in every other case
 */
extern int pthread_mutex_lock(pthread_mutex_t *mutex);

/**
 * @brief unlocks the mutex and waked up next thread from the sleeperslist
 * 
 * @param mutex pointer to mutex in the program
 * 
 * @return 0 if mutex could be unlocked and thread got woken up again -1 in every other case
 */
extern int pthread_mutex_unlock(pthread_mutex_t *mutex);

extern int pthread_attr_init(pthread_attr_t *attr);

extern int pthread_attr_destroy(pthread_attr_t *attr);

/**
 * @brief initializes the condition and all the locks etc. it needs 
 * 
 * @param cond pointer to condition in the program
 * @param attr condattribute
 * 
 * @return 0 if cond wasnt init -1 if something went wrong (already init or params trash)
 */
extern int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);

/**
 * @brief marks the condition as uninit
 * 
 * @param cond pointer to condition in the program
 * 
 * @return 0 if cond was init -1 if something went wrong (already uninit or params trash)
 */
extern int pthread_cond_destroy(pthread_cond_t *cond);

extern int pthread_cond_signal(pthread_cond_t *cond);

extern int pthread_cond_broadcast(pthread_cond_t *cond);

/**
 * @brief basically the same function like a mutex except for that we instantly go to sleep without checking anything first and
 * we release the mutex we currently have
 * 
 * @param cond pointer to cond in the program
 * @param mutex pointer to mutex in the program which shall be released
 * 
 * @return 0 if sucess -1 else
 */
extern int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

#ifdef __cplusplus
}
#endif
