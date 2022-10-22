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


UserProcess::UserProcess(ustl::string filename, FileSystemInfo *fs_info, uint32 terminal_number) :
    pid_(ProcessRegistry::instance()->createID()), 
    fd_(VfsSyscall::open(filename, O_RDONLY)), 
    fs_info_(fs_info),
    working_dir_(fs_info),
    name_(filename.c_str()),
    threads_lock_("UserProcess::threads_lock_"),
    returnvalue_lock_("UserProcess::retvallock"),
    offsetlist_lock_("UserProcess::offsets")
{
  debug(USERPROCESS, "entering constructor of %s\n", name_.c_str());
  debug(USERPROCESS, "fs_info present. pointer in there is: %p\n", fs_info_);
  ProcessRegistry::instance()->processStart(); //should also be called if you fork a process

  if (fd_ >= 0)
    loader_ = new Loader(fd_);

  if (!loader_ || !loader_->loadExecutableAndInitProcess())
  {
    debug(USERPROCESS, "Error: loading %s failed!\n", name_.c_str());
    // kill(); // not needed anymore, no thread created yet
    return;
  }
  debug(X_USERPROCESS, "%s: Loader finished. Loader lies at (%p)\n", name_.c_str(), loader_);

  UserThread* first_thread = new UserThread(this, working_dir_, name_.c_str(),terminal_number, UserProcess::getRandomPageOffset());
  assert(first_thread && "UserThread constructor failed");

  /* moved to addToThreadList()
  threads_lock_.acquire();
  threads_.insert(ustl::make_pair(first_thread->getTID(), first_thread));
  threads_lock_.release();*/
}

// User Process Constructor for fork
UserProcess::UserProcess(UserProcess *parent, size_t pid) :
  pid_(pid),
  fd_(VfsSyscall::open(parent->name_, O_RDONLY)),
  fs_info_(parent->fs_info_),
  //loader_(new Loader(fd_)),
  working_dir_(new FileSystemInfo(*parent->fs_info_)),
  my_terminal_(parent->my_terminal_),
  name_(parent->name_),
  threads_lock_("UserProcess::threads_lock_"),
  returnvalue_lock_("UserProcess::retvallock")
{
  debug(X_USERPROCESS, "Entering UserProcess fork constructor\n");
  if(!working_dir_)
  {
    debug(USERPROCESS, "Failed to obtain working directory!\n");
    return;
  }

  loader_ = new Loader(fd_);
  if(fd_ < 0)
  {
    debug(USERPROCESS, "Failed to create Loader!\n");
  }

  if(!loader_ || !loader_->arch_memory_.page_map_level_4_)
  {
    debug(USERPROCESS, "Failed to create Loader!\n");
    return;
  }

  if(!loader_->loadExecutableAndInitProcess())
  {
    debug(USERPROCESS, "Failed to init Process\n");
    return;
  }

  debug(USERPROCESS, "Start copying virtual memory!\n");
  ((UserThread*)currentThread)->loader_->arch_memory_.copyVirtualMem(loader_->arch_memory_);


  //local fd

  debug(USERPROCESS, "Creating new Thread for Fork\n");
  auto thread = new UserThread(this,(UserThread*) currentThread);
  if(!thread || thread->getTID()==0)
  {
    debug(USERPROCESS, "Failed to create Thread for Fork!\n");
    delete thread;
    return;
  }

  threads_lock_.acquire();
  threads_.insert({thread->getTID(),thread});
  threads_lock_.release();

  ProcessRegistry::instance()->processStart();
  Scheduler::instance()->addNewThread(thread);
  Scheduler::instance()->printThreadList();
}

UserProcess::~UserProcess()
{
  debug(X_USERPROCESS, "PID [%ld]: destructor called\n", pid_);
  assert(Scheduler::instance()->isCurrentlyCleaningUp());
  delete loader_;
  loader_ = 0;

  if (fd_ > 0)
    VfsSyscall::close(fd_);

  delete working_dir_;
  working_dir_ = 0;

  ProcessRegistry::instance()->processExit();
  debug(X_USERPROCESS, "PID [%ld]: destructor done\n", pid_);
}

bool UserProcess::addToThreadList(UserThread* thread)
{
  threads_lock_.acquire();
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

  threads_lock_.release();
  return true;
}
//this function locks internally!
bool UserProcess::addToRetvalList(size_t tid, void* value){
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
  if(threads_.size() == 1)
    thread->setLast();

  threads_.erase(tid);
  debug(X_USERPROCESS, "removed TID: [%ld] from UserProcess::threads_\n", tid);

  return true;
}

size_t UserProcess::getRandomPageOffset(){
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
  } while (checkInList(page_offset));
  offsets_.push_back(rand);
  offsetlist_lock_.release();
  //debug(USERPROCESS,"read %ld from tsc and MAX STACKS btw is %lld offset is %ld!!\n", rand, MAX_STACKS, page_offset);
  
  return page_offset;
}

bool UserProcess::checkInList(size_t NR)
{
  for (auto val : offsets_)
  {
    if(val == NR)
      return true;
  }
  return false;
}

//caution! aquire lock before!!!
Thread* UserProcess::findInThreadList(size_t tid){
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

size_t UserProcess::createNewThread(size_t start_routine, size_t args, size_t wrapper)
{
  // pthread
  UserThread* thread = new UserThread(wrapper, UserProcess::getRandomPageOffset());
  /*First Argument: RDI
    Second Argument: RSI
    Third Argument: RDX
    Fourth Argument: RCX
    Fifth Argument: R8
    Sixth Argument: R9
  */
  thread->user_registers_->rdi = start_routine;
  thread->user_registers_->rsi = args;
  if(thread)
    return thread->getTID();
  
  return 0;
}

void UserProcess::exit(size_t exit_code)
{
  debug(USERPROCESS, "PID: [%ld] exit(exit_code = %ld) called\n", pid_, exit_code);
  threads_lock_.acquire();
  for(auto thread : threads_) // first = tid, second = *Thread
  {
    if(unlikely(thread.first == currentThread->getTID()));
    else
    {
      threads_lock_.release();
      killThread(thread.second);
      threads_lock_.acquire();
      removeFromThreadList(thread.second);
    }
  }
  removeFromThreadList((UserThread*) currentThread);
  threads_lock_.release();


  killThread((UserThread*)currentThread);
  
  debug(USERPROCESS, "PID: [%ld] exit killed all except for currentThread->tid_ = %ld\n", pid_, currentThread->getTID());

}

void UserProcess::killThread(UserThread* thread)
{
  debug(USERPROCESS, "PID: [%ld] killThread() called for tid [%ld]\n", pid_, thread->getTID());
  thread->kill();
}

bool UserProcess::getRetVal(size_t tid, void** value){
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
