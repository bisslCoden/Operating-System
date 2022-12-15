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
#include "InvertedPageTable.h"
#include "Scheduler.h"

//syscall alle binary pages printen

// standard process creation
UserProcess::UserProcess(ustl::string filename, FileSystemInfo *fs_info, size_t* returnto, uint32 terminal_number) :
    pid_(ProcessRegistry::instance()->createID()), 
    fd_(VfsSyscall::open(filename.c_str(), O_RDONLY)), 
    fs_info_(fs_info),
    working_dir_(fs_info),
    name_(filename),
    threads_lock_("UserProcess::threads_lock_"),
    returnvalue_lock_("UserProcess::retvallock"),
    offsetlist_lock_("UserProcess::offsets"),
    waiting_exec_(0),
    waiting_exec_lock_("UserProcess::waiting_exec_lock_"), 
    waitpid_sem_("Userprocess::waitpid_sem_"),
    PBreak_mutex_("Userprocess::PBreak_mutex_")
{
  *returnto = 5;
  waitpid_sem_.init(0);
  debug(USERPROCESS, "entering constructor of %s\n", name_.c_str());
  debug(USERPROCESS, "fs_info present. pointer in there is: %p\n", fs_info_);
  size_t ppn = PageManager::instance()->allocPPN();
  if(!setupLoader(fd_, ppn))
  {
    *returnto = 1;
    return;
  }
  if (!initPBreak())
  {
    debug(USERPROCESS, "Binary is too large for a heap to fit in.. kill proc\n");
    *returnto = 1;
    return;
  }
  initial_PBreak_ = loader_->getPBreak();
  

  size_t returnto_th = -1;
  debug(X_USERPROCESS, "%s: Loader finished. Loader lies at (%p)\n", name_.c_str(), loader_);
  setChildStatus(0);
  ustl::queue<size_t> ppns;
  PageManager::instance()->allocPagesAndAddQueue(4, &ppns);

  UserThread* first_thread = new UserThread(this, working_dir_, name_.c_str(), terminal_number, &returnto_th, &ppns);

  PageManager::instance()->freeRestOfPages(&ppns);

  //assert(first_thread && "UserThread constructor failed");
  if (returnto_th != 0)
  {
    *returnto = 4;
    return;
  }

  threads_lock_.acquire();
  if (KILLED_)
  {
    *returnto = 5;
    threads_lock_.release();
    return;
  }
  threads_lock_.release();
  *returnto = 0; 
  //ProcessRegistry::instance()->addProcToList(this);
  first_thread_ = first_thread;
  //Scheduler::instance()->addNewThread(first_thread);
  return;
}


