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
    list_of_processes_lock_("ProcessRegistry::list_of_processes_lock_"),
    wait_pid_lock_("ProcessRegistry::wait_pid_lock_")
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

void ProcessRegistry::processExit(UserProcess* user_proc)
{
  list_of_processes_lock_.acquire();
  if (list_of_processes_.find(user_proc->getPID()) != list_of_processes_.end())
  {
    list_of_processes_.erase(user_proc->getPID());
    UserProcess* waiter = 0;
    if ((waiter = user_proc->checkWaiter()) != 0)
    {
      debug(X_USERPROCESS, "Process [%ld] now dying and posting for [%ld]\n", user_proc->getPID(), waiter->getPID());
      waiter->postPIDSem();
    }
    else
      debug(X_USERPROCESS, "Process [%ld] now dying and posting for nobody :(\n", user_proc->getPID());
  }
  else
  {
    list_of_processes_lock_.release();
    return;
  }
  list_of_processes_lock_.release();

    //assert(false && "how did that process get removed already?");
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
  size_t return_to = 6;
  auto parent = currentUserThread->getProcess();
  
  if (parent->checkKill())
  {
    return -1;
  }
  
  //debug(PROCESS_REG, "After parent read %p\n", parent);
  debug(X_USERTHREAD, "[%ld] is creating the proc\n", currentThread->getTID());
  UserProcess* process = 0;
  process = new UserProcess(parent, &return_to);

  if (return_to != 0)
  {
    debug(PROCESS_REG, "Ups, something went wrong creating the UserProcess for fork[%ld]... dont assert!\n", return_to);
    debug(X_USERTHREAD, "[%ld] is deleting the proc\n", currentThread->getTID());
    delete process;
    return -1;
    //assert(false);
  }
  
  debug(PROCESS_REG, "After new UserProcess\n");
  // if (!process || process->getPID() == 0)
  // {
  //   debug(PROCESS_REG, "Ups, something went wrong creating the UserProcess for fork!\n");
  //   delete process;
  //   return -1;
  // }

  list_of_processes_lock_.acquire();
  list_of_processes_.insert(ustl::make_pair(process->getPID(), process));
  list_of_processes_lock_.release();
  
  debug(PROCESS_REG, "forked process with pid (%ld)\n", process->getPID());
  return process->getPID();
}

ustl::map<size_t, UserProcess*> ProcessRegistry::getProcessList()
{
  return list_of_processes_;
}


void ProcessRegistry::createProcess(const char* path)
{
  debug(PROCESS_REG, "createProcess(path = %s)\n", path);
  size_t returnto = 6;
  FileSystemInfo* fs_info = new FileSystemInfo(*working_dir_);
  if(!fs_info)
  {
    debug(PROCESS_REG, "ERROR: createProcess() -> unable to create object fs_info\n");
    return;
  }
  UserProcess* process = new UserProcess(path, fs_info, &returnto);
  if (returnto != 0)
  {
    debug(PROCESS_REG, "Ups, something went wrong creating the UserProcess[%ld]: [%ld]... assert!\n",process->getPID(), returnto);
    assert(false);
  }
  // if(!process || process->getPID() == 0)
  // {
  //   debug(PROCESS_REG, "ERROR: createProcess() -> unable to create object process\n");
  //   return;
  // }

  debug(X_PROCESS_REG, "created process successfully!\n");
  // successful UserProcess creation: add to ProcessRegistry::list_of_processes_
  list_of_processes_lock_.acquire();
  list_of_processes_.insert(ustl::make_pair(process->getPID(), process));
  list_of_processes_lock_.release();
  debug(PROCESS_REG, "PID [%ld] filename: %s | Created and added to ProcessRegistry::list_of_processes_\n", process->getPID(), path);
}

int ProcessRegistry::execv(const char* path, char *const argv[])
{
  debug(X_PROCESS_REG, "execv said: argv != NULL -> execvProcess(path, argv) called\n");
  int argc = areExecArgsValid(path, argv);
  if(argc > 0)
  {
    // copy args locally because user's pointers may have wonky behaviour
    char* here[argc];
    for (int i = 0; i < argc; i++)
    {
      here[i] = new char[strlen(argv[i]) + 1];
      memcpy(here[i], argv[i], strlen(argv[i]));
      here[i][strlen(argv[i])] = '\0';
      debug(X_USERTHREAD, "execv(): memcpy(): copying %s from %lx to here[%d] which lies at %lx\n", argv[i], (size_t)(argv + i), i, (size_t)(here + i));
    }
    debug(X_USERTHREAD, "execv(): copied from old archmem into char* here[] finished\n");
    return currentUserThread->getProcess()->execv(path, here, argc);
  }

  return argc;
}

int ProcessRegistry::areExecArgsValid(const char* path, char* const argv[])
{
  debug(X_PROCESS_REG, "areExecArgsValid()?\n");
  
  // here we already know that path is okay and argv != NULL -> check if 
  if((size_t)argv >= USER_BREAK || (size_t)argv[0] >= USER_BREAK)
    return -1;

  // first char* may be null. call exec without args
  if(!argv[0])
    return execv(path);

  // increase argc until NULL. also don't accept too long strings.
  int argc = 0;
  while(argv[argc])
  {
    for(int i = 0; argv[argc][i]; i++)
      if(unlikely(i > EXECV_MAX_ARG_LEN))
        return -1;
    debug(X_PROCESS_REG, "argv[%d] is %s\n", argc, argv[argc]);
    argc++;
  }

  // everything seemed okay..
  debug(X_PROCESS_REG, "areExecArgsValid(): everything seems fine\n");
  return argc;
}

