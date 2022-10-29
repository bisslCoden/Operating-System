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
  
  // setup stack, UserRegisters and address space
  mystack_.page_offset_ = page_offset;
  setupStack();
  ArchThreads::createUserRegisters(user_registers_, 
                                   loader_->getEntryFunction(),
                                   getUserstackStart(),
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

  debug(X_USERTHREAD, "TID [%ld]: UserThread() for first thread finished\n", getTID());
  switch_to_userspace_ = 1;
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
  process_->addToThreadList(this);

  Scheduler::instance()->addNewThread((Thread*)this);

  debug(X_USERTHREAD, "TID [%ld]: UserThread() for pthread_create finished\n", getTID());
  switch_to_userspace_ = 1;
}

// fork
UserThread::UserThread(UserProcess *child_process, UserThread* parent_thread) :
  Thread(child_process->getWorkingDir(), "fork thread", Thread::USER_THREAD, ProcessRegistry::instance()->createID()),
  process_(child_process),
  flag_mutex_{"thread::flag_mutex_"}, 
  condition_mutex_{"Thread::cond_mutex_"},
  join_cond_{&condition_mutex_, 
  "Thread::join_cond"}
{
  loader_ = child_process->getLoader();

  ArchThreads::createUserRegisters(user_registers_,
                                   (void*) parent_thread->user_registers_->rip,
                                   parent_thread->getUserstackStart(),
                                   parent_thread->getKernelStackStartPointer());

  memcpy(user_registers_, parent_thread->user_registers_, sizeof(ArchThreadRegisters));
  user_registers_->rax = 0;
  user_registers_->rsp0 = (size_t) getKernelStackStartPointer();

  ArchThreads::setAddressSpace(this, process_->getLoader()->arch_memory_);

  switch_to_userspace_ = 1;
  debug(X_USERTHREAD, "TID [%ld]: UserThread() for fork finished\n", getTID());
  ArchThreads::printThreadRegisters(this);
}

UserThread::~UserThread()
{
  switch_to_userspace_ = 0;
  //debug(X_USERTHREAD, "~UserThread called for thread [%ld] in pid: [%ld] called %s . removing from UserProcess::threads_\n", tid_, parent_process_->getPID(), name_.c_str());

  
  if(isLast())
  {
    debug(X_USERTHREAD, "Last Thread with TID [%ld] from process [%ld]. Deleting parent_process_\n", getTID(), process_->getPID());
    delete process_;
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
  debug(X_USERTHREAD, "got my first physical page with number %lx\n", ppn_for_stack);
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

  // success man
  mystack_.userstack_start_ = stack_start_ptr;
  debug(USERTHREAD, "[%ld]: my stack starts at: %lx\n",tid_, mystack_.userstack_start_);
  return true;
}

int UserThread::execv(char* const argv[], size_t argc)
{
  debug(X_USERTHREAD, "argc = %ld\n", argc);
  for(size_t i = 0; i < argc; i++)
    debug(USERTHREAD, "argv[%ld] = %s\n", i, argv[i]);



  /* needed later for argument passing
  size_t vpn = USER_BREAK / PAGE_SIZE - 1; 
  size_t ppn = PageManager::instance()->allocPPN();
  assert(loader_->arch_memory_.mapPage(vpn, ppn, 1));
  size_t ident = ArchMemory::getIdentAddressOfPPN(ppn);
  debug(USERTHREAD, "exec(): vpn = %lx, ppn = %lx, ident = %lx\n", vpn, ppn, ident);

  size_t end_of_page = vpn * PAGE_SIZE;
  size_t argv_addr = end_of_page + sizeof(size_t) * argc;
  size_t argv_ident = ident + sizeof(size_t) * argc;
  //debug(USERTHREAD, "exec(): end_of_page = %lx, argv_addr = %lx, argv_ident = %lx\n", end_of_page, argv_addr, argv_ident);
  for(size_t i = 0; argv[i]; i++)
  {
    debug(X_USERTHREAD, "start of for loop: ident = %lx, argv_addr = %lx, argv_ident = %lx\n",ident, argv_addr, argv_ident);
    debug(X_USERTHREAD, "argv[%ld] = %s\n", i, argv[i]);
    *((size_t*)ident) = argv_addr;
    ident += sizeof(size_t);
    debug(X_USERTHREAD, "ident increased to %lx\n", ident);
    memcpy((void*)argv_ident, (void*)argv[i], strlen(argv[i]));
    debug(X_USERTHREAD, "memcpy(argv_ident = %lx, argv[%ld] = %s, strlen() = %ld)\n", argv_ident, i, argv[i], strlen(argv[i]));
    argv_addr += sizeof(char)*strlen(argv[i]) + 1;
    argv_ident += sizeof(char)*strlen(argv[i]) + 1;
    debug(X_USERTHREAD, "end of for loop: argv_addr = %lx, argv_ident = %lx\n", argv_addr, argv_ident);
    debug(X_USERTHREAD, "---\n");
  }
  */

  // important: after this section, the parameters pointing to userspace become null!! 
  name_ = process_->getName();
  loader_ = process_->getLoader();
  ArchThreads::setAddressSpace(this, loader_->arch_memory_);
  setupStack();
  user_registers_->rip = (size_t)loader_->getEntryFunction();
  //user_registers_->rdi = ; // overwritten
  //user_registers_->rsi = ;
  debug(X_USERTHREAD, "loader set, addressspace set, stack set, name change and rip = %lx. returningd\n", user_registers_->rip);
  return 0;
}