// User Process Constructor for fork
UserProcess::UserProcess(UserProcess *parent, size_t* returnto) :
  pid_(ProcessRegistry::instance()->createID()),
  fd_(VfsSyscall::open(parent->name_.c_str(), O_RDONLY)),
  fs_info_(parent->fs_info_),
  //loader_(new Loader(fd_)),
  working_dir_(new FileSystemInfo(*parent->fs_info_)),
  my_terminal_(parent->my_terminal_),
  name_(parent->name_),
  threads_lock_("UserProcess::threads_lock_"),
  returnvalue_lock_("UserProcess::retvallock"), 
  offsetlist_lock_("UserProcess::offsets"),
  waiting_exec_(0),
  waiting_exec_lock_("UserProcess::waiting_exec_lock_"),
  waitpid_sem_("Userprocess::waitpid_sem_"),
  PBreak_mutex_("Userprocess::PBreak_mutex_")
{
  *returnto = 5;
  waitpid_sem_.init(0);
  waiting_exec_lock_.acquire();
  waiting_exec_ = 0;
  waiting_exec_lock_.release();

  debug(X_USERPROCESS, "UserProcess() fork PID = [%ld], parent [%ld]\n", pid_, parent->getPID());
  if(!working_dir_)
  {
    debug(USERPROCESS, "UserProcess() fork: Failed to obtain working directory!\n");
    *returnto = 2;
    return;
  }
  size_t ppn = PageManager::instance()->allocPPN(); 
  if (!setupLoader(fd_, ppn))
  {
    *returnto = 1;
    //PageManager::instance()->freePPN();
    return;
  }
  if (parent->loader_->getPBreak() > END_OF_HEAP || parent->loader_->getPBreak() < parent->loader_->getBSSEnd())
  {
    debug(USERPROCESS, "waaait what?! parent had corrupted heap!!!\n");
    *returnto = 1;
  }
  
  loader_->setPBreak(parent->loader_->getPBreak());
  initial_PBreak_ = parent->initial_PBreak_;

  debug(USERPROCESS, "UserProcess() fork: sucessfully setupLoader()\n");
  
  currentUserThread->loader_->arch_memory_.setCowToArchmemPages(loader_->arch_memory_, this);
  debug(USERPROCESS, "UserProcess() fork: setCowToArchmemPages()\n");

  debug(USERPROCESS, "UserProcess() fork: Creating new Thread for Fork\n");
  UserThread* parent_thread = currentUserThread;
  size_t returnto_th = -1;
  auto thread = new UserThread(this, parent_thread, &returnto_th);
  if(!thread || thread->getTID()==0 || returnto_th != 0)
  {
    debug(USERPROCESS, "UserProcess() fork: Failed to create Thread for Fork!\n");
    delete thread;
    first_thread_ = 0;
    *returnto = 4;
    return;
  }
  offsets_.push_back(currentUserThread->getStackInfo()->page_offset_);
  setChildStatus(1);
 // ProcessRegistry::instance()->processStart();
  
  //?
 // Scheduler::instance()->printThreadList();
 first_thread_ = thread;
  *returnto = 0;
  return;
}


UserProcess::~UserProcess()
{
  InvertedPageTable* IPT = InvertedPageTable::instance();
  
  if (!IPT->checkIPT())
    IPT->lockIPT();
  debug(X_USERPROCESS, "PID [%ld]: destructor called by [%ld]\n", pid_, currentThread->getTID());
  ProcessRegistry::instance()->processExit(this);
  // if(Scheduler::instance()->isCurrentlyCleaningUp())
  //   Scheduler::instance()->yield();
  if (loader_ != 0)
  {
    delete loader_;
  }
  Scheduler::instance()->printThreadList();
  loader_ = 0;

  debug(X_USERPROCESS, "annoying but test if vfs is prob\n");
 
  debug(X_USERPROCESS, "I SHOULD UNLOCK IPT RIGHT FUCKING NOW....\n");
  IPT->unlockIPT();
  if (fd_ > 0)
    VfsSyscall::close(fd_);

  debug(X_USERPROCESS, "vfs isnt prob\n");

  delete working_dir_;
  working_dir_ = 0;
  debug(X_USERPROCESS, "PID [%ld]: destructor done by [%ld]\n", pid_, currentThread->getTID());
}

void UserProcess::lockThreadMutex()                
{ 
  assert(!InvertedPageTable::instance()->checkIPT() && "this is the wrong locking ! never lock IPT firssst");
  threads_lock_.acquire(); 
}


bool UserProcess::addToThreadList(UserThread* thread)
{
  assert(thread && "WHAT?");
  assert(threads_lock_.isHeldBy(currentThread) && "threads_lock_ must be aqcuired before!");

  size_t tid = thread->getTID();
  if(threads_.find(tid) != threads_.end())
    assert(false && "SHIT: addToThreadList() already has thread with tid [%ld] in list");

  threads_.insert(ustl::make_pair(tid, thread));
  debug(X_USERPROCESS, "added TID: [%ld] to UserProcess::threads_\n", tid);

  return true;
}


bool UserProcess::addToRetvalList(size_t tid, void* value)
{
  if(!returnvalue_lock_.isHeldBy(currentUserThread))
    returnvalue_lock_.acquire();

  if (returnvalues_.find(tid) != returnvalues_.end())
  {
    returnvalue_lock_.release();
    debug(USERPROCESS, "how did that thread [%ld] exit twice??\n", tid);
    assert(false);
    return false;
  }

  returnvalues_.insert(ustl::make_pair(tid, value));
  debug(X_USERPROCESS, "Process: %ld : added retval %ld for thread %ld to my returnvalue list\n", pid_, (size_t)value, tid);

  returnvalue_lock_.release();
  return true;
}

