#include "Semaphore.h"

void Semaphore::wait(){
    counter_lock_.acquire();
    if (max_threads_ > 0)
    {
        max_threads_--;
        counter_lock_.release();
    }
    else 
    {
        counter_lock_.release();
        condition_lock_.acquire();
        threads_cond_.wait();
        condition_lock_.release();
        counter_lock_.acquire();
        max_threads_--;
        counter_lock_.release();
    }
    return;
}

void Semaphore::post(){
    counter_lock_.acquire();
    ++max_threads_;
    counter_lock_.release();
    condition_lock_.acquire();
    threads_cond_.signal();
    condition_lock_.release();
    return;
}

