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


UserProcess::UserProcess(ustl::string filename, FileSystemInfo *fs_info, uint32 terminal_number) :
    pid_(ProcessRegistry::instance()->createID()), 
    fd_(VfsSyscall::open(filename, O_RDONLY)), 
    fs_info_(fs_info),
    name_(filename.c_str()),
    threads_lock_("UserProcess::threads_lock_")
{
  debug(USERPROCESS, "entering constructor of %s\n", name_.c_str());
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

  UserThread* first_thread = new UserThread(this, working_dir_, name_.c_str(), terminal_number);
  assert(first_thread && "UserThread constructor failed");

  /* moved to addToThreadList()
  threads_lock_.acquire();
  threads_.insert(ustl::make_pair(first_thread->getTID(), first_thread));
  threads_lock_.release();*/
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

    assert(false); // assert or not? 
    return false; 
  }

  threads_.insert(ustl::make_pair(tid, thread));
  debug(X_USERPROCESS, "added TID: [%ld] to UserProcess::threads_\n", tid);

  threads_lock_.release();
  return true;
}

bool UserProcess::removeFromThreadList(UserThread* thread)
{
  threads_lock_.acquire();

  // checks if the thread is in list
  size_t tid = thread->getTID();
  if(threads_.find(tid) == threads_.end())
  {
    debug(USERPROCESS, "SHIT: removeFromThreadList() could not find thread with tid [%ld] in list\n", tid);
    threads_lock_.release();
    assert(false); // assert or not? 
    return false; 
  }

  // sets Thread::last_ if it's last thread
  if(threads_.size() == 1)
    thread->setLast();

  threads_.erase(tid);
  debug(X_USERPROCESS, "removed TID: [%ld] from UserProcess::threads_\n", tid);

  threads_lock_.release();
  return true;
}

size_t UserProcess::createNewThread()
{
  // here the UserThread should be created with a new constructor that's not implemented yet.
  size_t tid = 123;

  return tid; // must be 0 if fail, TID on success!
}