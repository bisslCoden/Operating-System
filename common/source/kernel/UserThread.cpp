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
UserThread::UserThread(UserProcess* process, FileSystemInfo* working_dir, ustl::string name, uint32 terminal_number, size_t page_offset) :
  Thread(working_dir, name, USER_THREAD, ProcessRegistry::instance()->createID()), // Thread's constructor
  process_(process),
  flag_mutex_{"thread::flag_mutex_"},
  join_cond_{ &process_->returnvalue_lock_, "Thread::join_cond" } , my_pages_lock_{"thread::my_pages_lock_"},
  exec_wait_{&process->waiting_exec_lock_, "Thread::exec_wait_"}// UserThread's members
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
  last_start_ = Scheduler::instance()->getRDTSC();
  Scheduler::instance()->addNewThread((Thread*)this);

  //should be threadsafe??
  switch_to_userspace_ = 1;
}

void UserThread::reDirectToDeath(){
    switch_to_userspace_ = 0;
    ArchThreads::changeInstructionPointer(kernel_registers_, (void*) Syscall::pthread_exit);
    return;
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
            getParentProcess()->incDuaration(Scheduler::instance()->getRDTSC() - getLastStart());
            setLastStart(Scheduler::instance()->getRDTSC());
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
    
    
    size_t sleepy = __atomic_exchange_n(mystack_.UserMutex, AWAKE_KS, ustl::memory_order_seq_cst);
    //debug(X_THREADSTACK, "Tid[%ld] sleepy = %ld\n", getTID(), sleepy);

    if(sleepy == SLEEPING_KS)
    {
      //get the right flag back
      __atomic_exchange_n(mystack_.UserMutex, SLEEPING_KS, ustl::memory_order_seq_cst);
      return false;
    }
    else if(getTimeToWake() > (Scheduler::instance()->getRDTSC() * 10))
    {
      return false;
    }
    else if (sleepy == AWAKE_KS)
    {
      getParentProcess()->incDuaration(Scheduler::instance()->getRDTSC() - getLastStart());
      setLastStart(Scheduler::instance()->getRDTSC());
      return true;
    }
    else
    {
      debug(X_USERTHREAD, "thread: [%ld]\n", tid_);
      assert(false && "Sleep flag was neither sleeping nor awake?\n");
    }
    debug(X_THREADSTACK, "schedulable finished!\n");
  }
  else if(state_ == ToBeDestroyed)
  {
    getParentProcess()->incDuaration(Scheduler::instance()->getRDTSC() - getLastStart()); 
  }
  return false;
}


// pthread_create
UserThread::UserThread(size_t wrapper, size_t page_offset, uint32_t terminal_number) :
  Thread(currentUserThread->working_dir_, currentUserThread->name_, 
          USER_THREAD, ProcessRegistry::instance()->createID()),
  process_(currentUserThread->process_), flag_mutex_{"thread::flag_mutex_"},
   join_cond_{&process_->returnvalue_lock_, "Thread::join_cond"},my_pages_lock_{"thread::my_pages_lock_"}
   , exec_wait_{&process_->waiting_exec_lock_, "Thread::exec_wait_"}// UserThread's members
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
  last_start_ = Scheduler::instance()->getRDTSC();
  Scheduler::instance()->addNewThread((Thread*)this);

  switch_to_userspace_ = 1;
}

// fork
UserThread::UserThread(UserProcess *child, UserThread* parent_thread) :
  Thread(child->getWorkingDir(), "fork thread", Thread::USER_THREAD, ProcessRegistry::instance()->createID()),
  process_(child),flag_mutex_{"thread::flag_mutex_"}, join_cond_{&child->returnvalue_lock_,
  "Thread::join_cond"}, my_pages_lock_{"thread::my_pages_lock_"}, exec_wait_{&child->waiting_exec_lock_, "Thread::exec_wait_"}
{
  debug(X_USERTHREAD, "Entering fork constructor for new thread...\n");
  loader_ = child->getLoader();

  //cant we somehow just write a new setupstack... this makes me uncomfortable as we have 3 different constructors where we
  // play around with stacks

  StackInfo parent_stack = parent_thread->getStackInfo();
  mystack_ = parent_stack;

  my_pages_lock_.acquire();
  my_pages_ = parent_thread->my_pages_;
  my_pages_lock_.release();
  //Nedzma said its the users fault :D
  // ArchMemoryMapping map = loader_->arch_memory_.resolveMapping(mystack_.userstack_start_ / PAGE_SIZE);
  // size_t location = (size_t) ArchMemory::getIdentAddressOfPPN(map.page_ppn);
  // location += PAGE_SIZE - sizeof(size_t);
  // mystack_.UserMutex = (size_t*) location;
  // *mystack_.UserMutex = *parent_thread->getStackInfo().UserMutex;

  ArchThreads::createUserRegisters(user_registers_,
                                   (void*) parent_thread->user_registers_->rip,
                                   (void*) mystack_.userstack_start_,
                                   parent_thread->getKernelStackStartPointer());


  memcpy(user_registers_, parent_thread->user_registers_, sizeof(ArchThreadRegisters));
  user_registers_->rax = 0;
  user_registers_->rsp0 = (size_t) getKernelStackStartPointer();

  Userthread = true;
  last_start_ = Scheduler::instance()->getRDTSC();



  ArchThreads::setAddressSpace(this, child->getLoader()->arch_memory_);

  //ArchThreads::printThreadRegisters(this);
  debug(X_USERTHREAD, "Fork constructor successful for new thread...\n");

}

