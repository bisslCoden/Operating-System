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
  parent_process_(parent_process) // UserThread's members
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
  parent_process_(((UserThread*)currentThread)->parent_process_)
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
  debug(X_USERTHREAD, "~UserThread called for thread [%ld] in pid: [%ld] called %s . removing from UserProcess::threads_\n", tid_, parent_process_->getPID(), name_.c_str());
  parent_process_->removeFromThreadList(this);
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