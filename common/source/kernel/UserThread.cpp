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
  parent_process_(parent_process), flag_mutex_{"thread::flag_mutex_"}, condition_mutex_{"Thread::cond_mutex_"},join_cond_{&condition_mutex_, 
  "Thread::join_cond"},  myret_lock_{"thread::retvallock"} // UserThread's members
{
  debug(USERTHREAD, "TID [%ld]: first thread constructor.\n", getTID());
  loader_ = parent_process_->getLoader();
  
  // setup stack, UserRegisters and address space
  setupStack();
  ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(),
                                   getUserstackStart(),
                                   getKernelStackStartPointer());
  ArchThreads::setAddressSpace(this, loader_->arch_memory_);

  debug(X_USERTHREAD, "TID: [%ld], cr3: %lx, rsp: %lx, rip: %lx\n", 
    getTID(), user_registers_->cr3, user_registers_->rsp, user_registers_->rip);
  debug(X_USERTHREAD, "TID [%ld]: Stack, Registers and AddressSpace set.\n", getTID());

  // set terminal
  if (main_console->getTerminal(terminal_number))
    setTerminal(main_console->getTerminal(terminal_number));

  // add Thread to process to scheduler
  parent_process_->addToThreadList(this);
  Scheduler::instance()->addNewThread((Thread*)this);

  debug(X_USERTHREAD, "TID [%ld]: first thread constructor finished\n", getTID());
  switch_to_userspace_ = 1;
}

// pthread_create
UserThread::UserThread(size_t wrapper, uint32_t terminal_number) :
  Thread(((UserThread*)currentThread)->working_dir_, ((UserThread*)currentThread)->name_, 
          USER_THREAD, ProcessRegistry::instance()->createID()),
  parent_process_(((UserThread*)currentThread)->parent_process_), flag_mutex_{"thread::flag_mutex_"},
   condition_mutex_{"Thread::cond_mutex_"},join_cond_{&condition_mutex_, "Thread::join_cond"}, myret_lock_{"thread::retvallock"} // UserThread's members
{
  //debug(USERTHREAD, "TID [%ld]: pthread thread constructor. start_routine = %lx\n", getTID(), start_routine);
  loader_ = parent_process_->getLoader();

  // set up user registers and adressspace
  setupStack();
  ArchThreads::createUserRegisters(user_registers_, (void*)wrapper, 
                                   getUserstackStart(), getKernelStackStartPointer());

  debug(X_USERTHREAD, "TID: [%ld], cr3: %lx, rsp: %lx, rip: %lx\n", 
    getTID(), user_registers_->cr3, user_registers_->rsp, user_registers_->rip);
  ArchThreads::setAddressSpace(this, loader_->arch_memory_);
  debug(X_USERTHREAD, "TID [%ld]: Registers and AddressSpace set.\n", getTID());

  // set terminal
  if (main_console->getTerminal(terminal_number))
    setTerminal(main_console->getTerminal(terminal_number));

  // add Thread to process to scheduler
  parent_process_->addToThreadList(this);
  Scheduler::instance()->addNewThread((Thread*)this);

  debug(X_USERTHREAD, "TID [%ld]: pthread thread constructor finished\n", getTID());
  switch_to_userspace_ = 1;
}

UserThread::~UserThread()
{
  switch_to_userspace_ = 0;
  debug(X_USERTHREAD, "~UserThread called for thread [%ld] in pid: [%ld] called %s . removing from UserProcess::threads_\n", tid_, parent_process_->getPID(), name_.c_str());

  
  if(isLast())
  {
    debug(X_USERTHREAD, "Last Thread with TID [%ld] from process [%ld]. Deleting parent_process_\n", getTID(), parent_process_->getPID());
    delete parent_process_;
  }
  debug(X_USERTHREAD, "returning from my killing\n");
  switch_to_userspace_ = 1;
}

bool UserThread::setupStack()
{
  debug(USERTHREAD, "TID: [%ld] setupStack()\n", getTID());
  bool vpn_mapped = false;
  size_t ppn_for_stack = 0;
  size_t vpn_for_stack = 0;
  size_t stack_page_offset = getTID() * PAGE_SIZE * PAGE_TABLE_ENTRIES * PAGE_DIR_ENTRIES * STACK_SIZE_MAX_IN_MB; // 4096KB * 512 * 512 = 1 MB
  size_t stack_start_ptr = USER_BREAK - sizeof(size_t) - stack_page_offset;

  // calc
  vpn_for_stack = stack_start_ptr / PAGE_SIZE; 
  ppn_for_stack = PageManager::instance()->allocPPN();
  vpn_mapped = loader_->arch_memory_.mapPage(vpn_for_stack, ppn_for_stack, 1);

  // worked? 
  debug(USERTHREAD, "setupStack() trying to map: vpn %lx to ppn %lx. stack lies at %lx\n", vpn_for_stack, ppn_for_stack, userstack_start_);
  assert(vpn_for_stack && ppn_for_stack);
  if(!vpn_mapped)
  {
    debug(USERTHREAD, "setupStack() RIP. returning false\n");
    PageManager::instance()->freePPN(ppn_for_stack);
    return false;
  }

  // success man
  userstack_start_ = stack_start_ptr;
  debug(USERTHREAD, "setupStack() success. returning true\n");
  return true;
}