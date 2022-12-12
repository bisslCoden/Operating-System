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
UserThread::UserThread( UserProcess* process, 
                        FileSystemInfo* working_dir, 
                        ustl::string name, 
                        uint32 terminal_number, 
                        size_t* returnto, ustl::queue<size_t>* ppns) :
  Thread( working_dir, name, USER_THREAD, 
          ProcessRegistry::instance()->createID()), 
  process_(process),
  flag_mutex_{"thread::flag_mutex_"},
  join_cond_{ &process_->returnvalue_lock_, "Thread::join_cond" }, 
  my_pages_lock_{"thread::my_pages_lock_"},
  exec_wait_{&process->waiting_exec_lock_, "Thread::exec_wait_"}
{
  debug(X_USERTHREAD, "UserThread() for createProcess() started for TID [%ld]\n", getTID());

  // loader and stack setup
  loader_ = process_->getLoader();
  mystack_.page_offset_ = process_->getRandomPageOffset();
  setupStack(ppns);

  // check if process already got killed
  process_->lockThreadMutex();
  if (process_-> KILLED_)
  {
    process_->unLockThreadMutex();
    *returnto = 9;
    return;
  }

  // user_registers_ + set AddressSpace
  ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(),
                                   (void*) mystack_.userstack_start_,
                                   getKernelStackStartPointer());
  ArchThreads::setAddressSpace(this, loader_->arch_memory_);
  debug(X_USERTHREAD, "TID: [%ld], cr3: %lx, rsp: %lx, rip: %lx\n",
    getTID(), user_registers_->cr3, user_registers_->rsp, user_registers_->rip);

  // set terminal (not very effective at the moment)
  if (main_console->getTerminal(terminal_number))
    setTerminal(main_console->getTerminal(terminal_number));

  // add Thread to process to scheduler and set starttime for clock
  last_start_ = Scheduler::instance()->getRDTSC();
  //process_->addToThreadList(this);
  //Scheduler::instance()->addNewThread((Thread*)this);

  //should be threadsafe??
  *returnto = 0;
  switch_to_userspace_ = 1;
  process_->unLockThreadMutex();
  debug(X_USERTHREAD, "UserThread() for createProcess() finished for TID [%ld]\n", getTID());
  return;
}


// pthread_create
UserThread::UserThread( size_t wrapper, 
                        size_t* returnto, ustl::queue<size_t>* ppns,
                        uint32_t terminal_number) :
  Thread( currentUserThread->working_dir_, currentUserThread->name_, USER_THREAD, 
          ProcessRegistry::instance()->createID()),
  process_(currentUserThread->process_), 
  flag_mutex_{"thread::flag_mutex_"},
  join_cond_{&process_->returnvalue_lock_, "Thread::join_cond"},
  my_pages_lock_{"thread::my_pages_lock_"},
  exec_wait_{&process_->waiting_exec_lock_, "Thread::exec_wait_"}
{
  debug(X_USERTHREAD, "UserThread() for pthread_create() started for TID [%ld]\n", getTID());

  // loader and stack setup
  loader_ = process_->getLoader();
  mystack_.page_offset_ = getProcess()->getRandomPageOffset();
  setupStack(ppns);

  // check if process already got killed
  process_->lockThreadMutex();
  if (process_-> KILLED_)
  {
    process_->unLockThreadMutex();
    *returnto = 9;
    return;
  }

  // user_registers_ + setAddressSpace
  ArchThreads::createUserRegisters(user_registers_, (void*)wrapper,
                                   (void*) mystack_.userstack_start_, 
                                   getKernelStackStartPointer());
  ArchThreads::setAddressSpace(this, loader_->arch_memory_);;
  debug(X_USERTHREAD, "TID: [%ld], cr3: %lx, rsp: %lx (stackstart %lx), rip: %lx\n",
    getTID(), user_registers_->cr3, user_registers_->rsp, mystack_.userstack_start_, user_registers_->rip);
  
  // set terminal (not very effective at the moment)
  if (main_console->getTerminal(terminal_number))
    setTerminal(main_console->getTerminal(terminal_number));

  // add Thread to process and scheduler
  last_start_ = Scheduler::instance()->getRDTSC();
  //process_->addToThreadList(this);
  //Scheduler::instance()->addNewThread((Thread*)this);

  switch_to_userspace_ = 1;
  *returnto = 0;
  process_->unLockThreadMutex();
  debug(X_USERTHREAD, "UserThread() for pthread_create() finished for TID [%ld]\n", getTID());
  return;
}


