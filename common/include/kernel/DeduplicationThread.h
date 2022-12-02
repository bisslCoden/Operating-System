#pragma once

#include "Thread.h"
#include "types.h"

class DeduplicationThread : public Thread
{
private:
    size_t scheduling_frequency_;
    size_t last_scheduled_;
public:
    DeduplicationThread();
    bool schedulable() override;
    virtual void kill();
    virtual void Run();
    virtual ~DeduplicationThread();
};

