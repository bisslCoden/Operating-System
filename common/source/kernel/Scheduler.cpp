#include "Scheduler.h"
#include "Thread.h"
#include "panic.h"
#include "ArchThreads.h"
#include "ArchCommon.h"
#include "kprintf.h"
#include "ArchInterrupts.h"
#include "KernelMemoryManager.h"
#include <ulist.h>
#include "backtrace.h"
#include "ArchThreads.h"
#include "Mutex.h"
#include "umap.h"
#include "ustring.h"
#include "Lock.h"

ArchThreadRegisters *currentThreadRegisters;
Thread *currentThread;

Scheduler *Scheduler::instance_ = 0;

Scheduler *Scheduler::instance()
{
  if (unlikely(!instance_))
    instance_ = new Scheduler();
  return instance_;
}

Scheduler::Scheduler()
{
  block_scheduling_ = 0;
  ticks_ = 0;
  addNewThread(&cleanup_thread_);
  addNewThread(&idle_thread_);
}

size_t* getToFlag(Thread* it)
{
  debug(X_THREADSTACK, "getting to flag from %ld\n", it->getTID());
  UserThread* user = (UserThread*) it;
  return (size_t*) ((size_t)user->getUserstackStart() + sizeof(size_t));
}

uint32 Scheduler::schedule()
{
  assert(!ArchInterrupts::testIFSet() && "Tried to schedule with Interrupts enabled");
  if (block_scheduling_ != 0)
  {
    debug(SCHEDULER, "schedule: currently blocked\n");
    return 0;
  }
  auto it = threads_.begin();
  for(; it != threads_.end(); ++it)
  {
    if ((*it)->Userthread && (*it)->getState() == Sleeping)
    {
      currentThread = *it;
      size_t* adressflag = getToFlag((*it));
      debug(X_THREADSTACK, "now trying to deref flag for thread [%ld]\n", (*it)->getTID());
      size_t sleepy = __atomic_exchange_n(adressflag, 1, ustl::memory_order_seq_cst);
      debug(X_THREADSTACK, "got %ld for sleepy for thread [%ld]\n", sleepy, (*it)->getTID());
      if (sleepy == 0)
      {
        wake((*it));
      }
    }
    
    if((*it)->schedulable())
    {
      currentThread = *it;
      break;
    }
  }

  assert(it != threads_.end() && "No schedulable thread found");

  ustl::rotate(threads_.begin(), it + 1, threads_.end()); // no new/delete here - important because interrupts are disabled

  //debug(SCHEDULER, "Scheduler::schedule: new currentThread is %p %s, switch_to_userspace: %d\n", currentThread, currentThread->getName(), currentThread->switch_to_userspace_);

  uint32 ret = 1;

  if (currentThread->switch_to_userspace_)
  {
    currentThreadRegisters = currentThread->user_registers_;
  }
  else
  {
    currentThreadRegisters = currentThread->kernel_registers_;
    ret = 0;
  }

  if (currentThread->Userthread)
  {
     UserThread* current = (UserThread*) currentThread;

    //do atomic checks
    if (!current->getflags()->knotcancelable.test_and_set())
    {
      if (current->getflags()->kasynchronous.test_and_set())
      {
        if (current->getflags()->kcancelreq.test_and_set())
        {
          currentThreadRegisters = currentThread->kernel_registers_;
          ret = 0;
          currentThreadRegisters->rdi = (uint64_t) PTHREAD_CANCELED;
          ArchThreads::changeInstructionPointer(currentThreadRegisters, (void*) &Syscall::pthread_exit);
          currentThread->switch_to_userspace_ = 0;
        }
        else
          current->getflags()->kcancelreq.clear();
      }
      else
        current->getflags()->kasynchronous.clear();
    }
    else 
      current->getflags()->knotcancelable.clear();
  }

  /*if (currentThread->Userthread)
  {
    currentThread->switch_to_userspace_ = 0;
    debug(X_THREADSTACK, "checking sleepflag for [%ld]\n", currentThread->getTID());
    size_t* test = (size_t*)((UserThread*) currentThread)->getUserstackStart();
    test -= sizeof(size_t);
    *test = 0;
    debug(X_THREADSTACK, "DAS HAT FUNKTIONIERT ARSCHLOCH\n");
    debug(X_THREADSTACK, "NOW also geting PF at %p????: %ld\n",test, *test);
    size_t* sleepflag = getToFlag(currentThread);
    debug(X_THREADSTACK, "checking sleepflag for [%ld]: %ld\n", currentThread->getTID(), *sleepflag);
    size_t sleeping = __atomic_exchange_n(sleepflag, 0, ustl::memory_order_seq_cst);

    if (sleeping == 1)
    {
      debug(X_THREADSTACK, "found out that [%ld] wants to sleep\n", currentThread->getTID());
      currentThreadRegisters = currentThread->kernel_registers_;
      ret = 0;
      currentThread->switch_to_userspace_ = 0;
      sleep();
    }
    currentThread->switch_to_userspace_ = 1;
  }
  */
  

  return ret;
}

void Scheduler::addNewThread(Thread *thread)
{
  assert(thread);
  debug(SCHEDULER, "addNewThread: %p  %zd:%s\n", thread, thread->getTID(), thread->getName());
  if (currentThread)
    ArchThreads::debugCheckNewThread(thread);
  KernelMemoryManager::instance()->getKMMLock().acquire();
  lockScheduling();
  KernelMemoryManager::instance()->getKMMLock().release();
  threads_.push_back(thread);
  unlockScheduling();
}

