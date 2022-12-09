#include "PageFaultHandler.h"
#include "kprintf.h"
#include "Thread.h"
#include "ArchInterrupts.h"
#include "offsets.h"
#include "Scheduler.h"
#include "Loader.h"
#include "Syscall.h"
#include "ArchThreads.h"
#include "PageManager.h"
#include "ArchMemory.h"

extern "C" void arch_contextSwitch();

const size_t PageFaultHandler::null_reference_check_border_ = PAGE_SIZE;

inline bool PageFaultHandler::checkPageFaultIsValid(size_t address, bool user,
                                                    bool present, bool switch_to_us, bool writing)
{
  Scheduler::instance()->printThreadList();
  debug(X_PAGEFAULT, "Entered checkPageFaultIsValid(). CurrentThread %p: with name %s \n", currentThread, currentThread->getName());
  if(currentThread->isUserThread())
    debug(X_PAGEFAULT, "currentthread is a userthread also\n");
  assert((user == switch_to_us) && "Thread is in user mode even though is should not be.");
  assert(!(address < USER_BREAK && currentThread->loader_ == 0) && "Thread accesses the user space, but has no loader.");
  assert(!(user && currentThread->user_registers_ == 0) && "Thread is in user mode, but has no valid registers.");
 // assert(address && "addres of pagefault was 0");

  if(address < null_reference_check_border_)
  {
    debug(PAGEFAULT, "Maybe you are dereferencing a null-pointer.\n");
  }
  else if(!user && address >= USER_BREAK)
  {
    debug(PAGEFAULT, "You are accessing an invalid kernel address.\n");
  }
  else if(user && address >= USER_BREAK)
  {
    debug(PAGEFAULT, "You are accessing a kernel address in user-mode.\n");
  }
  else if(present && !writing)
  {
    debug(PAGEFAULT, "You got a pagefault even though the address is mapped. and writable\n");
  }
  else
  {
    debug(PAGEFAULT, "everything seems to be okay....\n");
    return true;
  }
  debug(PAGEFAULT, "OH NO... pagefault invalid?!?!?\n");
  return false;
}

inline void PageFaultHandler::handlePageFault(size_t address, bool user,
                                          bool present, bool writing,
                                          bool fetch, bool switch_to_us)
{
  if (PAGEFAULT & OUTPUT_ENABLED)
    kprintfd("\n");
  debug(PAGEFAULT, "TID: [%zu], Address: %18zx - name: %s (%p)\n",
        currentThread->getTID(), address, currentThread->getName(), currentThread);
  debug(PAGEFAULT, "Flags: %spresent, %s-mode, %s, %s-fetch, switch to userspace: %1d\n",
        present ? "    " : "not ",
        user ? "  user" : "kernel",
        writing ? "writing" : "reading",
        fetch ? "instruction" : "    operand",
        switch_to_us);

  ArchThreads::printThreadRegisters(currentThread, false);
  

  //test
  if (checkPageFaultIsValid(address, user, present, switch_to_us, writing))
  {
    if (PageManager::instance()->checkForCow(address))
    {
      debug(PAGEFAULT, "Copy on Write found + copied page. returning.\n");
      return;
    }
    if (switch_to_us && address > END_OF_STACKS)
    {
      debug(PAGEFAULT, "checking for stack-extension....\n");
      currentUserProcess->lockThreadMutex();
      UserThread* stack_owner = 0;
      if((stack_owner = currentUserThread->getProcess()->checkStackAdress(address)) != 0)
      {
        debug(PAGEFAULT, "seems like our currentthread just wants a new Page for someone!\n");
        stack_owner->getNewStackPage(address);  
        currentUserProcess->unLockThreadMutex();    
      }
      else
      {
        currentUserProcess->unLockThreadMutex();
        debug(PAGEFAULT, "OH OH... Pagefault invalid!\n");
            // the page-fault seems to be faulty, print out the thread stack traces
        ArchThreads::printThreadRegisters(currentThread, true);
        currentThread->printBacktrace(true);
        if (currentThread->loader_)
          Syscall::exit(9999);
        else
          currentThread->kill();
      }
    }
    else
    {
      currentThread->loader_->loadPage(address);
    }
  }
  else
  {
    // the page-fault seems to be faulty, print out the thread stack traces
    ArchThreads::printThreadRegisters(currentThread, true);
    currentThread->printBacktrace(true);
    if (currentThread->loader_)
      Syscall::exit(9999);
    else
      currentThread->kill();
  }
  debug(PAGEFAULT, "Page fault handling finished for Address: %18zx.\n", address);
}

void PageFaultHandler::enterPageFault(size_t address, bool user,
                                      bool present, bool writing,
                                      bool fetch)
{
  assert(currentThread && "You have a pagefault, but no current thread");
  //save previous state on stack of currentThread
  uint32 saved_switch_to_userspace = currentThread->switch_to_userspace_;

  currentThread->switch_to_userspace_ = 0;
  currentThreadRegisters = currentThread->kernel_registers_;
  ArchInterrupts::enableInterrupts();

  handlePageFault(address, user, present, writing, fetch, saved_switch_to_userspace);

  ArchInterrupts::disableInterrupts();
  currentThread->switch_to_userspace_ = saved_switch_to_userspace;
  if (currentThread->switch_to_userspace_)
    currentThreadRegisters = currentThread->user_registers_;
}
