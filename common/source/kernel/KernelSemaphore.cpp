#include "KernelSemaphore.h"
#include "kprintf.h"
#include "ArchThreads.h"
#include "ArchInterrupts.h"
#include "Scheduler.h"
#include "Thread.h"
#include "panic.h"
#include "backtrace.h"
#include "assert.h"
#include "Stabs2DebugInfo.h"

extern Stabs2DebugInfo const* kernel_debug_info;

class Scheduler;

KernelSemaphore::KernelSemaphore(const char* name) :
  Lock::Lock(name)
{
}


int KernelSemaphore::wait()
{
  if(unlikely(system_state != RUNNING))
    return -1;
  if (!initialized_)
    return -1;
  
  
//  debug(LOCK, "Mutex::acquire:  Mutex: %s (%p), currentThread: %s (%p).\n",
//           getName(), this, currentThread->getName(), currentThread);
//  if(kernel_debug_info)
//  {
//    debug(LOCK, "The acquire is called by: ");
//    kernel_debug_info->printCallInformation(called_by);
//  }
  // check for deadlocks, interrupts...
  //doChecksBeforeWaiting();

  while((max_threads_ - 1) < 0)
  {
    checkCurrentThreadStillWaitingOnAnotherLock();
    lockWaitersList();
    // Here we have to check for the lock again, in case some one released it in between, we might sleep forever.
    if(max_threads_ - 1 >= 0)
    {
      unlockWaitersList();
      break;
    }
    // check for deadlocks, interrupts...
    //doChecksBeforeWaiting();
    sleepAndRelease();
    // We have been waken up again.
    currentThread->lock_waiting_on_ = 0;
  }
  assert(max_threads_-- >= 0 && "whaat didnt get the sem!\n");
 // pushFrontToCurrentThreadHoldingList();
 // last_accessed_at_ = called_by;
  //held_by_ = currentThread;
  return 0;
}

int KernelSemaphore::post()
{
  if(unlikely(system_state != RUNNING))
    return -1;
  if (!initialized_)
    return -1;
  
//  debug(LOCK, "Mutex::release:  Mutex: %s (%p), currentThread: %s (%p).\n",
//           getName(), this, currentThread->getName(), currentThread);
//  if(kernel_debug_info)
//  {
//    debug(LOCK, "The release is called by: ");
//    kernel_debug_info->printCallInformation(called_by);
//  }
 // checkInvalidRelease("Mutex::release");
  //removeFromCurrentThreadHoldingList();
 // last_accessed_at_ = called_by;
//   held_by_ = 0;
//   mutex_ = 0;
  // Wake up a sleeping thread. It is okay that the mutex is not held by the current thread any longer.
  // In worst case a new thread is woken up. Otherwise (first wake up, then release),
  // it could happen that a thread is going to sleep after the this one is trying to wake up one.
  // Then we are dead... (the thread may sleep forever, in case no other thread is going to acquire this mutex again).
  lockWaitersList();
  max_threads_++;
  Thread* thread_to_be_woken_up = popBackThreadFromWaitersList();
  unlockWaitersList();
  if(thread_to_be_woken_up)
  {
    Scheduler::instance()->wake(thread_to_be_woken_up);
  }
  return 0;
}

bool KernelSemaphore::isFree()
{
  if(unlikely(ArchInterrupts::testIFSet() && Scheduler::instance()->isSchedulingEnabled()))
  {
    debug(LOCK, "Mutex::isFree: ERROR: Should not be used with IF=1 AND enabled Scheduler, use acquire instead\n");
    assert(false);
  }
  return (max_threads_ > 0);
}

// int KernelSemaphore::wait(){
//     if (!initialized_)
//     {
//         return -1;
//     }
    
//     counter_lock_.acquire();
//     if (max_threads_ > 0)
//     {
//         max_threads_--;
//         counter_lock_.release();
//     }
//     else 
//     {
//         while (max_threads_ == 0)
//         {
//             counter_lock_.release();
//             condition_lock_.acquire();
//             threads_cond_.wait();
//             condition_lock_.release();
//             counter_lock_.acquire();
//         }
//         max_threads_--;
//         counter_lock_.release();
//     }
//     return 0;
// }

// int KernelSemaphore::post(){
//     if (!initialized_)
//     {
//         return -1;
//     }
//     counter_lock_.acquire();
//     ++max_threads_;
//     counter_lock_.release();
//     condition_lock_.acquire();
//     threads_cond_.signal();
//     condition_lock_.release();
//     return 0;
// }