UserThread::~UserThread()
{
  switch_to_userspace_ = 0;
  //debug(X_USERTHREAD, "~UserThread called for thread [%ld] in pid: [%ld] called %s . removing from UserProcess::threads_\n", tid_, process_->getPID(), name_.c_str());
  
  freeMyPages();
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

void UserThread::getNewStackPage(size_t adress){
  my_pages_lock_.acquire();
  process_->lockArchMem();
  size_t new_page = PageManager::instance()->allocPPN();
  if (!loader_->arch_memory_.mapPage((adress / PAGE_SIZE), new_page, 1))
  {
    //might need change in the future
    debug(USERTHREAD, "getnewpage(): RIP. asserting.\n");
    assert(false);
  }
  my_pages_.push_back(adress / PAGE_SIZE);
  process_->unlockArchMem();
  my_pages_lock_.release();
  return;
}

void UserThread::freeMyPages(){
  my_pages_lock_.acquire();
  process_->lockArchMem();
  for (size_t i = 0; i < my_pages_.size(); i++)
  {
    //might delete the assert later
    assert(loader_->arch_memory_.unmapPage(my_pages_[i]) && "couldnt cleanup my own pages?");
  }
  process_->unlockArchMem();
  my_pages_lock_.release();
}



bool UserThread::setupStack()
{
  debug(X_USERTHREAD, "setupStack(): TID[%ld] my offset is: %lx\n", tid_, mystack_.page_offset_);

  // virtual address
  size_t stack_page_offset = mystack_.page_offset_ * PAGE_SIZE  * (STACK_SIZE_IN_PAGES + 2); 
  size_t stack_start_ptr = USER_BREAK - sizeof(size_t) - stack_page_offset;
  size_t frontguard = stack_start_ptr / PAGE_SIZE;
  stack_start_ptr -= PAGE_SIZE;

  size_t stackend = stack_start_ptr - PAGE_SIZE * STACK_SIZE_IN_PAGES;
  size_t endguard = (stackend - PAGE_SIZE) / PAGE_SIZE;
  size_t vpn_for_stack = stack_start_ptr / PAGE_SIZE; 
  // ppn
  size_t ppn_for_stack = PageManager::instance()->allocPPN();
  // check stack vpn and ppn + mapPage()
  assert(vpn_for_stack && ppn_for_stack);

  process_->lockArchMem();
  if(!loader_->arch_memory_.mapPage(vpn_for_stack, ppn_for_stack, 1))
  {
    debug(USERTHREAD, "setupStack(): RIP. asserting.\n");
    assert(false);
    PageManager::instance()->freePPN(ppn_for_stack);
    return false;
  }
  debug(X_USERTHREAD, "setupStack(): mapPage(vpn_for_stack = %lx, ppn_for_stack = %lx)\n", vpn_for_stack, ppn_for_stack);
  process_->unlockArchMem();

  my_pages_lock_.acquire();
  my_pages_.push_back(vpn_for_stack);
  my_pages_lock_.release();

  mystack_.userstack_start_ = stack_start_ptr - 2 * sizeof(size_t);
  size_t location = (size_t) ArchMemory::getIdentAddressOfPPN(ppn_for_stack);
  location += PAGE_SIZE - sizeof(size_t);
  mystack_.UserMutex = (size_t*) location;
  *mystack_.UserMutex = AWAKE_KS;

  size_t* userlock_wait = (size_t*)(location - sizeof(size_t));
  *userlock_wait = NO_LOCK_KS;
  mystack_.guardpage_front_nr_ = frontguard;
  mystack_.userstack_end_ = stackend;
  mystack_.guardpage_back_nr_ = endguard;
  
  debug(USERTHREAD, "[%ld]: my stack starts at: %lx (VPN %ld) and ends at %lx (VPN %ld) flag is at %lx"
  "and guardpages are at %ld and %ld\n",tid_, mystack_.userstack_start_, (mystack_.userstack_start_ / PAGE_SIZE),
  mystack_.userstack_end_, (mystack_.userstack_end_ / PAGE_SIZE),stack_start_ptr, mystack_.guardpage_front_nr_, 
  mystack_.guardpage_back_nr_);


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
  debug(X_USERTHREAD, "execv(): %s. argc = %ld\n", name_.c_str(), argc);

  //MEMLEAK
  char* here[argc];
  for (size_t i = 0; i < argc; i++)
  {
    here[i] = new char[strlen(argv[i]) + 1];
    memcpy(here[i], argv[i], strlen(argv[i]));
    here[i][strlen(argv[i])] = '\0';
    debug(X_USERTHREAD, "execv(): memcpy(): copying %s from %lx to here[%ld] which lies at %lx\n", argv[i], (size_t)(argv + i), i, (size_t)(here + i));
  }
  debug(X_USERTHREAD, "execv(): copied from old archmem into char* here[] finished\n");
    
  // set new archmemory to thread
  loader_ = process_->getLoader();
  ArchThreads::setAddressSpace(this, loader_->arch_memory_);
  debug(X_USERTHREAD, "execv(): set new archmemory\n");
  
  // setup copy
  size_t ppn = PageManager::instance()->allocPPN();
  size_t vpn = (USER_BREAK - 8)/ PAGE_SIZE;
  process_->lockArchMem();
  assert(loader_->arch_memory_.mapPage(vpn, ppn, 1));
  process_->unlockArchMem();

  size_t new_argv = USER_BREAK - PAGE_SIZE;
  char** argv_arr = (char**) new_argv;
  size_t str_offset = argc * sizeof(char*);
  // copy from here[] into fresh archmem
  for(size_t i = 0; i < argc; i++)
  {
    size_t str_len = strlen(here[i]) + 1;
    if(str_offset + str_len >= PAGE_SIZE)
      return -1;
    
    memcpy((void*)(new_argv + str_offset), (void*) here[i], str_len);
    debug(X_USERTHREAD, "execv(): memcpy(): copying %s from here[%ld] to (argv_arr + str_offset) = %lx\n", here[i], (size_t)(here + i), (size_t)(new_argv + str_offset));
    
    argv_arr[i] = (char*)(new_argv + str_offset);
    debug(X_USERTHREAD, "execv(): memcpy() memcpy(argv_arr[%lx] = (%lx)\n", (size_t)argv_arr[i], new_argv + str_offset);

    str_offset += str_len;
  }

  for (size_t i = 0; i < argc; i++)
  {
    delete[] here[i];
  }
  

  debug(X_USERTHREAD, "execv(): after for-loop.\n");


  for (size_t i = 0; i < argc; i++)
  {
    debug(X_USERTHREAD, "execv(): argv_arr[%ld]: %s\n", i, argv_arr[i]);
  }
  debug(X_USERTHREAD, "execv(): copied from here[] to new location\n");

  // setup stack and set registers
  mystack_.page_offset_ = process_->getRandomPageOffset();
  setupStack();
  user_registers_->rsp = (size_t)getUserstackStart();
  user_registers_->rip = (size_t)loader_->getEntryFunction();
  user_registers_->rdi = argc; 
  user_registers_->rsi = new_argv; 
  debug(X_USERTHREAD, "execv(): rsp = %lx, rip = %lx, rdi = %ld, rsi = %lx\n", user_registers_->rsp, user_registers_->rip, user_registers_->rdi, user_registers_->rsi);
  return 0;
}

bool UserThread::detectCircularJoin(UserThread* to_be_joined){
  if (join_waiter_ == 0)
  {
    return false;
  }
  else if (join_waiter_ == to_be_joined)
  {
    return true;
  }
  else
  {
    UserThread* iter = join_waiter_;
    debug(X_USERTHREAD, "joincheck for [%ld] iter: %p\n", iter->getTID(), iter);
    while (iter != 0)
    {
      if (iter == to_be_joined)
        return true;
      else
      {
        debug(X_USERTHREAD, "next on list is: %p\n", iter->join_waiter_);
        iter = iter->join_waiter_;
      }
    }
  }
  return false;
}



int UserThread::execv()
{
  // important: after setAddressSpace the cr3 register of the thread is updated to the new archmemory
  name_ = process_->getName();
  loader_ = process_->getLoader();
  ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(),
                                   (void*) mystack_.userstack_start_,
                                   getKernelStackStartPointer());
  ArchThreads::setAddressSpace(this, loader_->arch_memory_);
  mystack_.page_offset_ = process_->getRandomPageOffset();
  setupStack();
  debug(X_USERTHREAD, "execv(): set name_ = %s, loader_ = %lx, setAddressSpace(), mystack_.page_offset_ = %lx\n", name_.c_str(), (size_t)loader_, mystack_.page_offset_);

  // passing new virtual memory to userspace
  user_registers_->rsp = (size_t)getUserstackStart();
  user_registers_->rip = (size_t)loader_->getEntryFunction();
  // user_registers_->rdi = (size_t)argv;
  // user_registers_->rsi = argc;
  debug(X_USERTHREAD, "execv(): rip = %lx, rdi = %lx, rsi = %lx\n", user_registers_->rip, user_registers_->rdi, user_registers_->rsi);
  return 0;
}
