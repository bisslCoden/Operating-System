#include "types.h"
#include "UserThread.h"
#include "Mutex.h"
#include "Condition.h"
#include "uvector.h"

class Semaphore
{
private:
    Mutex counter_lock_;
    Mutex threads_lock_;
    Mutex condition_lock_;
    Condition threads_cond_;
    size_t max_threads_;
    ustl::vector<UserThread*> waiters_list_;
    
public:
    Semaphore(size_t count) : counter_lock_{"sem::counter_lock_"}, threads_lock_{"sem::threads_lock_"}, condition_lock_{"sem::condition_lock_"},
     max_threads_{count}, threads_cond_{&condition_lock_, "sem::condition"}{}

    ~Semaphore();

    void wait();
    void post();
};



