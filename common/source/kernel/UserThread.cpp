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
  assert(vpn_for_stack && ppn_for_stack);
  if(!loader_->arch_memory_.mapPage(vpn_for_stack, ppn_for_stack, 1))
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
  debug(X_USERTHREAD, "execv(): dear god, allmighty\n"
                      "execv(): please recognise my praying\n"  
                      "execv(): just let this code work\n");

  debug(X_USERTHREAD, "execv(): argc = %ld\n", argc);
  for(size_t i = 0; i < argc; i++)
    debug(X_USERTHREAD, "execv(): argv[%ld] = %s\n", i, argv[i]? argv[i]: "NULL");
  
  /** CURRENT PLAN:
   *  1 create new page for the arguments
   *  2 getIdentAddress()
   *  3 argv will lie direcly below USER_BREAK
   *  4 mapPage(ppn) with ARCHMEM FROM PROCESS
   *  5 set length of pointer array + create pointer array (char**)
   *  6 first iteration
   *    6.1 set length of string
   *    6.2 check if length of pointer + length of string is below PAGE_SIZE
   *    6.3 copy the address AFTER the pointer array (pointer array addr + length of pointer array) into first element of pointer array
   *    6.4 copy string from argv to first address after pointer array
   *    6.5 repeat 6 with with i++ and offset += length of string until i = argc
   */ 

  size_t new_argc = argc - 1;
  char* here[argc];
  for (size_t i = 0; i < new_argc; i++)
  {
    here[i] = new char[strlen(argv[i]) + 1];
    memcpy(here[i], argv[i], strlen(argv[i]));
    here[i][strlen(argv[i])] = '\0';
  }
  for (size_t i = 0; i < new_argc; i++)
  {
    debug(X_USERTHREAD, "here: %s\n", here[i]);
  }
  
  
  
  name_ = process_->getName();
  loader_ = process_->getLoader();
  ArchThreads::setAddressSpace(this, loader_->arch_memory_);


  //debug(X_USERTHREAD, "execv(): set name_ = %s, loader_ = %lx, setAddressSpace(), mystack_.page_offset_ = %lx\n", name_.c_str(), (size_t)loader_, mystack_.page_offset_);

  // passing new virtual memory to userspace 
 
  
  debug(X_USERTHREAD, "execv(): about to set up stuff for argument copying\n");
  // don't count NULL! otherwise pagefault at 0x0
  size_t ppn = PageManager::instance()->allocPPN();
  // char* ident_start[] will hold the pointers to the char strings. the page will also hold the strings
  // the page in the virtual memory via which the user will be able to access the pointers and the strings that they point to
  size_t vpn = (USER_BREAK - 8)/ PAGE_SIZE;
  // mag earlier
  assert(loader_->arch_memory_.mapPage(vpn, ppn, 1));
  //char** ident_start = (char**)ArchMemory::getIdentAddressOfPPN(ppn);
  //debug(X_USERTHREAD, "execv(): ppn = %lx, ident_start = %lx, vpn = %lx\n", ppn, (size_t)ident_start, vpn);
  
  // the start of the page aka the tstart of the pointer array.
  size_t new_argv = USER_BREAK - PAGE_SIZE;
  char** argv_arr = (char**) new_argv;
  
  const char* string = "pups:D";
  memcpy((void*)argv_arr, (void*) string, strlen(string));
  debug(X_USERTHREAD, "%s is what i found at mapped_start [%p]\n", (char*) argv_arr, (char*) argv_arr);
  
  // the offset for the strings. first set directly behind the char* ident_start[] pointers. will be increased by str_len per for-loop-iterations
  size_t str_offset = argc * sizeof(char*);
  debug(X_USERTHREAD, "execv(): before for-loop: new_argv = %p, str_offset = %ld\n\n", argv_arr, str_offset);
  for(size_t i = 0; i < new_argc; i++)
  {
    debug(X_USERTHREAD, "execv(): loop start: %ld from %ld args copied\n", i, new_argc);
    // + 1 for null termination
    size_t str_len = strlen(here[i]) + 1;
    // if str_offset + str_len gets larger than a PAGE_SIZE, we overshoot the page obviously
    if(str_offset + str_len >= PAGE_SIZE)
      return -1;
    
    // copy string from userspace location to string array after pointer array. also copies null-termination
  //  debug(X_USERTHREAD, "execv(): memcpy(ident_start + str_offset = %lx, argv[i] = %s, str_len = %ld)\n", (size_t)(ident_start + str_offset), argv[i], str_len);
    debug(X_USERTHREAD, "before memcp: will copy to location %p\n", (void*)(new_argv + str_offset));
    memcpy((void*)(new_argv + str_offset), (void*) here[i], str_len);
    //*((char*)(new_argv + str_offset + str_len)) = '\0';

    // copy the address of the current string location to char* ident_start[i]
   // debug(X_USERTHREAD, "execv(): element %ld at %lx: points to string at (new_argv + str_offset) = %lx\n", i, (size_t)(ident_start + i), new_argv + str_offset);
    *(argv_arr + i) = (char*)(new_argv + str_offset);
    // check if copied successfully

    debug(X_USERTHREAD, "execv(): found string %s at %p\n", argv_arr[i], argv_arr[i]);
    // increase string offset by str_len
    str_offset += str_len;
    debug(X_USERTHREAD, "execv(): loop end %ld from %ld args copied\n\n", i + 1, new_argc);
  }

  debug(X_USERTHREAD, "execv(): after for-loop.\n");
  for (size_t i = 0; i < argc; i++)
  {
    debug(X_USERTHREAD, "just to check %ld: %s\n", i, argv_arr[i]);
  }

  mystack_.page_offset_ = process_->getRandomPageOffset();
  setupStack();
  user_registers_->rsp = (size_t)getUserstackStart();
  user_registers_->rip = (size_t)loader_->getEntryFunction();
  user_registers_->rdi = new_argc; 
  user_registers_->rsi = (size_t)argv_arr; 
  
  debug(X_USERTHREAD, "execv(): rsp = %lx, rip = %lx, rdi = %ld, rsi = %lx\n", user_registers_->rsp, user_registers_->rip, user_registers_->rdi, user_registers_->rsi);
  return 0;
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
  debug(X_USERTHREAD, "execv(): rip = %lx, rdi = %lx, rsi = %lx\n", user_registers_->rip, user_registers_->rdi, user_registers_->rsi);
  return 0;
}
