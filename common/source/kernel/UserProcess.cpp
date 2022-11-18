#include "ProcessRegistry.h"
#include "UserProcess.h"
#include "UserThread.h"
#include "kprintf.h"
#include "Console.h"
#include "Loader.h"
#include "VfsSyscall.h"
#include "File.h"
#include "ArchMemory.h"
#include "PageManager.h"
#include "ArchThreads.h"
#include "offsets.h"
#include "VfsSyscall.h"
#include "Scheduler.h"

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
    waitpid_sem_("Userprocess::waitpid_sem_")
{
  *returnto = 5;
  waitpid_sem_.init(0);
  debug(USERPROCESS, "entering constructor of %s\n", name_.c_str());
  debug(USERPROCESS, "fs_info present. pointer in there is: %p\n", fs_info_);
  ProcessRegistry::instance()->processStart(); //should also be called if you fork a process

  if(!setupLoader(fd_))
  {
    *returnto = 1;
    return;
  }
  size_t returnto_th = -1;
  debug(X_USERPROCESS, "%s: Loader finished. Loader lies at (%p)\n", name_.c_str(), loader_);
  setChildStatus(0);
  //threads_lock_.acquire();
  UserThread* first_thread = new UserThread(this, working_dir_, name_.c_str(),terminal_number, UserProcess::getRandomPageOffset(), &returnto_th);
  //threads_lock_.release();
  assert(first_thread && "UserThread constructor failed");
  if (returnto_th != 0)
  {
    *returnto = 4;
    return;
  }
  
  *returnto = 0;
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
  waitpid_sem_("Userprocess::waitpid_sem_")
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


  if (!setupLoader(fd_))
  {
    *returnto = 1;
    return;
  }
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
    *returnto = 4;
    return;
  }
  offsets_.push_back(currentUserThread->getStackInfo()->page_offset_);
  setChildStatus(1);

  if (!threads_lock_.isHeldBy(currentThread))
  {
    threads_lock_.acquire();
  }
  
  addToThreadList(thread);
  threads_lock_.release();
  ProcessRegistry::instance()->processStart();
  
  //?
  Scheduler::instance()->printThreadList();
  *returnto = 0;
  return;
}

UserProcess::~UserProcess()
{

  debug(X_USERPROCESS, "PID [%ld]: destructor called by [%ld]\n", pid_, currentThread->getTID());
  if(Scheduler::instance()->isCurrentlyCleaningUp())
    Scheduler::instance()->yield();
  if (loader_ != 0)
  {
    delete loader_;
  }
  
  loader_ = 0;

  if (fd_ > 0)
    VfsSyscall::close(fd_);

  delete working_dir_;
  working_dir_ = 0;

  ProcessRegistry::instance()->processExit(this);
  debug(X_USERPROCESS, "PID [%ld]: destructor done by [%ld]\n", pid_, currentThread->getTID());
}

bool UserProcess::addToThreadList(UserThread* thread)
{
  //threads_lock_.acquire();
  size_t tid = thread->getTID();

  if(threads_.find(tid) != threads_.end())
  {
    debug(USERPROCESS, "SHIT: addToThreadList() already has thread with tid [%ld] in list\n", tid);
    threads_lock_.release();

    assert(false); // assert or not? - lets leave them in for now :D 
    return false; 
  }

  threads_.insert(ustl::make_pair(tid, thread));
  debug(X_USERPROCESS, "added TID: [%ld] to UserProcess::threads_\n", tid);

  //threads_lock_.release();
  return true;
}

