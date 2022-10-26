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
UserThread::UserThread(UserProcess* parent_process, FileSystemInfo* working_dir, ustl::string name, uint32 terminal_number, size_t page_offset) : 
  Thread(working_dir, name, USER_THREAD, ProcessRegistry::instance()->createID()), // Thread's constructor
  parent_process_(parent_process), flag_mutex_{"thread::flag_mutex_"}, condition_mutex_{"Thread::cond_mutex_"},join_cond_{&condition_mutex_, 
  "Thread::join_cond"} // UserThread's members
{
  debug(USERTHREAD, "TID [%ld]: first thread constructor.\n", getTID());
  loader_ = parent_process_->getLoader();
  Userthread = true;
  //  size_t sleepflag = 1;
  // size_t res = __atomic_exchange_n(&sleepflag, 0, ustl::memory_order_seq_cst);
  // debug(X_THREADSTACK, "jst checking exchange: sleep = %ld and check = %ld\n", sleepflag, res);
  // setup stack, UserRegisters and address space
  mystack_.page_offset_ = page_offset;
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

bool UserThread::schedulable(){
  
  if (getState() == Running)
  {
    // size_t addr = (size_t) mystack_.userstack_start_ + sizeof(size_t);
    // addr = *(size_t*)addr;
    size_t sleepy = __atomic_exchange_n(mystack_.UserMutex, 0, ustl::memory_order_seq_cst);
    debug(X_THREADSTACK, "Tid[%ld] sleepy = %ld\n", getTID(), sleepy);
    if (sleepy == 0)
    {
      return true;
    }
    else if(sleepy == 1)
    {
      //get the right flag back
      __atomic_exchange_n(mystack_.UserMutex, 1, ustl::memory_order_seq_cst);
      return false;
    }
    else
    {
      assert(false && "Sleep flag was neither 1 nor 0?\n");
    }
  }
  return false;
}


// pthread_create
UserThread::UserThread(size_t wrapper, size_t page_offset, uint32_t terminal_number) :
  Thread(((UserThread*)currentThread)->working_dir_, ((UserThread*)currentThread)->name_, 
          USER_THREAD, ProcessRegistry::instance()->createID()),
  parent_process_(((UserThread*)currentThread)->parent_process_), flag_mutex_{"thread::flag_mutex_"},
   condition_mutex_{"Thread::cond_mutex_"},join_cond_{&condition_mutex_, "Thread::join_cond"} // UserThread's members
{
  //debug(USERTHREAD, "TID [%ld]: pthread thread constructor. start_routine = %lx\n", getTID(), start_routine);
  loader_ = parent_process_->getLoader();
  mystack_.page_offset_ = page_offset;

  Userthread = true;

  // set up user registers and adressspace
  setupStack();
  ArchThreads::createUserRegisters(user_registers_, (void*)wrapper,
                                   getUserstackStart(), getKernelStackStartPointer());

  debug(X_USERTHREAD, "TID: [%ld], cr3: %lx, rsp: %lx (stackstart %lx), rip: %lx\n", 
    getTID(), user_registers_->cr3, user_registers_->rsp, mystack_.userstack_start_, user_registers_->rip);
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

// Constructor of UserThread for fork
UserThread::UserThread(UserProcess *child, UserThread* parent_thread) :
  Thread(child->getWorkingDir(), "fork thread", Thread::USER_THREAD,parent_thread->getTID()),
  parent_process_(child),flag_mutex_{"thread::flag_mutex_"}, condition_mutex_{"Thread::cond_mutex_"},join_cond_{&condition_mutex_, 
  "Thread::join_cond"}
{
  loader_ = child->getLoader();

  ArchThreads::createUserRegisters(user_registers_,
                                   (void*) parent_thread->user_registers_->rip,
                                   parent_thread->getUserstackStart(),
                                   parent_thread->getKernelStackStartPointer());

  memcpy(user_registers_, parent_thread->user_registers_, sizeof(ArchThreadRegisters));
  user_registers_->rax = 0;
  user_registers_->rsp0 = (size_t) getKernelStackStartPointer();

  Userthread = true;

  ArchThreads::setAddressSpace(this, child->getLoader()->arch_memory_);

  switch_to_userspace_ = 1;
  debug(X_USERTHREAD, "TID [%ld]: pthread thread constructor for fork finished\n", getTID());
  //ArchThreads::printThreadRegisters(this);
}

UserThread::~UserThread()
{
  switch_to_userspace_ = 0;
  //debug(X_USERTHREAD, "~UserThread called for thread [%ld] in pid: [%ld] called %s . removing from UserProcess::threads_\n", tid_, parent_process_->getPID(), name_.c_str());

  
  if(isLast())
  {
    debug(X_USERTHREAD, "Last Thread with TID [%ld] from process [%ld]. Deleting parent_process_\n", getTID(), parent_process_->getPID());
    delete parent_process_;
  }
  //debug(X_USERTHREAD, "returning from my killing\n");
  switch_to_userspace_ = 1;
}


void UserThread::setCancelState(int state){ 
  myflags_.cancelable = state;
  if(state == PTHREAD_CANCEL_DISABLE)
    myflags_.knotcancelable.test_and_set();
  else
    myflags_.knotcancelable.clear();
  return;
}
void UserThread::setCancelType(int type) { 
  myflags_.deferred = type; 
  if (type == PTHREAD_CANCEL_ASYNCHRONOUS)
    myflags_.kasynchronous.test_and_set();
  else
    myflags_.kasynchronous.clear();
  return; 
}
void UserThread::sendCancelRequest(){ 
  myflags_.cancelreq = true;
  myflags_.kcancelreq.test_and_set();
  return; 
  }

bool UserThread::setupStack()
{
  //debug(USERTHREAD, "TID: [%ld] setupStack()\n", getTID());
  bool vpn_mapped = false;
  size_t ppn_for_stack = 0;
  debug(X_THREADSTACK, "TID[%ld] my offset IIIIS: %ld\n", tid_, mystack_.page_offset_);
  size_t vpn_for_stack = 0;
  size_t stack_page_offset = mystack_.page_offset_ * PAGE_SIZE * PAGE_TABLE_ENTRIES * PAGE_DIR_ENTRIES * STACK_SIZE_MAX_IN_MB; // 4096KB * 512 * 512 = 4 MB
  size_t stack_start_ptr = USER_BREAK - sizeof(size_t) - stack_page_offset;

  // calc
  vpn_for_stack = stack_start_ptr / PAGE_SIZE; 
  ppn_for_stack = PageManager::instance()->allocPPN();
  debug(X_USERTHREAD, "got my first physical page with number %ld\n", ppn_for_stack);
  vpn_mapped = loader_->arch_memory_.mapPage(vpn_for_stack, ppn_for_stack, 1);

  // worked? 
  //debug(USERTHREAD, "setupStack() trying to map: vpn %lx to ppn %lx.\n", vpn_for_stack, ppn_for_stack);
  assert(vpn_for_stack && ppn_for_stack);
  if(!vpn_mapped)
  {
    debug(USERTHREAD, "setupStack() RIP. returning false\n");
    assert(false);
    PageManager::instance()->freePPN(ppn_for_stack);
    return false;
  }


  mystack_.userstack_start_ = stack_start_ptr - sizeof(size_t);
  mystack_.UserMutex = ((size_t*) ArchMemory::getIdentAddressOfPPN(ppn_for_stack)) + PAGE_SIZE;

  *mystack_.UserMutex = 0;

  debug(USERTHREAD, "[%ld]: my stack starts at: %lx and flag is at %lx"
  "in kernel mapping %p\n",tid_, mystack_.userstack_start_, stack_start_ptr, mystack_.UserMutex);


  return true;
}