//not threadsafe: acquire before
bool UserProcess::removeFromThreadList(UserThread* thread)
{
  // checks if the thread is in list
  size_t tid = thread->getTID();
  if(threads_.find(tid) == threads_.end())
  {
    debug(USERPROCESS, "SHIT: removeFromThreadList() could not find thread with tid [%ld] in list\n", tid);
    //assert(false); // assert or not?
    return false; 
  }

  // sets Thread::last_ if it's last thread
  debug(X_USERPROCESS, "%ld threads left in my process\n", threads_.size());
  // for (size_t i = 0; i < threads_.size(); i++)
  // {
  //   debug(X_USERPROCESS, "  %ld  ", threads_[i]->getTID());
  // }
  // debug(X_USERPROCESS, "\n");
  waiting_exec_lock_.acquire();
  if (threads_.size() == 2 && waiting_exec_ != 0)
  {
    waiting_exec_->signalExec();
    waiting_exec_lock_.release();
  }
  else 
    waiting_exec_lock_.release();

  // about to remove the last thread.. better set a flag that leads to process deletion aswell..
  if(threads_.size() == 1)
    thread->setLast();

  threads_.erase(tid);
  debug(X_USERPROCESS, "removed TID: [%ld] from UserProcess::threads_\n", tid);

  return true;
}

void UserProcess::removeFromOffsetList(size_t NR){
  offsetlist_lock_.acquire();
  size_t* my_offset = 0;
  for (size_t i = 0; i < offsets_.size(); i++)
  {
    if (offsets_[i] == NR)
    {
      my_offset = &offsets_[i];
      break;
    }
  }
  if (my_offset != 0)
  {
    offsets_.erase(my_offset);
  }
  else
  {
    debug(X_USERPROCESS, "tried to erase offset %ld but DID NOT FIND IT WTF!!!\n", NR);
  }
  offsetlist_lock_.release();
  return;
}

//locks threadlock internally!
UserThread* UserProcess::checkStackAdress(size_t address){
  // if (!threads_lock_.isHeldBy(currentThread))
  // {
  //   threads_lock_.acquire();
  // }
  ustl::map<size_t, UserThread*>::iterator it;
  for (it = threads_.begin(); it != threads_.end(); it++)
  {
    if (address <= it->second->getStackInfo()->userstack_start_ && address > it->second->getStackInfo()->userstack_end_)
    {
      //threads_lock_.release();
      return it->second;
    }
  }
  //threads_lock_.release();
  return 0;
}



size_t UserProcess::getRandomPageOffset()
{
  size_t firstbits;
  size_t lastbits;
  size_t page_offset = 0;
  size_t rand;
  offsetlist_lock_.acquire();
  do
  {
    offsetlist_lock_.release();
    asm volatile("rdtsc \n\t" : "=a"(lastbits), "=d"(firstbits));
    rand =  lastbits | firstbits << 32;
    page_offset = rand % (MAX_STACKS);
    offsetlist_lock_.acquire();
  } while (page_offset == 0 || checkInOffsetList(page_offset));
  offsets_.push_back(page_offset);
  offsetlist_lock_.release();
  //debug(X_USERPROCESS, "[%ld] read %ld from tsc and MAX STACKS btw is %lld offset is %lx!!\n", getPID(), rand, MAX_STACKS, page_offset);
  
  //offsetlist_lock_.acquire();
  // for(size_t i = 0; i < offsets_.size(); i++)
  //   debug(X_USERPROCESS, "[%ld] getRandomPageOffset(): UserProcess::offsets_.at(%ld) = %lx\n", getPID(), i, offsets_.at(i));
  //offsetlist_lock_.release();


  return page_offset;
}

