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
  //dont switch to userspace during creation

  size_t offset = 1;
  debug(X_USERTHREAD, "TID [%ld]: first thread constructor called.\n", getTID());
  loader_ = parent_process_->getLoader();
  // gets thread's stack a ppn and a mapped vpn
  ppns_for_userstack_[0] = PageManager::instance()->allocPPN();
  vpns_for_userstack_[0] = USER_BREAK / PAGE_SIZE - offset; // for different VPNs mabye subtract getTID(); !bigger stacks might break the tid method to get page
  loader_->arch_memory_.resolveMapping(vpns_for_userstack_[0]);
  bool vpn_mapped = loader_->arch_memory_.mapPage(vpns_for_userstack_[0], ppns_for_userstack_[0], 1);
  
  //set stack start and stack end in Adressspace and set to Stack Canary
  userstack_start_ = (size_t*) (USER_BREAK - (offset - 1) * PAGE_SIZE - sizeof(size_t*));
  userstack_end_ = (size_t*) (USER_BREAK - offset * PAGE_SIZE - sizeof(size_t*)); //currently 1 as our stack is ownly 1 Page!


  assert(vpn_mapped && "Virtual page for stack was already mapped - this should never happen");
  debug(X_USERTHREAD, "TID [%ld]: PPN (%ld) for stack set and mapped to VPN (%ld)\n", getTID(), ppns_for_userstack_[0], vpns_for_userstack_[0]);

  debug(X_USERTHREAD, "STACK first:this woud be giving stack adress of: %p", (void*) userstack_start_);
  //set to custom userstack start as created befroe... somehow makes no sense 
  ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(),
                                   (void*) userstack_start_,
                                   getKernelStackStartPointer());
  
  void* stackstart = (void*)(USER_BREAK - sizeof(pointer));
  debug(X_USERTHREAD, "STACK second: this would be giving stack adress of: %p", stackstart);
  //this was this funct before
  // ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(),
  //                                  (void*) (USER_BREAK - sizeof(pointer)),
  //                                  getKernelStackStartPointer());

  ArchThreads::setAddressSpace(this, loader_->arch_memory_);
  debug(X_USERTHREAD, "TID [%ld]: Registers and AddressSpace set.\n", getTID());

  // set terminal
  if (main_console->getTerminal(terminal_number))
    setTerminal(main_console->getTerminal(terminal_number));

  // add Thread to Scheduler and to process' list_of_threads_
  Scheduler::instance()->addNewThread((Thread*)this);
  parent_process_->addToThreadList(this);
  /*
  list_of_threads_lock_.acquire();
  list_of_threads_.insert(ustl::make_pair(first_thread->getTID(), first_thread));
  list_of_threads_lock_.release();*/
  *(userstack_start_ + sizeof(size_t*)) = STACK_CANARY;
  *userstack_end_ = STACK_CANARY;

  debug(X_USERTHREAD, "TID [%ld]: first thread constructor finished\n", getTID());
  switch_to_userspace_ = 1;
}

UserThread::~UserThread()
{
  switch_to_userspace_ = 0;
  //race conditionnn! because when exit is called during pthread exit page get freed 2 times
  debug(X_USERTHREAD, "~UserThread called.\n");
  //can be used if we find our Userstack :S
  //if (!isUserStackCanaryOK())
  //{
    //debug(USER_THREAD, "Stack got corrupted!\n");
    //kill progamm then 
  //}
  //free Stack Pages
  for (size_t i = 0; i < USERSTACK_SIZE; i++)
  {
    debug(X_USERTHREAD, "freeing Stack pages for thread [%ld]\n", tid_);
    PageManager::instance()->freePPN(ppns_for_userstack_[i]);
    if(!loader_->arch_memory_.unmapPage(vpns_for_userstack_[i]))
      debug(X_USERTHREAD, "coul not free vpn :?\n");
  }

  //delete Thread from Process and sheduler
  // calls delete for process if the thread is the last thread
  parent_process_->removeFromThreadList(this);
  if(isLast())
  {
    debug(X_USERTHREAD, "Last Thread with TID [%ld] from process [%ld]\n", getTID(), parent_process_->getPID());
    delete parent_process_;
  }
  this->kill();
  switch_to_userspace_ = 1;
}

bool UserThread::isUserStackCanaryOK(){
  return (*userstack_start_ == STACK_CANARY && *userstack_end_ == STACK_CANARY);
}