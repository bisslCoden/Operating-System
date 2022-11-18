#pragma once 

#include "types.h"
//#include "UserThread.h"
// #include "Mutex.h"
// #include "Condition.h"
#include "Lock.h"
#include "uatomic.h"

class KernelSemaphore: public Lock
{
private:
    const char* name_;
    ustl::atomic<int> max_threads_;
    bool initialized_ = false;
    
public:
    KernelSemaphore(const char* name);

    void init(size_t max_threads) { max_threads_ = max_threads; initialized_ = true;}
    int wait();
    int post();
    bool isFree();
};