bool UserProcess::checkInOffsetList(size_t NR)
{

  for (auto val : offsets_)
  {
    if(val == NR)
      return true;
  }
  return false;
}

Thread* UserProcess::findInThreadList(size_t tid)
{
  assert(testThreadMutex(currentThread) && "PLEASE LOCK BEFORE UserProcess::findInThreadList()");
  if(threads_.find(tid) == threads_.end())
    return (Thread*) 0x00;
  return threads_[tid];
}

size_t UserProcess::getNrOfThreads()
{
  threads_lock_.acquire();
  size_t number = threads_.size();
  threads_lock_.release();
  return number;
}

UserThread* UserProcess::createNewThread(size_t start_routine, size_t args, size_t wrapper, ustl::queue<size_t>* ppns, int32 joinstate = PTHREAD_CREATE_DETACHED)
{
  UserThread* thread = 0;
  size_t return_to = 6;
  threads_lock_.acquire();
  if (KILLED_)
  {
    threads_lock_.release();
    return 0;
  }
  threads_lock_.release();

  thread = new UserThread(wrapper, &return_to, ppns);
  
  threads_lock_.acquire();

  if (return_to != 0 || KILLED_)
  {
    debug(USERPROCESS, "Ups, something went wrong creating the Userthread for proc [%ld] [%ld]... assert!\n", pid_, return_to);
    delete thread;
    threads_lock_.release();
    return 0;
  }
  
  if (joinstate != PTHREAD_CREATE_JOINABLE)
      thread->setJoinState(joinstate);
    
  thread->user_registers_->rdi = start_routine;
  thread->user_registers_->rsi = args;
  
  addToThreadList(thread);
  Scheduler::instance()->addNewThread(thread);
  threads_lock_.release();
  return thread;
}

void UserProcess::exit(size_t exit_code, bool kill_currentThread)
{
  debug(USERPROCESS, "PID: [%ld] exit(exit_code = %ld) called\n", pid_, exit_code);

  if (!threads_lock_.isHeldBy(currentThread))
    threads_lock_.acquire();

  for(auto thread : threads_) // first = tid, second = *Thread
  {
    if(thread.first == currentThread->getTID());
    else
    {
      if (!thread.second->checkFlagLock(currentThread))
        thread.second->lockFlagMutex();
      debug(X_USERTHREAD, "[%ld]: send out a cancel to %ld\n", currentThread->getTID(), thread.first);

      thread.second->setCancelState(PTHREAD_CANCEL_ENABLE);
      thread.second->setCancelType(PTHREAD_CANCEL_ASYNCHRONOUS);
      thread.second->sendCancelRequest();

      thread.second->unlockFlagMutex();
    }
  }
  lockRetVal();
  UserThread* joiner = currentUserThread->getJoiner();
  if (joiner)
  {
    joiner->signalJoin();
    currentUserThread->setJoiner(0);
  }
  unlockRetVal();
  
  KILLED_ = true;
  threads_lock_.release();
  debug(USERPROCESS, "PID: [%ld]: [%ld] called exit for this process!\n", pid_,currentThread->getTID());
  // callingThread->lockFlagMutex();
  // callingThread->setCancelState(PTHREAD_CANCEL_ENABLE);
  // callingThread->setCancelType(PTHREAD_CANCEL_ASYNCHRONOUS);
  // callingThread->sendCancelRequest();
  // callingThread->unlockFlagMutex();
  if(kill_currentThread)
    Syscall::pthread_exit((void*) exit_code);
}

//unlocks the retvallock
bool UserProcess::getRetVal(size_t tid, void** value)
{
  if(!returnvalue_lock_.isHeldBy(currentUserThread))
    returnvalue_lock_.acquire();

  if (returnvalues_.find(tid) != returnvalues_.end())
  {
    *value = returnvalues_[tid];
    returnvalues_.erase(tid);
    returnvalue_lock_.release();
    return true;
  }
  returnvalue_lock_.release();
  return false;
}