//this function locks internally!
bool UserProcess::addToRetvalList(size_t tid, void* value){
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
  if (!threads_lock_.isHeldBy(currentThread))
  {
    threads_lock_.acquire();
  }
  
  ustl::map<size_t, UserThread*>::iterator it;
  for (it = threads_.begin(); it != threads_.end(); it++)
  {
    if (address <= it->second->getStackInfo()->userstack_start_ && address > it->second->getStackInfo()->userstack_end_)
    {
      threads_lock_.release();
      return it->second;
    }
  }
  threads_lock_.release();
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

UserThread* UserProcess::createNewThread(size_t start_routine, size_t args, size_t wrapper, int32 joinstate = PTHREAD_CREATE_JOINABLE)
{
  // pthread
  UserThread* thread = 0;
  size_t return_to = 6;
  threads_lock_.acquire();
  if (KILLED_)
  {
    threads_lock_.release();
    return 0;
  }
  threads_lock_.release();

  thread = new UserThread(wrapper, getRandomPageOffset(), &return_to);
  
  threads_lock_.acquire();
  if (KILLED_ && return_to == 0)
  {
    assert(false && "this is baaad... created thread even though i should be dead1\n");
  }
  if (return_to != 0)
  {
    debug(USERPROCESS, "Ups, something went wrong creating the Userthread for proc [%ld] [%ld]... assert!\n", pid_, return_to);
    threads_lock_.release();
    delete thread;
    return 0;
  }
    threads_lock_.release();


  /*First Argument: RDI
    Second Argument: RSI
    Third Argument: RDX
    Fourth Argument: RCX
    Fifth Argument: R8
    Sixth Argument: R9
  */
  if (thread)
  {
    if (joinstate != PTHREAD_CREATE_JOINABLE)
      thread->setJoinState(joinstate);
    
    thread->user_registers_->rdi = start_routine;
    thread->user_registers_->rsi = args;
    
    return thread;
  }
  else
  {
    debug(X_USERPROCESS, "something went wrong with threadcreation\n");
    return 0;
  }
  
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

int UserProcess::execv(const char* path)
{

  debug(X_USERPROCESS, "execv() called. opening fd of %s and setting up loader\n", path);
  ssize_t old_fd = fd_;
  Loader* old_loader = loader_;
  ssize_t new_fd = VfsSyscall::open(path, O_RDONLY);
  if(!setupLoader(new_fd))
  {
    debug(USERPROCESS, "execv() ERREOR with fd or Loader\n");
    fd_ = old_fd;
    VfsSyscall::close(new_fd);
    return -1;
  }
  debug(X_USERPROCESS, "execv() fd and loader setup finished successfully\n");
  if(!removeOldProcessInformation())
  {
  //  VfsSyscall::close(new_fd);
    fd_ = old_fd;
    VfsSyscall::close(new_fd);
    return -1;
  }
  name_ = path;

  // exec
  debug(X_USERPROCESS, "execv(path = %s) sucessfully opened file + created loader + did loadExecutablea() + killed all threads.\n", path);
  threads_lock_.acquire();
  KILLED_ = false;
  threads_lock_.release();

  currentUserThread->execv();

  VfsSyscall::close(old_fd);
  delete old_loader; // triggers assert.. i guess i'll just accept the memory leak.
  waiting_exec_lock_.acquire();
  waiting_exec_ = 0;
  waiting_exec_lock_.release();
  
  debug(X_USERPROCESS, "[%ld] execv() fd and loader setup finished successfully exiting exec\n", getPID());
  return 0;
}

int UserProcess::execv(const char* path, char *const argv[], size_t argc)
{
  debug(X_USERPROCESS, "execv() called. opening fd of %s and setting up loader\n", path);
  ssize_t old_fd = fd_;
  Loader* old_loader = loader_;
  ssize_t new_fd = VfsSyscall::open(path, O_RDONLY);
  if(!setupLoader(new_fd))
  {
    debug(USERPROCESS, "execv() ERREOR with fd or Loader\n");
    fd_ = old_fd;
    VfsSyscall::close(new_fd);
    return -1;
  }
  debug(X_USERPROCESS, "execv() fd and loader setup finished successfully\n");
  if(!removeOldProcessInformation())
  {
  //  VfsSyscall::close(new_fd);
    fd_ = old_fd;
    VfsSyscall::close(new_fd);
    return -1;
  }
  name_ = path;

  threads_lock_.acquire();
  KILLED_ = false;
  threads_lock_.release();
  
  currentUserThread->execv(argv, argc);

  VfsSyscall::close(old_fd);
  delete old_loader; // triggers assert.. i guess i'll just accept the memory leak.
  
  waiting_exec_lock_.acquire();
  waiting_exec_ = 0;
  waiting_exec_lock_.release();
  
  debug(X_USERPROCESS, "[%ld] execv() fd and loader setup finished successfully exiting exec\n", getPID());
  return 0;
}

bool UserProcess::setupLoader(ssize_t fd)
{

  fd_ = fd;
  if(fd_ < 0)
  {
    debug(LOADER, "setuploader failed because fd is unreasonable(%ld)\n", fd);
    return false;
  }
  Loader* new_loader = new Loader(fd_);
  if(!new_loader || !new_loader->loadExecutableAndInitProcess())
  {
    debug(LOADER, "setuploader failed because %s\n", (new_loader) ? "couldnt load executable" : "couldnt create archmem");
    return false;
  }

  loader_ = new_loader;
  loader_->arch_memory_.setProcess(this);
  return true;
}

bool UserProcess::removeOldProcessInformation()
{
  debug(X_USERPROCESS, "[%ld] removingOldProcessInformation() entered\n", getPID());

  threads_lock_.acquire();
  if (threads_.size() < 2)
  {
    threads_lock_.release();
    goto done;
  }
  threads_lock_.release();

  waiting_exec_lock_.acquire();
  if (waiting_exec_ == 0)
  {
    waiting_exec_ = currentUserThread;
    exit(13579, false);
    currentUserThread->waitExec();
  }
  else
  {
    waiting_exec_lock_.release();
    debug(X_USERTHREAD, "Somebody was faster than me... well I m dying goodbye!\n");
    return false;
  }
  waiting_exec_lock_.release();

done:
  if (!returnvalue_lock_.isHeldBy(currentUserThread))
    returnvalue_lock_.acquire();
  returnvalues_.clear();
  returnvalue_lock_.release();

  if (!offsetlist_lock_.isHeldBy(currentUserThread))
    offsetlist_lock_.acquire();
  offsets_.clear();
  offsetlist_lock_.release();
  debug(X_USERPROCESS, "[%ld] removingOldProcessInformation() finished\n", getPID());
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
  threads_lock_.acquire();
  debug(CLOCK, "Number of threads %ld\n", getNrOfThreads());
  for (ustl::map<size_t, UserThread*>::iterator i = threads_.begin(); i != threads_.end(); ++i) 
  {
    if(i->second->schedulable() == true)
    {
      debug(CLOCK, "RDTSC - last start %ld\n", Scheduler::instance()->getRDTSC() - i->second->getLastStart());
      sum += Scheduler::instance()->getRDTSC() - i->second->getLastStart();
      debug(CLOCK, "sum: %ld\n", sum);
    }
  }
  threads_lock_.release();
  return sum;
}