void Scheduler::sleep()
{
  debug(X_THREADSTACK, "sleep called!\n");
  if (currentThread->Userthread)
  {
    debug(X_THREADSTACK, "by a userthread!\n");
    size_t* sleepflag = getToFlag(currentThread);
    debug(X_THREADSTACK, "flag was %ld settingnow to 1\n", *sleepflag);
    __atomic_exchange_n(sleepflag, 1, ustl::memory_order_seq_cst);
    debug(SCHEDULER, "now setting current [%ld] to sleep\n", currentThread->getTID());
  }
  
  currentThread->setState(Sleeping);
  assert(block_scheduling_ == 0);
  yield();
}

void Scheduler::wake(Thread* thread_to_wake)
{

  // wait until the thread is sleeping
  while(thread_to_wake->getState() != Sleeping)
    yield();
  
  if (thread_to_wake->Userthread)
  {
    size_t* sleepflag = getToFlag(thread_to_wake);
    debug(SCHEDULER, "now waking up current [%ld]\n", currentThread->getTID());
    __atomic_exchange_n(sleepflag, 0, ustl::memory_order_seq_cst);
  }

  thread_to_wake->setState(Running);
}

void Scheduler::yield()
{
  assert(this);
  if (!ArchInterrupts::testIFSet())
  {
    assert(currentThread);
    kprintfd("Scheduler::yield: WARNING Interrupts disabled, do you really want to yield ? (currentThread %p %s)\n",
             currentThread, currentThread->name_.c_str());
    currentThread->printBacktrace();
  }
  ArchThreads::yield();
}

void Scheduler::cleanupDeadThreads()
{
  /* Before adding new functionality to this function, consider if that
     functionality could be implemented more cleanly in another place.
     (e.g. Thread/Process destructor) */

  lockScheduling();
  uint32 thread_count_max = threads_.size();
  if (thread_count_max > 1024)
    thread_count_max = 1024;
  Thread* destroy_list[thread_count_max];
  uint32 thread_count = 0;
  for (uint32 i = 0; i < threads_.size(); ++i)
  {
    Thread* tmp = threads_[i];
    if (tmp->getState() == ToBeDestroyed)
    {
      destroy_list[thread_count++] = tmp;
      threads_.erase(threads_.begin() + i); // Note: erase will not realloc!
      --i;
    }
    if (thread_count >= thread_count_max)
      break;
  }
  unlockScheduling();
  if (thread_count > 0)
  {
    for (uint32 i = 0; i < thread_count; ++i)
    {
      delete destroy_list[i];
    }
    debug(SCHEDULER, "cleanupDeadThreads: done\n");
  }
}

void Scheduler::printThreadList()
{
  lockScheduling();
  debug(SCHEDULER, "Scheduler::printThreadList: %zd Threads in List\n", threads_.size());
  for (size_t c = 0; c < threads_.size(); ++c)
    debug(SCHEDULER, "Scheduler::printThreadList: threads_[%zd]: %p  %zd:%s     [%s]\n", c, threads_[c],
          threads_[c]->getTID(), threads_[c]->getName(), Thread::threadStatePrintable[threads_[c]->state_]);
  unlockScheduling();
}

void Scheduler::lockScheduling() //not as severe as stopping Interrupts
{
  if (unlikely(ArchThreads::testSetLock(block_scheduling_, 1)))
    kpanict("FATAL ERROR: Scheduler::*: block_scheduling_ was set !! How the Hell did the program flow get here then ?\n");
}

void Scheduler::unlockScheduling()
{
  block_scheduling_ = 0;
}

bool Scheduler::isSchedulingEnabled()
{
  if (this)
    return (block_scheduling_ == 0);
  else
    return false;
}

bool Scheduler::isCurrentlyCleaningUp()
{
  return currentThread == &cleanup_thread_;
}

uint32 Scheduler::getTicks()
{
  return ticks_;
}

void Scheduler::incTicks()
{
  ++ticks_;
}

void Scheduler::printStackTraces()
{
  lockScheduling();
  debug(BACKTRACE, "printing the backtraces of <%zd> threads:\n", threads_.size());

  for (ustl::list<Thread*>::iterator it = threads_.begin(); it != threads_.end(); ++it)
  {
    (*it)->printBacktrace();
    debug(BACKTRACE, "\n");
    debug(BACKTRACE, "\n");
  }

  unlockScheduling();
}

void Scheduler::printLockingInformation()
{
  size_t thread_count;
  Thread* thread;
  lockScheduling();
  kprintfd("\n");
  debug(LOCK, "Scheduler::printLockingInformation:\n");
  for (thread_count = 0; thread_count < threads_.size(); ++thread_count)
  {
    thread = threads_[thread_count];
    if(thread->holding_lock_list_ != 0)
    {
      Lock::printHoldingList(threads_[thread_count]);
    }
  }
  for (thread_count = 0; thread_count < threads_.size(); ++thread_count)
  {
    thread = threads_[thread_count];
    if(thread->lock_waiting_on_ != 0)
    {
      debug(LOCK, "Thread %s (%p) is waiting on lock: %s (%p).\n", thread->getName(), thread,
            thread->lock_waiting_on_ ->getName(), thread->lock_waiting_on_ );
    }
  }
  debug(LOCK, "Scheduler::printLockingInformation finished\n");
  unlockScheduling();
}
