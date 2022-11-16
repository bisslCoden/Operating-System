#include "types.h"
//#include "UserThread.h"
#include "Mutex.h"
#include "Condition.h"

class KernelSemaphore
{
private:
    const char* name_;
    Mutex counter_lock_;
    Mutex threads_lock_;
    Mutex condition_lock_;
    Condition threads_cond_;
    size_t max_threads_;
    bool initialized_ = false;
    
public:
    KernelSemaphore(const char* name) : name_{name}, counter_lock_{"sem::counter_lock_"}, threads_lock_{"sem::threads_lock_"}, condition_lock_{"sem::condition_lock_"},
    threads_cond_{&condition_lock_, "sem::condition"}{}

    void init(size_t max_threads) { max_threads_ = max_threads;
    initialized_ = true; };
    int wait();
    int post();
};



