#include "KernelSemaphore.h"

int KernelSemaphore::wait(){
    if (!initialized_)
    {
        return -1;
    }
    
    counter_lock_.acquire();
    if (max_threads_ > 0)
    {
        max_threads_--;
        counter_lock_.release();
    }
    else 
    {
        while (max_threads_ == 0)
        {
            counter_lock_.release();
            condition_lock_.acquire();
            threads_cond_.wait();
            condition_lock_.release();
            counter_lock_.acquire();
        }
        max_threads_--;
        counter_lock_.release();
    }
    return 0;
}

int KernelSemaphore::post(){
    if (!initialized_)
    {
        return -1;
    }
    counter_lock_.acquire();
    ++max_threads_;
    counter_lock_.release();
    condition_lock_.acquire();
    threads_cond_.signal();
    condition_lock_.release();
    return 0;
}

