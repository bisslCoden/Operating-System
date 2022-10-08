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
    pid_(ProcessRegistry::instance()->createPID()), 
    fd_(VfsSyscall::open(filename, O_RDONLY)), 
    fs_info_(fs_info),
    name_(filename.c_str()),
    list_of_threads_lock_("UserProcess::list_of_threads_lock_")
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

  // add  first UserThread to UserProcess::list_of_threads_
  list_of_threads_lock_.acquire();
  list_of_threads_.insert(ustl::make_pair(first_thread->getTID(), first_thread));
  list_of_threads_lock_.release();
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