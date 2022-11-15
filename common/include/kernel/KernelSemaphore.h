#include "types.h"
//#include "UserThread.h"
#include "Mutex.h"
#include "Condition.h"

class KernelSemaphore
{
private:
    Mutex counter_lock_;
    Mutex threads_lock_;
    Mutex condition_lock_;
    Condition threads_cond_;
    size_t max_threads_;
    
public:
    KernelSemaphore(size_t count) : counter_lock_{"sem::counter_lock_"}, threads_lock_{"sem::threads_lock_"}, condition_lock_{"sem::condition_lock_"},
    threads_cond_{&condition_lock_, "sem::condition"}, max_threads_{count}{}


    void wait();
    void post();
};



