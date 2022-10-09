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

UserThread::UserThread(UserProcess* parent_process, FileSystemInfo* working_dir, ustl::string name, uint32 terminal_number) : 
  Thread(working_dir, name, USER_THREAD, ProcessRegistry::instance()->createID()), // Thread's constructor
  parent_process_(parent_process) // UserThread's members
{
  debug(X_USERTHREAD, "TID [%ld]: Constructor called.\n", getTID());
  loader_ = parent_process_->getLoader();
  // gets thread's stack a ppn and a mapped vpn
  size_t page_for_stack = PageManager::instance()->allocPPN();
  size_t vpn_for_stack = USER_BREAK / PAGE_SIZE - 1; // for different VPNs mabye subtract getTID(); instead of 1
  bool vpn_mapped = loader_->arch_memory_.mapPage(vpn_for_stack, page_for_stack, 1);
  assert(vpn_mapped && "Virtual page for stack was already mapped - this should never happen");
  debug(X_USERTHREAD, "TID [%ld]: PPN (%ld) for stack set and mapped to VPN (%ld)\n", getTID(), page_for_stack, vpn_for_stack);

  // set up user registers and adressspace
  ArchThreads::createUserRegisters(user_registers_, loader_->getEntryFunction(),
                                   (void*) (USER_BREAK - sizeof(pointer)),
                                   getKernelStackStartPointer());
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

  debug(X_USERTHREAD, "TID [%ld]: constructor finished\n", getTID());
  switch_to_userspace_ = 1;
}

UserThread::~UserThread()
{
  debug(X_USERTHREAD, "~UserThread called.\n");
  
  // calls delete for process if the thread is the last thread
  parent_process_->removeFromThreadList(this);
  if(isLast())
  {
    debug(X_USERTHREAD, "Last Thread with TID [%ld] from process [%ld]\n", getTID(), parent_process_->getPID());
    delete parent_process_;
  }
}