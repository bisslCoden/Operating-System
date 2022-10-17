#include <mm/KernelMemoryManager.h>
#include "ProcessRegistry.h"
#include "Scheduler.h"
#include "UserProcess.h"
#include "kprintf.h"
#include "VfsSyscall.h"
#include "VirtualFileSystem.h"
#include "ArchThreads.h"



ProcessRegistry* ProcessRegistry::instance_ = 0;

ProcessRegistry::ProcessRegistry(FileSystemInfo *root_fs_info, char const *progs[]) :
    Thread(root_fs_info, "ProcessRegistry", Thread::KERNEL_THREAD), progs_(progs), progs_running_(0),
    counter_lock_("ProcessRegistry::counter_lock_"),
    all_processes_killed_(&counter_lock_, "ProcessRegistry::all_processes_killed_"),
    next_id_lock_("ProcessRegistry::next_pid_lock_"),
    list_of_processes_lock_("ProcessRegistry::list_of_processes_lock_")
{
  debug(X_PROCESS_REG, "instance created\n");
  instance_ = this; // instance_ is static! -> Singleton-like behaviour
}

ProcessRegistry::~ProcessRegistry()
{
}

ProcessRegistry* ProcessRegistry::instance()
{
  return instance_;
}

void ProcessRegistry::Run()
{
  if (!progs_ || !progs_[0])
    return;

  debug(PROCESS_REG, "mounting userprog-partition \n");

  debug(PROCESS_REG, "mkdir /usr\n");
  assert( !VfsSyscall::mkdir("/usr", 0) );
  debug(PROCESS_REG, "mount idea1\n");
  assert( !VfsSyscall::mount("idea1", "/usr", "minixfs", 0) );

  debug(PROCESS_REG, "mkdir /dev\n");
  assert( !VfsSyscall::mkdir("/dev", 0) );
  debug(PROCESS_REG, "mount devicefs\n");
  assert( !VfsSyscall::mount(NULL, "/dev", "devicefs", 0) );


  KernelMemoryManager::instance()->startTracing();

  for (uint32 i = 0; progs_[i]; i++)
  {
    createProcess(progs_[i]);
  }

  counter_lock_.acquire();

  while (progs_running_)
    all_processes_killed_.wait();

  counter_lock_.release();

  debug(PROCESS_REG, "unmounting userprog-partition because all processes terminated \n");

  VfsSyscall::umount("/usr", 0);
  VfsSyscall::umount("/dev", 0);
  vfs.rootUmount();

  Scheduler::instance()->printStackTraces();

  Scheduler::instance()->printThreadList();

  kill();
}

void ProcessRegistry::processExit()
{
  counter_lock_.acquire();

  if (--progs_running_ == 0)
    all_processes_killed_.signal();

  counter_lock_.release();
}

void ProcessRegistry::processStart()
{
  counter_lock_.acquire();
  ++progs_running_;
  debug(X_PROCESS_REG, "processStart(): progs_running_ %d\n", progs_running_);
  counter_lock_.release();
}

size_t ProcessRegistry::processCount()
{
  ScopeLock lock(counter_lock_);
  return progs_running_;
}

/* Used to create PIDs
size_t ProcessRegistry::createUID()
{
  ArchThreads::atomic_add(process_pids_,1);
  return process_pids_;
}
*/

size_t ProcessRegistry::processFork()
{
  size_t pid = createID();

  debug(PROCESS_REG, "Forking Process, next call to the UserProcess constructor with pid %ld\n",pid);
  auto process = new UserProcess(((UserThread*)currentThread)->getParentProcess(),pid);

  if (!process|| process->getPID()==0)
  {
    debug(PROCESS_REG, "Ups, something went wrong creating the UserProcess for frok!\n");
    delete process;
    return -1;
  }


  list_of_processes_lock_.acquire();
  list_of_processes_.insert(ustl::make_pair(pid, process));
  list_of_processes_lock_.release();

  debug(PROCESS_REG, "forked process with pid (%ld)\n",pid);
  return pid;
}



void ProcessRegistry::createProcess(const char* path)
{
  debug(PROCESS_REG, "create process %s\n", path);
  FileSystemInfo test = *working_dir_;
  debug(PROCESS_REG, "was able to deref that\n");
  UserProcess* process = new UserProcess(path, new FileSystemInfo(*working_dir_));
  assert(process && "Process creation failed miserably o_O");

  debug(PROCESS_REG, "created process successfully!\n");
  // successful UserProcess creation: add to ProcessRegistry::list_of_processes_
  list_of_processes_lock_.acquire();
  list_of_processes_.insert(ustl::make_pair(process->getPID(), process));
  list_of_processes_lock_.release();
  debug(PROCESS_REG, "PID [%ld] filename: %s | Created and added to ProcessRegistry::list_of_processes_\n", process->getPID(), path);
}

size_t ProcessRegistry::createID()
{
  next_id_lock_.acquire();
  size_t tmp = next_id_++;
  next_id_lock_.release();
  return tmp; 
}