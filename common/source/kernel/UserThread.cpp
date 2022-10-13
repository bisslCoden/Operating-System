#include "debug.h"
#include "UserThread.h"
#include "FileSystemInfo.h"
#include "ProcessRegistry.h"
#include "UserProcess.h"
#include "kprintf.h"
#include "Console.h"
#include "Loader.h"
#include "VfsSyscall.h"
#include "File.h"
#include "ArchMemory.h"
#include "PageManager.h"
#include "ArchThreads.h"
#include "offsets.h"

// first thread
UserThread::UserThread(UserProcess* parent_process, FileSystemInfo* working_dir, ustl::string name, uint32 terminal_number) : 
  Thread(working_dir, name, USER_THREAD, ProcessRegistry::instance()->createID()), // Thread's constructor
  parent_process_(parent_process), flag_mutex_{"thread::flag_mutex_"}// UserThread's members
{
  debug(USERTHREAD, "TID [%ld]: first thread constructor.\n", getTID());
  loader_ = parent_process_->getLoader();

  // setup stack, UserRegisters and address space
  setupStack(THREADSETUP_FIRST);

  ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(),
                                   getUserstackStart(),
                                   getKernelStackStartPointer());

  ArchThreads::setAddressSpace(this, loader_->arch_memory_);
  debug(X_USERTHREAD, "TID [%ld]: Stack, Registers and AddressSpace set.\n", getTID());

  // set terminal
  if (main_console->getTerminal(terminal_number))
    setTerminal(main_console->getTerminal(terminal_number));

  // add Thread to process and LATER to Scheduler
  parent_process_->addToThreadList(this);
  Scheduler::instance()->addNewThread((Thread*)this);

  debug(X_USERTHREAD, "TID [%ld]: first thread constructor finished\n", getTID());
  switch_to_userspace_ = 1;
}

// pthread_create
UserThread::UserThread(size_t start_routine, uint32_t terminal_number) :
  Thread(((UserThread*)currentThread)->working_dir_, ((UserThread*)currentThread)->name_, 
          USER_THREAD, ProcessRegistry::instance()->createID()),
  parent_process_(((UserThread*)currentThread)->parent_process_), flag_mutex_{"thread::flag_mutex_"}
{
  debug(USERTHREAD, "TID [%ld]: pthread thread constructor. start_routine = %lx\n", getTID(), start_routine);
  loader_ = parent_process_->getLoader();

  // set up user registers and adressspace
  setupStack(THREADSETUP_PTHREAD);
  ArchThreads::createUserRegisters(user_registers_, (void*)start_routine,
                                   getUserstackStart(), getKernelStackStartPointer());
  ArchThreads::setAddressSpace(this, loader_->arch_memory_);
  debug(X_USERTHREAD, "TID [%ld]: Registers and AddressSpace set.\n", getTID());

  // set terminal
  if (main_console->getTerminal(terminal_number))
    setTerminal(main_console->getTerminal(terminal_number));

  // add Thread to process and LATER to scheduler
  parent_process_->addToThreadList(this);
  Scheduler::instance()->addNewThread((Thread*)this);

  debug(X_USERTHREAD, "TID [%ld]: pthread thread constructor finished\n", getTID());
  switch_to_userspace_ = 1;
}

UserThread::UserThread(UserProcess *parent) :
  Thread(parent->getWorkingDir(), parent->getName(), Thread::USER_THREAD,ProcessRegistry::instance()->createID()),
  parent_process_(parent)
{
  loader_ = parent->getLoader();

  ArchThreads::createUserRegisters(user_registers_,
                                   loader_->getEntryFunction(),
                                   NULL,
                                   getKernelStackStartPointer());

  *user_registers_ = *currentThread->user_registers_;
  user_registers_->rax = 0;
  user_registers_->rsp0 = (size_t) getKernelStackStartPointer();

  ArchThreads::setAddressSpace(this, loader_->arch_memory_);

  switch_to_userspace_ = 1;
  ArchThreads::printThreadRegisters(this);
}

UserThread::~UserThread()
{
  switch_to_userspace_ = 0;
  //race conditionnn! because when exit is called during pthread exit page get freed 2 times
  debug(X_USERTHREAD, "~UserThread called for thread [%ld] %s.\n", tid_, name_.c_str());
  //can be used if we find our Userstack :S
  //if (!isUserStackCanaryOK())
  //{
    //debug(USER_THREAD, "Stack got corrupted!\n");
    //kill progamm then 
  //}
  //free Stack Pages
  //debug(X_USERTHREAD, "freeing Stack page: %ld for thread [%ld]: %s\n", ppns_for_userstack_[0] ,tid_, name_);
  //debug(X_USERTHREAD, "calling freePPN(%lx) in ~UserThread for thread %ld\n", ppns_for_userstack_[0], getTID());
  // PageManager::instance()->freePPN(ppns_for_userstack_[0]);

  /* vpns_for_userstack currently not set
  for (size_t i = 0; i < USERSTACK_SIZE; i++)
  {
    if(!loader_->arch_memory_.unmapPage(vpns_for_userstack_[i]))
      debug(X_USERTHREAD, "could not free vpn :?\n");
  }
  */

  //delete Thread from Process and sheduler
  // calls delete for process if the thread is the last thread
  parent_process_->lockThreadMutex();
  parent_process_->removeFromThreadList(this);
  parent_process_->unLockThreadMutex();

  if(isLast())
  {
    debug(X_USERTHREAD, "Last Thread with TID [%ld] from process [%ld]. Deleting parent_process_\n", getTID(), parent_process_->getPID());
    delete parent_process_;
  }
  debug(X_USERTHREAD, "~UserThread called for thread [%ld] parent process died.\n", tid_);
  switch_to_userspace_ = 1;
}

bool UserThread::isUserStackCanaryOK()
{
  return (userstack_start_ == STACK_CANARY && userstack_end_ == STACK_CANARY);
}

bool UserThread::setupStack(int first_thread)
{
  debug(USERTHREAD, "setupStack(first_thread = %d)\n", first_thread);
  size_t vpn_for_stack = 0;
  size_t ppn_for_stack = 0;
  bool vpn_mapped = false;
  if(first_thread == THREADSETUP_FIRST)
  {
    userstack_start_= USER_BREAK - sizeof(size_t);
    vpn_for_stack = userstack_start_ / PAGE_SIZE;
    ppn_for_stack = PageManager::instance()->allocPPN();
    vpn_mapped = loader_->arch_memory_.mapPage(vpn_for_stack, ppn_for_stack, 1);
  }
  else if(first_thread == THREADSETUP_PTHREAD)
  {
    size_t stack_page_offset = getTID() * PAGE_SIZE;
    userstack_start_ = USER_BREAK - sizeof(size_t) - stack_page_offset;
    vpn_for_stack = userstack_start_ / PAGE_SIZE;
    ppn_for_stack = PageManager::instance()->allocPPN();
    vpn_mapped = loader_->arch_memory_.mapPage(vpn_for_stack, ppn_for_stack, 1);
  }

  // worked?
  debug(USERTHREAD, "setupStack() trying to map: vpn %lx to ppn %lx\n", vpn_for_stack, ppn_for_stack);
  assert(vpn_for_stack && ppn_for_stack);
  if(!vpn_mapped)
  {
    debug(USERTHREAD, "setupStack() RIP. returning false\n");
    PageManager::instance()->freePPN(ppn_for_stack);
    return false;
  }
  debug(USERTHREAD, "setupStack() success. returning true\n");
  return true;
}