// fork
UserThread::UserThread( UserProcess *child, 
                        UserThread* parent_thread, 
                        size_t* returnto) :
  Thread( child->getWorkingDir(), "fork thread", USER_THREAD, 
          ProcessRegistry::instance()->createID()),
  process_(child),flag_mutex_{"thread::flag_mutex_"}, 
  join_cond_{&child->returnvalue_lock_, "Thread::join_cond"}, 
  my_pages_lock_{"thread::my_pages_lock_"}, 
  exec_wait_{&child->waiting_exec_lock_, "Thread::exec_wait_"}
{
  debug(X_USERTHREAD, "UserThread() for fork() started for TID [%ld]\n", getTID());
  loader_ = child->getLoader();

  // hannes' fun
  mystack_.guardpage_back_nr_ = parent_thread->mystack_.guardpage_back_nr_;
  size_t* UserMut = parent_thread->mystack_.UserMutex;
  mystack_.UserMutex = UserMut; 
  mystack_.page_offset_ = parent_thread->mystack_.page_offset_;
  mystack_.guardpage_front_nr_ = parent_thread->mystack_.guardpage_front_nr_;
  mystack_.userstack_end_ = parent_thread->mystack_.userstack_end_;
  mystack_.userstack_start_ = parent_thread->mystack_.userstack_start_;

  my_pages_lock_.acquire();
  my_pages_ = parent_thread->my_pages_;
  my_pages_lock_.release();

  // user_registers_ + setAddressSpace
  ArchThreads::createUserRegisters(user_registers_,
                                   (void*) parent_thread->user_registers_->rip,
                                   (void*) mystack_.userstack_start_,
                                   parent_thread->getKernelStackStartPointer());
  memcpy(user_registers_, parent_thread->user_registers_, sizeof(ArchThreadRegisters));
  user_registers_->rax = 0;
  user_registers_->rsp0 = (size_t) getKernelStackStartPointer();
  ArchThreads::setAddressSpace(this, child->getLoader()->arch_memory_);

  // add Thread to process and scheduler
  last_start_ = Scheduler::instance()->getRDTSC();
 // Scheduler::instance()->addNewThread((Thread*)this);

  *returnto = 0;
  debug(X_USERTHREAD, "UserThread() for fork() finished for TID [%ld]\n", getTID());
  return;
}


UserThread::~UserThread()
{
  switch_to_userspace_ = 0;
  debug(X_USERTHREAD, "[%ld] freed my pages now...\n",tid_);
  if(isLast())
  {
    debug(X_USERTHREAD, "Last Thread with TID [%ld] from process [%ld]. Deleting process_\n", getTID(), process_->getPID());
    delete process_;
  }
}


void UserThread::reDirectToDeath()
{
    switch_to_userspace_ = 0;
    ArchThreads::changeInstructionPointer(kernel_registers_, (void*) Syscall::pthread_exit);
    return;
}

//474387766000
//150854471140

bool UserThread::schedulable()
{
  if (getState() == Running)
  {
    //testsystem
    //checks if exit is called
    //debug(X_THREADSTACK, "schedulable called for thread %ld by thread %ld!\n", getTID(), currentThread->getTID());ö-
    if (!myflags_.knotcancelable)
      if (myflags_.kasynchronous)
        if (myflags_.kcancelreq)
        {
          was_scheduled_ = 1;
          setLastStart(Scheduler::instance()->getRDTSC());
          return true;
        }

    if(DYING_)
    {
      was_scheduled_ = 1;
      setLastStart(Scheduler::instance()->getRDTSC());
      return true;
    }
    else if(getTimeToWake() > (Scheduler::instance()->getRDTSC() * 10))
    {
      // my_pages_lock_.release();
      debug(SLEEP, "[%ld] I should still sleep: %ld, %ld\n", tid_, getTimeToWake(), Scheduler::instance()->getRDTSC() * 10);
      was_scheduled_ = 0;
      return false;
    }
    return true;
  }
  return false;
}
// F*ck the userspace mutexes now ... they bring no points and only lead to problems...
//     //debug(X_THREADSTACK, "Tid[%ld] sleepy = %ld\n", getTID(), sleepy);
//     if (mystack_.UserMutex == USERMUTEX_INVALID)
//     {

//       was_scheduled_ = 1;
//       setLastStart(Scheduler::instance()->getRDTSC());
//       return true;
//     }

