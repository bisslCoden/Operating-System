#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

//pthread typedefs
typedef size_t pthread_t;

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
#define PTHREAD_

//pthread spinlock typedefs

//pthread cond typedefs
typedef unsigned int pthread_cond_t;
typedef unsigned int pthread_condattr_t;

typedef struct spinlock{
    size_t mylock_;
    size_t initialized_;
    int pshared_;
}pthread_spinlock_t;

typedef struct threadattribute
{
    int initialized_;
    int detach_state_;
    size_t guard_size_; 
    size_t* stackaddress_;
    size_t  stacksize_;
} pthread_attr_t;


#define PAGE_SIZE_US 4096

typedef struct UserMutex{
    size_t initialized_;
    pthread_spinlock_t sleeperslist_lock_;
    size_t* firstsleeper_;
    pthread_mutexattr_t my_attr_;
    size_t lock_;
}pthread_mutex_t;


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



extern int pthread_cancel(pthread_t thread);

extern int pthread_join(pthread_t thread, void **value_ptr);

extern int pthread_detach(pthread_t thread);

extern int pthread_mutex_init(pthread_mutex_t *mutex,
                              const pthread_mutexattr_t *attr);

extern int pthread_mutex_destroy(pthread_mutex_t *mutex);

extern int pthread_mutex_lock(pthread_mutex_t *mutex);

extern int pthread_mutex_unlock(pthread_mutex_t *mutex);

extern int pthread_attr_init(pthread_attr_t *attr);

extern int pthread_attr_destroy(pthread_attr_t *attr);



extern int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);

extern int pthread_cond_destroy(pthread_cond_t *cond);

extern int pthread_cond_signal(pthread_cond_t *cond);

extern int pthread_cond_broadcast(pthread_cond_t *cond);

extern int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

#ifdef __cplusplus
}
#endif


