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
UserThread::UserThread(UserProcess* process_, FileSystemInfo* working_dir, ustl::string name, uint32 terminal_number, size_t page_offset) : 
  Thread(working_dir, name, USER_THREAD, ProcessRegistry::instance()->createID()), // Thread's constructor
  process_(process_), 
  flag_mutex_{"thread::flag_mutex_"}, 
  condition_mutex_{"Thread::cond_mutex_"},
  join_cond_{ &condition_mutex_, "Thread::join_cond" } // UserThread's members
{
  debug(USERTHREAD, "TID [%ld]: first thread constructor.\n", getTID());
  loader_ = process_->getLoader();
  Userthread = true;
  //  size_t sleepflag = 1;
  // size_t res = __atomic_exchange_n(&sleepflag, 0, ustl::memory_order_seq_cst);
  // debug(X_THREADSTACK, "jst checking exchange: sleep = %ld and check = %ld\n", sleepflag, res);
  // setup stack, UserRegisters and address space
  mystack_.page_offset_ = page_offset;
  setupStack();
  ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(),
                                   (void*) mystack_.userstack_start_,
                                   getKernelStackStartPointer());
  ArchThreads::setAddressSpace(this, loader_->arch_memory_);
  debug(X_USERTHREAD, "TID: [%ld], cr3: %lx, rsp: %lx, rip: %lx\n",
    getTID(), user_registers_->cr3, user_registers_->rsp, user_registers_->rip);

  // set terminal
  if (main_console->getTerminal(terminal_number))
    setTerminal(main_console->getTerminal(terminal_number));

  // add Thread to process to scheduler
  process_->addToThreadList(this);
  Scheduler::instance()->addNewThread((Thread*)this);

  //should be threadsafe??
  process_->lockKill();
  if (process_->checkKill())
  {
    switch_to_userspace_ = 0;
    ArchThreads::changeInstructionPointer(kernel_registers_, (void*) Syscall::pthread_exit);
    process_->unlockKill();
  }
  else
  {
    process_->unlockKill();
    switch_to_userspace_ = 1;
  }
}

bool UserThread::schedulable(){
  
  if (getState() == Running)
  {
    //testsystem
    //checks if exit is called
    //debug(X_THREADSTACK, "schedulable called for thread %ld by thread %ld!\n", getTID(), currentThread->getTID());
    if (!getflags()->knotcancelable.test_and_set())
      {
        if (getflags()->kasynchronous.test_and_set())
        {
          if (getflags()->kcancelreq.test_and_set())
          {
            return true;
          }
          else
            getflags()->kcancelreq.clear();
        }
        else
          getflags()->kasynchronous.clear();
      }
      else 
        getflags()->knotcancelable.clear();
    
    
    size_t sleepy = __atomic_exchange_n(mystack_.UserMutex, 0, ustl::memory_order_seq_cst);
    //debug(X_THREADSTACK, "Tid[%ld] sleepy = %ld\n", getTID(), sleepy);
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
    debug(X_THREADSTACK, "schedulable finished!\n");
  }
  return false;
}


// pthread_create
UserThread::UserThread(size_t wrapper, size_t page_offset, uint32_t terminal_number) :
  Thread(((UserThread*)currentThread)->working_dir_, ((UserThread*)currentThread)->name_, 
          USER_THREAD, ProcessRegistry::instance()->createID()),
  process_(((UserThread*)currentThread)->process_), flag_mutex_{"thread::flag_mutex_"},
   condition_mutex_{"Thread::cond_mutex_"},join_cond_{&condition_mutex_, "Thread::join_cond"} // UserThread's members
{
  //debug(USERTHREAD, "TID [%ld]: pthread thread constructor. start_routine = %lx\n", getTID(), start_routine);
  loader_ = process_->getLoader();
  mystack_.page_offset_ = page_offset;

  Userthread = true;

  // set up user registers and adressspace
  setupStack();
  ArchThreads::createUserRegisters(user_registers_, (void*)wrapper,
                                   (void*) mystack_.userstack_start_, getKernelStackStartPointer());

  debug(X_USERTHREAD, "TID: [%ld], cr3: %lx, rsp: %lx (stackstart %lx), rip: %lx\n", 
    getTID(), user_registers_->cr3, user_registers_->rsp, mystack_.userstack_start_, user_registers_->rip);
  ArchThreads::setAddressSpace(this, loader_->arch_memory_);
  debug(X_USERTHREAD, "TID [%ld]: Registers and AddressSpace set.\n", getTID());

  // set terminal
  if (main_console->getTerminal(terminal_number))
    setTerminal(main_console->getTerminal(terminal_number));

  // add Thread to process to scheduler
  process_->addToThreadList(this);

  Scheduler::instance()->addNewThread((Thread*)this);

  process_->lockKill();
  if (process_->checkKill())
  {
    switch_to_userspace_ = 0;
    ArchThreads::changeInstructionPointer(kernel_registers_, (void*) Syscall::pthread_exit);
    process_->unlockKill();
  }
  else
  {
    process_->unlockKill();
    switch_to_userspace_ = 1;
  }
}