//     size_t sleepy = *mystack_.UserMutex;
//     if(sleepy == SLEEPING_KS)
//     {
//       //get the right flag back
//       // __atomic_exchange_n(mystack_.UserMutex, SLEEPING_KS, ustl::memory_order_seq_cst);
//       // my_pages_lock_.release();
//       was_scheduled_ = 0;
//       debug(X_USERTHREAD, "[%ld] sleeping in US\n", tid_);
//       return false;
//     }
//     else if (sleepy == AWAKE_KS)
//     {
//       // my_pages_lock_.release();
//       was_scheduled_ = 1;
//       setLastStart(Scheduler::instance()->getRDTSC());
//       debug(X_USERTHREAD, "[%ld] awake in US\n", tid_);
//       return true;
//     }
//     else
//     {
//       // my_pages_lock_.release();
//       debug(X_USERTHREAD, "thread: [%ld] sleepy : %ld\n", tid_, sleepy);
//       assert(false && "Sleep flag was neither sleeping nor awake?\n");
//     }
// //    debug(X_THREADSTACK, "schedulable finished!\n");
//   }
//   was_scheduled_ = 0;
//}


void UserThread::setCancelState(int state)
{
  myflags_.cancelable = state;
  if(state == PTHREAD_CANCEL_DISABLE)
    myflags_.knotcancelable = true;
  return;
}


void UserThread::setCancelType(int type) 
{
  myflags_.deferred = type;
  if (type == PTHREAD_CANCEL_ASYNCHRONOUS)
    myflags_.kasynchronous = true;
  return;
}


void UserThread::sendCancelRequest()
{
  myflags_.cancelreq = true;
  myflags_.kcancelreq = true;
  return;
}


void UserThread::getNewStackPage(size_t adress, ustl::queue<size_t>* ppns)
{
  if(process_->checkKill())
    return;
  my_pages_lock_.acquire();
  //size_t new_page = PageManager::instance()->allocPPN();
  //debug(X_USERTHREAD, "[%ld] got my page: %lx\n", tid_, new_page);
  if (!loader_->arch_memory_.mapPage((adress / PAGE_SIZE), ppns, 1))
  {
    //might need change in the future
    debug(USERTHREAD, "getnewpage(): RIP. asserting.\n");
    assert(false);
  }
  my_pages_.push_back(adress / PAGE_SIZE);
  my_pages_lock_.release();
  return;
}


void UserThread::freeMyPagesAndDie(bool actually_die)
{
  // kill now if KILLED_ is already true
  if (process_->checkKill() && actually_die)
    this->kill();
  
  ustl::queue<size_t> ppns;
  PageManager::instance()->allocPagesAndAddQueue(5, &ppns);
  // 
  DYING_ = true;
  my_pages_lock_.acquire();
  InvertedPageTable::instance()->lockIPT();
  loader_->arch_memory_.lockArchMemory();
  for (size_t i = 0; i < my_pages_.size(); i++)
  {
    debug(X_USERTHREAD, "[%ld] tried to free a page!\n", tid_);
    assert(loader_->arch_memory_.unmapPage(my_pages_[i], &ppns) && "couldnt cleanup my own pages?");
  }
  InvertedPageTable::instance()->unlockIPT();
  loader_->arch_memory_.unlockArchMemory();
  my_pages_lock_.release();

  PageManager::instance()->freeRestOfPages(&ppns);

  // kill now
  if(actually_die)
    this->kill();
  else
    DYING_ = false;
}


bool UserThread::setupStack(ustl::queue<size_t>* ppns)
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
 
  //size_t ppn_for_stack = PageManager::instance()->allocPPN();
  assert(vpn_for_stack && !ppns->empty());
  size_t ppn_for_stack = ppns->front();
  if(!loader_->arch_memory_.mapPage(vpn_for_stack, ppns, 1))
  {
    debug(USERTHREAD, "setupStack(): RIP. asserting.\n");
    assert(false);
    //PageManager::instance()->freePPN(ppn_for_stack);
    return false;
  }
  //debug(X_USERTHREAD, "setupStack(): mapPage(vpn_for_stack = %lx, ppn_for_stack = %lx)\n", vpn_for_stack, pp);

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
  
  debug(USERTHREAD, "[%ld]: my stack starts at: %lx (VPN %lx) and ends at %lx (VPN %lx) flag is at %lx"
    "and guardpages are at %lx and %lx\n",tid_, mystack_.userstack_start_, (mystack_.userstack_start_ / PAGE_SIZE),
    mystack_.userstack_end_, (mystack_.userstack_end_ / PAGE_SIZE),stack_start_ptr, mystack_.guardpage_front_nr_, 
    mystack_.guardpage_back_nr_);


  return true;
}