int UserProcess::execv(const char* path, char *const argv[], size_t argc, ustl::queue<size_t>* ppns)
{
  debug(X_USERPROCESS, "execv() called. opening fd of %s and setting up loader\n", path);

  waiting_exec_lock_.acquire();
  if (waiting_exec_ == 0)
  {
    ssize_t old_fd = fd_;
    Loader* old_loader = loader_;
    ssize_t new_fd = VfsSyscall::open(path, O_RDONLY);

    // setup_fail makes use of short circuit evaluation (true || X -> true. X will not be executed).
    size_t loader_ppn = ppns->front();
    ppns->pop();
    bool setup_fail = !setupLoader(new_fd, loader_ppn) || !removeOldProcessInformation() || (currentUserThread->execv(argv, argc, ppns) == -1);

    if(setup_fail)
    {
      debug(USERPROCESS, "execv() ERREOR in setup_fail triggered. returning -1.\n");
      VfsSyscall::close(new_fd);
      if (loader_)
      {
        InvertedPageTable::instance()->lockIPT();
        delete loader_;
        InvertedPageTable::instance()->unlockIPT();

      }
      
      fd_ = old_fd;
      loader_ = old_loader;
      waiting_exec_ = 0;
      waiting_exec_lock_.release();
      return -1;
    }

    // new_fd and new_loader - old ones must be closed and freed afterwards!
    VfsSyscall::close(old_fd);
    InvertedPageTable::instance()->lockIPT();
    delete old_loader; 
    InvertedPageTable::instance()->unlockIPT();


    // exec success, we're ready for another exec call!
    waiting_exec_ = 0;
    waiting_exec_lock_.release();
  }
  else
  {
    waiting_exec_lock_.release();
    return -1;
  }
  // open new_fd and new_loader - old ones must be closed and freed afterwards.
 // debug(X_USERPROCESS, "execv() [%ld]  fd and loader setup finished successfully exiting exec\n", getPID());
  return 0;
}

bool UserProcess::setupLoader(ssize_t fd, size_t ppn)
{
  // faulty fd
  if(fd < 0)
  {
    debug(X_USERPROCESS, "setuploader(%ld) failed because... fd value\n", fd);
    PageManager::instance()->freePPN(ppn);
    return false;
  }

  // set fd_, new_loader -> check new loader, get ready to rumble if possible
  fd_ = fd;
  Loader* new_loader = new Loader(fd_, ppn);
  if(!new_loader || !new_loader->loadExecutableAndInitProcess())
  {
    debug(LOADER, "setuploader() failed because %s\n", (new_loader) ? "couldnt load executable" : "couldnt create archmem");
    loader_ = 0;
    PageManager::instance()->freePPN(ppn);
    return false;
  }

  // success
  loader_ = new_loader;
  loader_->arch_memory_.setProcess(this);
  debug(LOADER, "setuploader() sucess \n");
  return true;
}

bool UserProcess::removeOldProcessInformation()
{
  debug(X_USERPROCESS, "removingOldProcessInformation() entered: PID [%ld] \n", getPID());

  // only 1 lonely thread -> nobody must be killed today.
  threads_lock_.acquire();
  if (threads_.size() < 2)
  {
    KILLED_ = true;
    threads_lock_.release();
    goto killing_done;
  }
  waiting_exec_ = currentUserThread;
  threads_lock_.release();

  exit(13579, false);
  currentUserThread->waitExec();

killing_done:
  // clear returnvalues_
  if (!returnvalue_lock_.isHeldBy(currentUserThread))
    returnvalue_lock_.acquire();
  returnvalues_.clear();
  returnvalue_lock_.release();

  // clear offsets_
  if (!offsetlist_lock_.isHeldBy(currentUserThread))
    offsetlist_lock_.acquire();
  offsets_.clear();
  offsetlist_lock_.release();
  debug(X_USERPROCESS, "[%ld] removingOldProcessInformation() finished\n", getPID());

  // set KILLED_ to false
  threads_lock_.acquire();
  KILLED_ = false;
  threads_lock_.release();
  return true;
}

void UserProcess::setWaitStatus(bool arg)
{ 
  wait_status_ = arg; 
}