int ProcessRegistry::execv(const char* path)
{
  debug(X_PROCESS_REG, "execv() said: no args! path = %s\n", path);
  UserProcess* currentProcess = currentUserThread->getProcess();
  debug(PROCESS_REG, "execv() for TID [%ld] in PID [%ld]\n", currentThread->getTID(), currentProcess->getPID());
  return currentProcess->execv(path);
}





size_t ProcessRegistry::waitPid(size_t arg1, size_t* arg2, size_t arg3, UserProcess* parent_process)
{
  debug(WAITPID, "id: %ld\n", parent_process->getPID());
  int return_pid = 0;
  if((long int) arg1 > 0) // any specifed process
  {
    debug(WAITPID, "arg1 greater 0, process %ld\n", arg1);
    list_of_processes_lock_.acquire();
    auto search_child = list_of_processes_.find(arg1);
    if (search_child == list_of_processes_.end())
    {
      list_of_processes_lock_.release();
      debug(WAITPID, "Not found, process %ld\n", arg1);
      return -1; //exit value returned
    }
    UserProcess* to_be_joined = search_child->second;
    if (to_be_joined->checkWaiter() == 0 && parent_process->checkWaiter() != to_be_joined)
    {
      to_be_joined->setWaiter(parent_process);  
    }
    else
    {
      list_of_processes_lock_.release();
      return -1;
    }
    
    return_pid = to_be_joined->getPID();
    list_of_processes_lock_.release();
    debug(X_USERPROCESS, "Process [%ld] now waiting for [%ld]\n", parent_process->getPID(), to_be_joined->getPID());
    parent_process->waitPIDSem();
    // while (parent_process->getWaitStatus() && !search_child->second->getWaitStatus() && search_child->second->getProcessState() == 2) 
    // {
    //   Scheduler::instance()->yield();
    //   if(process_state != search_child->second->getProcessState() || search_child->second->getProcessState() == 0)
    //   {
    //     list_of_processes_lock_.acquire();
    //     parent_process->setWaitStatus(0);
    //     list_of_processes_lock_.release();
    //   }
    // }
  }
  /*
  else if((long int) arg1 == -1) // any child process.
  {
    debug(WAITPID, "arg1 equals -1, process %ld\n", arg1);
    wait_pid_lock_.acquire();
    ustl::map<size_t, UserProcess*> list = ProcessRegistry::getProcessList();
    UserProcess* child = parent_process;
    wait_pid_lock_.release();
    for (ustl::map<size_t, UserProcess*>::iterator i = list.begin(); i != list.end(); ++i) 
    {
      if((i->second->getChildStatus() == 1) && (parent_process->getPID() != i->second->getPID()) && (child->getWaitStatus() == 0))
      {
        wait_pid_lock_.acquire();
        child = i->second;
        wait_pid_lock_.release();
        break;
      }
    }
    if(child->getPID() == parent_process->getPID()){
      debug(WAITPID, "No child process\n");
      return -1;
    }
    wait_pid_lock_.acquire();
    parent_process->setWaitStatus(1);
    size_t process_state = child->getProcessState();
    return_pid = child->getPID();
    wait_pid_lock_.release();
    //maybe the child wait status can be changed to 1 with more waitpids
    while (parent_process->getWaitStatus() && !child->getWaitStatus() && child->getProcessState() == 2) 
    {
      debug(WAITPID, "in while  %ld\nSTATES parent: %d, child %d\nID parent: %ld, child %ld\nCHILD parent: %d, child %d\nWAIT parent: %d, child %d\n",
      arg1, parent_process->getProcessState(), child->getProcessState(),
      parent_process->getPID(), child->getPID(), parent_process->getChildStatus(), child->getChildStatus(),
      parent_process->getWaitStatus(), child->getWaitStatus());

      Scheduler::instance()->yield();
      if(process_state != child->getProcessState() || child->getProcessState() == 0)
      {
        wait_pid_lock_.acquire();
        parent_process->setWaitStatus(0);
        wait_pid_lock_.release();
      }
    }
    debug(WAITPID, "after while \n");
  }
  */
  else if((long int) arg1 < -1) //  any child process whose process group ID is equal to the absolute value of pid. 
  {
    debug(WAITPID, "arg1 smaller -1\n"); // dont need to implement process groups
    return 0;
  }
  else if((long int) arg1 == 0) // any child process whose process group ID is equal to that of the calling process. 
  {
    debug(WAITPID, "arg1 equals 0\n"); // dont need to implement process groups
    return 0;
  }
  else //   something went wrong
  {
    debug(WAITPID, "we have an error somewhere, process %ld\n", arg1);
    return -1;
  } 
  if(arg2 != 0)
  {
    debug(WAITPID, "arg2 different 0, process %ld\n", arg1);
  }
  if(arg3 > 0) 
  {
    debug(WAITPID, "arg3 bigger 0, process %ld\n", arg1);
  }
  return return_pid;
}