// fork
UserThread::UserThread(UserProcess *child, UserThread* parent_thread) :
  Thread(child->getWorkingDir(), "fork thread", Thread::USER_THREAD, ProcessRegistry::instance()->createID()),
  process_(child),flag_mutex_{"thread::flag_mutex_"}, condition_mutex_{"Thread::cond_mutex_"},join_cond_{&condition_mutex_, 
  "Thread::join_cond"}
{
  loader_ = child->getLoader();

  //cant we somehow just write a new setupstack... this makes me uncomfortable as we have 3 different constructors where we
  // play around with stacks

  StackInfo parent_stack = parent_thread->getStackInfo(); 
  mystack_ = parent_stack;


  ArchMemoryMapping map = loader_->arch_memory_.resolveMapping(mystack_.userstack_start_ / PAGE_SIZE);
  size_t location = (size_t) ArchMemory::getIdentAddressOfPPN(map.page_ppn);
  location += PAGE_SIZE - sizeof(size_t);
  mystack_.UserMutex = (size_t*) location;
  *mystack_.UserMutex = 0;

  ArchThreads::createUserRegisters(user_registers_,
                                   (void*) parent_thread->user_registers_->rip,
                                   (void*) mystack_.userstack_start_,
                                   parent_thread->getKernelStackStartPointer());


  memcpy(user_registers_, parent_thread->user_registers_, sizeof(ArchThreadRegisters));
  user_registers_->rax = 0;
  user_registers_->rsp0 = (size_t) getKernelStackStartPointer();

  Userthread = true;



  ArchThreads::setAddressSpace(this, child->getLoader()->arch_memory_);

  process_->lockKill();
  if (process_->checkKill())
  {
    switch_to_userspace_ = 0;
    ArchThreads::changeInstructionPointer(kernel_registers_, (void*) Syscall::pthread_exit);
    process_->unlockKill();
  }
  else
  {
    process_->unlockKill();
    switch_to_userspace_ = 1;
  }
  //ArchThreads::printThreadRegisters(this);
}

UserThread::~UserThread()
{
  switch_to_userspace_ = 0;
  //debug(X_USERTHREAD, "~UserThread called for thread [%ld] in pid: [%ld] called %s . removing from UserProcess::threads_\n", tid_, process_->getPID(), name_.c_str());
  
  if(isLast())
  {
    debug(X_USERTHREAD, "Last Thread with TID [%ld] from process [%ld]. Deleting process_\n", getTID(), process_->getPID());
    delete process_;
  }
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
  debug(X_USERTHREAD, "setupStack(): TID[%ld] my offset is: %lx\n", tid_, mystack_.page_offset_);

  // virtual address
  size_t stack_page_offset = mystack_.page_offset_ * PAGE_SIZE * PAGE_TABLE_ENTRIES * PAGE_DIR_ENTRIES * STACK_SIZE_MAX_IN_MB; // 4096KB * 512 * 512 = 4 MB
  size_t stack_start_ptr = USER_BREAK - sizeof(size_t) - stack_page_offset;
  size_t vpn_for_stack = stack_start_ptr / PAGE_SIZE; 
  // ppn
  size_t ppn_for_stack = PageManager::instance()->allocPPN();
  // check stack vpn and ppn + mapPage()
  if(!vpn_for_stack || !ppn_for_stack || !loader_->arch_memory_.mapPage(vpn_for_stack, ppn_for_stack, 1))
  {
    debug(USERTHREAD, "setupStack(): RIP. asserting.\n");
    assert(false);
    PageManager::instance()->freePPN(ppn_for_stack);
    return false;
  }
  debug(X_USERTHREAD, "setupStack(): mapPage(vpn_for_stack = %lx, ppn_for_stack = %lx)\n", vpn_for_stack, ppn_for_stack);


  mystack_.userstack_start_ = stack_start_ptr - sizeof(size_t);
  size_t location = (size_t) ArchMemory::getIdentAddressOfPPN(ppn_for_stack);
  location += PAGE_SIZE - sizeof(size_t);
  mystack_.UserMutex = (size_t*) location;
  *mystack_.UserMutex = 0;

  debug(USERTHREAD, "[%ld]: my stack starts at: %lx and flag is at %lx"
  "in kernel mapping %p\n",tid_, mystack_.userstack_start_, stack_start_ptr, mystack_.UserMutex);


  return true;
}

