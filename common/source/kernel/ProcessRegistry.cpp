#include <mm/KernelMemoryManager.h>
#include "ProcessRegistry.h"
#include "Scheduler.h"
#include "UserProcess.h"
#include "kprintf.h"
#include "VfsSyscall.h"
#include "VirtualFileSystem.h"
#include "ArchThreads.h"
#include "offsets.h"


ProcessRegistry* ProcessRegistry::instance_ = 0;

ProcessRegistry::ProcessRegistry(FileSystemInfo *root_fs_info, char const *progs[]) :
    Thread(root_fs_info, "ProcessRegistry", Thread::KERNEL_THREAD), progs_(progs), progs_running_(0),
    counter_lock_("ProcessRegistry::counter_lock_"),
    all_processes_killed_(&counter_lock_, "ProcessRegistry::all_processes_killed_"),
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

size_t ProcessRegistry::createID()
{
  ArchThreads::atomic_add(next_id_,1);
  return next_id_;
}

size_t ProcessRegistry::processFork()
{
  debug(PROCESS_REG, "processFork() called starting process creation\n");
  auto parent = currentUserThread->getProcess();
  //debug(PROCESS_REG, "After parent read %p\n", parent);
  auto process = new UserProcess(parent);

  debug(PROCESS_REG, "After new UserProcess\n");
  if (!process || process->getPID() == 0)
  {
    debug(PROCESS_REG, "Ups, something went wrong creating the UserProcess for fork!\n");
    delete process;
    return -1;
  }

  list_of_processes_lock_.acquire();
  list_of_processes_.insert(ustl::make_pair(process->getPID(), process));
  list_of_processes_lock_.release();
  
  debug(PROCESS_REG, "forked process with pid (%ld)\n", process->getPID());
  return process->getPID();
}

void ProcessRegistry::createProcess(const char* path)
{
  debug(PROCESS_REG, "createProcess(path = %s)\n", path);
  FileSystemInfo* fs_info = new FileSystemInfo(*working_dir_);
  if(!fs_info)
  {
    debug(PROCESS_REG, "ERROR: createProcess() -> unable to create object fs_info\n");
    return;
  }
  UserProcess* process = new UserProcess(path, fs_info);
  if(!process || process->getPID() == 0)
  {
    debug(PROCESS_REG, "ERROR: createProcess() -> unable to create object process\n");
    return;
  }

  debug(X_PROCESS_REG, "created process successfully!\n");
  // successful UserProcess creation: add to ProcessRegistry::list_of_processes_
  list_of_processes_lock_.acquire();
  list_of_processes_.insert(ustl::make_pair(process->getPID(), process));
  list_of_processes_lock_.release();
  debug(PROCESS_REG, "PID [%ld] filename: %s | Created and added to ProcessRegistry::list_of_processes_\n", process->getPID(), path);
}

int ProcessRegistry::execvProcess(const char* path, char *const argv[])
{
  debug(X_PROCESS_REG, "execv said: argv != NULL -> execvProcess(path, argv) called\n");
  // check if path is okay (no NULL-ptr & terminated with '\0')
  int argc = areExecArgsValid(path, argv);
  if(argc == -1)
    return argc;

  debug(PROCESS_REG, "execvProcess(path = %s, argv = %lx, argc = %d\n", path, (size_t)argv, argc);
  UserProcess* currentProcess = currentUserThread->getProcess();
  debug(PROCESS_REG, "execv() for TID [%ld] in PID [%ld]\n", currentThread->getTID(), currentProcess->getPID());
  return currentProcess->execv(path, argv, argc);
}

int ProcessRegistry::areExecArgsValid(const char* path, char* const argv[])
{
  debug(X_PROCESS_REG, "areExecArgsValid()?\n");
  // here we already know that path is okay and argv != NULL
  if((size_t)argv >= USER_BREAK)
    return -1;

  // first char* may null. call exec without process
  if(!argv[0])
  {
    debug(X_EXECV, "argv[0] = 0. calling execvProcess(path)\n");
    execvProcess(path);
  }

  int argc = 0;
  while(argv[argc])
  {
    for(int i = 0; argv[argc][i]; i++)
      if(unlikely(i > EXECV_MAX_ARG_LEN))
        return -1;
    argc++;
  }

  // everything seemed okay..
  debug(X_PROCESS_REG, "areExecArgsValid(): everything seems fine\n");
  return argc;
}

int ProcessRegistry::execvProcess(const char* path)
{
  debug(X_PROCESS_REG, "execv said: no args! path = %s\n", path);
  UserProcess* currentProcess = currentUserThread->getProcess();
  debug(PROCESS_REG, "execv() for TID [%ld] in PID [%ld]\n", currentThread->getTID(), currentProcess->getPID());
  return currentProcess->execv(path);
}
