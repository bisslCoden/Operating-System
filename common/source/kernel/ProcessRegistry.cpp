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
  size_t pid = createID();

  debug(PROCESS_REG, "Forking Process, next call to the UserProcess constructor with pid %ld\n",pid);
  auto parent = ((UserThread*)currentThread)->getParentProcess();
  //debug(PROCESS_REG, "After parent read %p\n", parent);
  auto process = new UserProcess(parent,pid);

  debug(PROCESS_REG, "After new UserProcess\n");
  if (!process || process->getPID()==0)
  {
    debug(PROCESS_REG, "Ups, something went wrong creating the UserProcess for fork!\n");
    delete process;
    return -1;
  }

  list_of_processes_lock_.acquire();
  list_of_processes_.insert(ustl::make_pair(pid, process));
  list_of_processes_lock_.release();
  debug(PROCESS_REG, "forked process with pid (%ld)\n",pid);
  
  return pid;
}

ustl::map<size_t, UserProcess*> ProcessRegistry::getProcessList()
{
  return list_of_processes_;
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

size_t ProcessRegistry::waitPid(size_t arg1, size_t* arg2, size_t arg3, UserProcess* parent_process)
{
  debug(DBEK, "id: %ld\n", parent_process->getPID());
  int return_pid = 0;
  if((long int) arg1 > 0) // any specifed process
  {
    list_of_processes_lock_.acquire();
    ustl::map<size_t, UserProcess*> list;
    list = ProcessRegistry::getProcessList();
    UserThread* callingthread = (UserThread*)currentThread;
    debug(DBEK, "arg1 greater 0, process %ld\n", arg1);
    auto search_child = list.find(arg1);
    list_of_processes_lock_.release();
   // auto search_parent = list.find(callingthread->getParentProcess()->getPID());
    if (search_child != list.end())
    {
      list_of_processes_lock_.acquire();
      callingthread->getParentProcess()->setWaitStatus(1);
      size_t process_state = search_child->second->getProcessState();
      return_pid = search_child->second->getPID();
      list_of_processes_lock_.release();
      while (callingthread->getParentProcess()->getWaitStatus()) 
      {
        Scheduler::instance()->yield();
        if(process_state != search_child->second->getProcessState() || search_child->second->getProcessState() == 0
        || callingthread->getParentProcess()->getProcessState() == 0)
        {
          list_of_processes_lock_.acquire();
          callingthread->getParentProcess()->setWaitStatus(0);
          list_of_processes_lock_.release();
        }
      }
    }
    else
    {
      debug(DBEK, "Not found, process %ld\n", arg1);
      //list_of_processes_lock_.release();
      return -1;
    }
    debug(DBEK, "PID of the return2: %ld\n", search_child->second->getPID());
  }
  else if((long int) arg1 == -1) // any child process.
  {
    list_of_processes_lock_.acquire();
    ustl::map<size_t, UserProcess*> list;
    list = ProcessRegistry::getProcessList();
    debug(DBEK, "arg1 equals -1, process %ld\n", arg1);
    ustl::map<size_t, UserProcess*>::iterator i;
    UserProcess* child = parent_process;
    list_of_processes_lock_.release();
    for (i = list.begin(); i != list.end(); ++i) 
    {
      if((i->second->getChildStatus() == 1) && (parent_process->getPID() != i->second->getPID()) 
      && (child->getWaitStatus() == 0))
      {
        list_of_processes_lock_.acquire();
        child = i->second;
        list_of_processes_lock_.release();
        break;
      }
    }
    list_of_processes_lock_.acquire();
    parent_process->setWaitStatus(1);
    size_t process_state = child->getProcessState();
    return_pid = child->getPID();
    list_of_processes_lock_.release();
    while (parent_process->getWaitStatus() && !child->getWaitStatus() && child->getProcessState() == 2) 
    {
      debug(DBEK, "in nw hile  %ld\nSTATES parent: %d, child %d\nID parent: %ld, child %ld\nCHILD parent: %d, child %d\nWAIT parent: %d, child %d\n",
       arg1, parent_process->getProcessState(), child->getProcessState(),
       parent_process->getPID(), child->getPID(), parent_process->getChildStatus(), child->getChildStatus(),
       parent_process->getWaitStatus(), child->getWaitStatus());
      Scheduler::instance()->yield();
      if(process_state != child->getProcessState() || child->getProcessState() == 0)
      {
        list_of_processes_lock_.acquire();
        parent_process->setWaitStatus(0);
        list_of_processes_lock_.release();
      }
    }
   // auto search_parent = list.find(callingthread->getParentProcess()->getPID());
  }

  else if((long int) arg1 < -1) //  any child process whose process group ID is equal to the absolute value of pid. 
  {
    debug(DBEK, "arg1 smaller -1\n");
  }
  else if((long int) arg1 == 0) // any child process whose process group ID is equal to that of the calling process. 
  {
    debug(DBEK, "arg1 equals 0\n");
  }
  else //   something went wrong
  {
    debug(DBEK, "we have an error somewhere, process %ld\n", arg1);
    //list_of_processes_lock_.release();
    return -1;
  } 
  if(arg2 != 0)
    debug(DBEK, "arg2 different 0, process %ld\n", arg1);
  if(arg3 > 0) 
    debug(DBEK, "arg3 bigger 0, process %ld\n", arg1);

  // for printing the elements of the map
  //ustl::map<size_t, UserProcess*>::iterator i;
  //for (i = list.begin(); i != list.end(); ++i) 
   // debug(DBEK, "element %ld\n", i->first);
  //list_of_processes_lock_.release();
  return return_pid;
}

// 49.6%
// 53.4%