int UserThread::execv(char* const argv[], size_t argc)
{
  debug(X_USERTHREAD, "argc = %ld\n", argc);
  for(size_t i = 0; i < argc; i++)
    debug(X_USERTHREAD, "argv[%ld] = %s\n", i, argv[i]);

  /*
  // vaddr... virtual address for old archmemory
  size_t vpn = USER_BREAK / PAGE_SIZE - 1; 
  size_t vaddr_end = vpn * PAGE_SIZE;
  size_t vaddr_start = vaddr_end + sizeof(size_t) * argc + sizeof(size_t);
  debug(X_USERTHREAD, "execv() here: vpn = %lx, vaddr_start = %lx, vaddr_end = %lx)\n", vpn, vaddr_start, vaddr_end);
  // ident... identMapping of freshly allocated PPN
  size_t ppn = PageManager::instance()->allocPPN();
  size_t ident_end = ArchMemory::getIdentAddressOfPPN(ppn);
  size_t ident_start = ident_end + sizeof(size_t) * argc + sizeof(size_t);
  debug(X_USERTHREAD, "execv() here: ppn = %lx, ident_start = %lx, ident_end = %lx)\n", vpn, ident_start, ident_end);
  // map vpn to ppn (this is still the old loader_->arch_memory_)
  assert(loader_->arch_memory_.mapPage(vpn, ppn, 1) && "UserThread::execv() mapPage() failed");
  // iterate and copy from old archmem to ident
  debug(X_USERTHREAD, "mapped vpn and ppn. copying from old archmem to ident:\n");
  size_t vaddr_i = vaddr_start;
  size_t ident_i = ident_start;
  for(size_t i = 0; (i < argc) && (ident_i != ident_end); i++)
  {
    // copy pointer to char ptr to ident address
    *((size_t*)ident_i) = vaddr_i;
    memcpy((void*)ident_i, (void*)argv[i], strlen(argv[i]));
    // 
    ident_i += sizeof(size_t);
    vaddr_i += sizeof(char)*strlen(argv[i]) + 1;
    ident_i += sizeof(char)*strlen(argv[i]) + 1;
  }
  */

  // important: after setAddressSpace the cr3 register of the thread is updated to the new archmemory 
  name_ = process_->getName();
  loader_ = process_->getLoader();
  ArchThreads::setAddressSpace(this, loader_->arch_memory_);
  mystack_.page_offset_ = process_->getRandomPageOffset();
  setupStack();
  debug(X_USERTHREAD, "execv(): set name_ = %s, loader_ = %lx, setAddressSpace(), mystack_.page_offset_ = %lx\n", name_.c_str(), (size_t)loader_, mystack_.page_offset_);

  // iterate and copy from ident to new virtual memory

  // passing new virtual memory to userspace 
  user_registers_->rsp = (size_t)getUserstackStart();
  user_registers_->rip = (size_t)loader_->getEntryFunction();
  // user_registers_->rdi = (size_t)argv; 
  // user_registers_->rsi = argc;
  debug(X_USERTHREAD, "execv(): rip = %lx, rdi = %lx, rsi = %lx\n", user_registers_->rip, user_registers_->rdi, user_registers_->rsi);
  return 0;
}