void UserProcess::setChildStatus(bool arg)
{ 
  child_ = arg; 
}


void UserProcess::setDuaration(size_t duaration)
{ 
  duaration_ = duaration; 
}
size_t UserProcess::getClockSum()
{
  size_t sum = 0;
  for (ustl::map<size_t, UserThread*>::iterator i = threads_.begin(); i != threads_.end(); ++i) 
  {
    sum += Scheduler::instance()->getRDTSC() - i->second->getLastStart();
  }
  return sum;
}

bool UserProcess::initPBreak()
{
  size_t rand, offset, addressnooffset;
  addressnooffset = loader_->getBSSEnd();
  addressnooffset += (5 * PAGE_SIZE);
  addressnooffset = addressnooffset >> 12;
  addressnooffset = addressnooffset<< 12;
  debug(USERPROCESS, "%lx is adress no offset\n", addressnooffset);
  if (addressnooffset > END_OF_HEAP)
  {
    //we cannot get a heap sry...
    return false;
  }
  else if (addressnooffset > BEGIN_HEAP_AT_LEAST)
  {
    //guess you ll just have a smaller heap
    loader_->setPBreak(addressnooffset);
    return true;
  }
  offset = 0;
  
  while (!offset)
  {
    rand = Scheduler::instance()->getRDTSC();
    offset = rand % ((BEGIN_HEAP_AT_LEAST - addressnooffset) / PAGE_SIZE);
    //just to double check
    if ((addressnooffset + (offset * PAGE_SIZE)) > BEGIN_HEAP_AT_LEAST)
      offset = 0;
  }
  loader_->setPBreak(addressnooffset + (offset * PAGE_SIZE));
  debug(USERPROCESS, "set PBreak to %lx\n", loader_->getPBreak());
  return true;
}

void UserProcess::getHeapPage(size_t address, ustl::queue<size_t>* ppns)
{
  //seems like we need a heap page!
  assert(threads_lock_.isHeldBy(currentThread));
  assert(PBreak_mutex_.isHeldBy(currentThread));
  
  if (checkKill())
    return;
  
  InvertedPageTable::instance()->lockIPT();
  loader_->arch_memory_.lockArchMemory();
  ArchMemoryMapping m = loader_->arch_memory_.resolveMapping(address / PAGE_SIZE);
  //need to resolve this step by step so we dont get pagefaults
  if (m.pml4[m.pml4i].present && m.pdpt[m.pdpti].pd.present && m.pd[m.pdi].pt.present && m.pt[m.pti].present)
  {
    debug(USERPROCESS, "Page already present!\n");
    InvertedPageTable::instance()->unlockIPT();
    loader_->arch_memory_.unlockArchMemory();
    return;
  }
  debug(USERPROCESS, "Page NOT present: getting a new one!\n");
  
 //size_t ppn = PageManager::instance()->allocPPN();
  if (!loader_->arch_memory_.mapPage((address / PAGE_SIZE), ppns, 1))
  {
    debug(USERPROCESS, "getnewpage(): RIP. asserting.\n");
    assert(false);
  }
}

void UserProcess::checkBrkFree(size_t brk_prev, size_t brk_now, ustl::queue<size_t>* ppns)
{
  InvertedPageTable* IPT = InvertedPageTable::instance();
  size_t vpn_start, vpn_end;
  vpn_start = brk_now / PAGE_SIZE;
  vpn_end = brk_prev / PAGE_SIZE;
  IPT->lockIPT();
  loader_->arch_memory_.lockArchMemory();
  for (size_t iter = vpn_start + 1; iter <= vpn_end; iter++)
  {
    ArchMemoryMapping m = loader_->arch_memory_.resolveMapping(iter);
    if (m.pml4[m.pml4i].present && m.pdpt[m.pdpti].pd.present && m.pd[m.pdi].pt.present && m.pt[m.pti].present)
    {
      loader_->arch_memory_.unmapPage(iter, ppns);
    }
  }
  IPT->unlockIPT();
  loader_->arch_memory_.unlockArchMemory();
}
