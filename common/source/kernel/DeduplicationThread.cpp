#include "DeduplicationThread.h"
#include "Scheduler.h"

DeduplicationThread::DeduplicationThread() : Thread(0, "DeduplicaitonThread", Thread::KERNEL_THREAD)
{
    scheduling_frequency_ = 3;
    last_scheduled_ = 0;
}

DeduplicationThread::~DeduplicationThread()
{
    assert(false && "we probably dont wanna kill the deduplicaion thread\n");
}
void DeduplicationThread::kill()
{
    assert(false && "we probably dont wanna kill the deduplicaion thread\n");
}

void DeduplicationThread::Run()
{

}

bool DeduplicationThread::schedulable()
{
    if (last_scheduled_ + 1 == scheduling_frequency_)
    {
        last_scheduled_ = 0;
        return true;
    }
    else
    {
        last_scheduled_++;
        return false;
    }
    
}