bool UserThread::reuseStack(StackInfo* old_stackinfo, ustl::queue<size_t>* ppns)
{
  // size_t ppn = PageManager::instance()->allocPPN();
  debug(X_USERTHREAD, "want to map my old stackpage startin at %lx (vpn %ld)\n", old_stackinfo->userstack_start_, old_stackinfo->userstack_start_ / PAGE_SIZE);
  size_t ppn = ppns->front();
  if(!loader_->arch_memory_.mapPage(old_stackinfo->userstack_start_ / PAGE_SIZE, ppns, 1))
    return false;
  
  my_pages_lock_.acquire();
  my_pages_.clear();
  my_pages_.push_back(old_stackinfo->userstack_start_ / PAGE_SIZE);
  my_pages_lock_.release();

  process_->offsetlist_lock_.acquire();
  process_->offsets_.push_back(mystack_.page_offset_);
  process_->offsetlist_lock_.release();

 // mystack_.UserMutex = USERMUTEX_INVALID;
  mystack_.guardpage_back_nr_ = old_stackinfo->guardpage_back_nr_;
  mystack_.guardpage_front_nr_ = old_stackinfo->guardpage_front_nr_;
  mystack_.page_offset_ = old_stackinfo->page_offset_;
  mystack_.userstack_start_ = old_stackinfo->userstack_start_;
  mystack_.userstack_end_ = old_stackinfo->userstack_end_;

  size_t location = (size_t) ArchMemory::getIdentAddressOfPPN(ppn);
  location += PAGE_SIZE - sizeof(size_t);
  mystack_.UserMutex = (size_t*) location;
  *mystack_.UserMutex = AWAKE_KS;
  size_t* userlock_wait = (size_t*)(location - sizeof(size_t));
  *userlock_wait = NO_LOCK_KS;
  return true;
}


int UserThread::execv(char* const argv[], size_t argc, ustl::queue<size_t>* ppns)
{
  
  if(!((argv && argc) || (!argv && !argc)))
    return -1;
  
  name_ = process_->getName();

  // debugs
  debug(X_USERTHREAD, "execv(argv = %lx, argc = %ld): name_ = %s\n", (size_t)argv, argc, name_.c_str());
  for(size_t i = 0; i < argc; i++)
    debug(X_USERTHREAD, "argv[%ld] = %s\n", i, argv[i]);

  // set new archmemory to thread
  loader_ = process_->getLoader();
  ArchThreads::setAddressSpace(this, loader_->arch_memory_);
  debug(X_USERTHREAD, "execv(): set new archmemory\n");

  // args: copy from argv[] (kernel_argv) into fresh archmem ONLY IF NECESSARY
  if(argc)
  {
    //size_t ppn = ppn;
    size_t vpn = (USER_BREAK - 8)/ PAGE_SIZE; // the virtual page we use to pass the args
    assert(loader_->arch_memory_.mapPage(vpn, ppns, 1) && "why tf was this mapped? This is exec place");
    size_t new_argv = USER_BREAK - PAGE_SIZE; // the virtual address
    char** argv_arr = (char**) new_argv; // the virtual address casted
    size_t str_offset = argc * sizeof(char*) + sizeof(size_t); // + sizeof(size_t) for null termination
    for(size_t i = 0; i < argc; i++)
    {
      size_t str_len = strlen(argv[i]) + 1;
      if(str_offset + str_len >= PAGE_SIZE)
        return -1;

      memcpy((void*)(new_argv + str_offset), (void*) argv[i], str_len);
      debug(X_USERTHREAD, "execv(): memcpy(): copying %s from argv[%lx] to (argv_arr + str_offset) = %lx\n", argv[i], (size_t)(argv + i), (size_t)(new_argv + str_offset));

      argv_arr[i] = (char*)(new_argv + str_offset);
      debug(X_USERTHREAD, "execv(): memcpy() memcpy(argv_arr[%lx] = (%lx)\n", (size_t)argv_arr[i], new_argv + str_offset);

      str_offset += str_len;
    }
    // args: null termination 
    argv_arr[argc] = NULL;

    // args: free argv which was saved as kernel_argv in kernelspace
    for (size_t i = 0; i < argc; i++)
      delete[] argv[i];

    // args: debugs copy
    debug(X_USERTHREAD, "execv(): after for-loop.\n");
    for (size_t i = 0; i < argc; i++)
      debug(X_USERTHREAD, "execv(): argv_arr[%ld]: %s\n", i, argv_arr[i]);
    debug(X_USERTHREAD, "execv(): copied from argv[] to new location.\n");

    // args: set registers for args
    user_registers_->rdi = argc;
    user_registers_->rsi = new_argv;
  }

  // setup stack 
  debug(X_USERTHREAD, "execv(): reuseStack() + setting user_registers_.\n");
  assert(reuseStack(&mystack_, ppns));
  mystack_.UserMutex = USERMUTEX_INVALID; // why not place this in reuse stack?
  // set registers
  user_registers_->rsp = (size_t) mystack_.userstack_start_;
  user_registers_->rip = (size_t)loader_->getEntryFunction();
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
