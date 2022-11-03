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
  debug(WAITPID, "id: %ld\n", parent_process->getPID());
  int return_pid = 0;
  if((long int) arg1 > 0) // any specifed process
  {
    debug(WAITPID, "arg1 greater 0, process %ld\n", arg1);
    list_of_processes_lock_.acquire();
    ustl::map<size_t, UserProcess*> list = ProcessRegistry::getProcessList();
    auto search_child = list.find(arg1);
    list_of_processes_lock_.release();
    if (search_child != list.end())
    {
      list_of_processes_lock_.acquire();
      parent_process->setWaitStatus(1);
      size_t process_state = search_child->second->getProcessState();
      return_pid = search_child->second->getPID();
      list_of_processes_lock_.release();
      while (parent_process->getWaitStatus() && !search_child->second->getWaitStatus() 
      && search_child->second->getProcessState() == 2) 
      {
        Scheduler::instance()->yield();
        if(process_state != search_child->second->getProcessState() || search_child->second->getProcessState() == 0)
        {
          list_of_processes_lock_.acquire();
          parent_process->setWaitStatus(0);
          list_of_processes_lock_.release();
        }
      }
    }
    else
    {
      debug(WAITPID, "Not found, process %ld\n", arg1);
      //list_of_processes_lock_.release();
      return -1;
    }
  }
  else if((long int) arg1 == -1) // any child process.
  {
    debug(WAITPID, "arg1 equals -1, process %ld\n", arg1);
    list_of_processes_lock_.acquire();
    ustl::map<size_t, UserProcess*> list;
    list = ProcessRegistry::getProcessList();
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
    debug(WAITPID, "before while \n");
    while (parent_process->getWaitStatus() && !child->getWaitStatus() && child->getProcessState() == 2) 
    {
      debug(WAITPID, "in while  %ld\nSTATES parent: %d, child %d\nID parent: %ld, child %ld\nCHILD parent: %d, child %d\nWAIT parent: %d, child %d\n",
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
    /*If wstatus is not NULL, wait() and waitpid() store status information in the int  to  which  it
       points.  This integer can be inspected with the following macros (which take the integer itself
       as an argument, not a pointer to it, as is done in wait() and waitpid()!):

       WIFEXITED(wstatus)
              returns true if the child terminated normally, that is, by calling exit(3) or  _exit(2),
              or by returning from main().

       WEXITSTATUS(wstatus)
              returns  the exit status of the child.  This consists of the least significant 8 bits of
              the status argument that the child specified in a call to exit(3) or _exit(2) or as  the
              argument for a return statement in main().  This macro should be employed only if WIFEX‐
              ITED returned true.

       WIFSIGNALED(wstatus)
              returns true if the child process was terminated by a signal.

       WTERMSIG(wstatus)
              returns the number of the signal that caused the child process to terminate.  This macro
              should be employed only if WIFSIGNALED returned true.

       WCOREDUMP(wstatus)
              returns  true if the child produced a core dump (see core(5)).  This macro should be em‐
              ployed only if WIFSIGNALED returned true.

              This macro is not specified in POSIX.1-2001 and is not available on some UNIX  implemen‐
              tations (e.g., AIX, SunOS).  Therefore, enclose its use inside #ifdef WCOREDUMP ... #en‐
              dif.

       WIFSTOPPED(wstatus)
              returns true if the child process was stopped by delivery of a signal; this is  possible
              only  if  the  call  was  done  using  WUNTRACED  or when the child is being traced (see
              ptrace(2)).

       WSTOPSIG(wstatus)
              returns the number of the signal which caused the child to stop.  This macro  should  be
              employed only if WIFSTOPPED returned true.

       WIFCONTINUED(wstatus)
              (since  Linux  2.6.10) returns true if the child process was resumed by delivery of SIG‐
              CONT.
*/
  if(arg2 != 0)
  {
    debug(WAITPID, "arg2 different 0, process %ld\n", arg1);
  }
  /*The value of options is an OR of zero or more of the following constants:

       WNOHANG
              return immediately if no child has exited.

       WUNTRACED
              also  return  if  a child has stopped (but not traced via ptrace(2)).  Status for traced
              children which have stopped is provided even if this option is not specified.

       WCONTINUED (since Linux 2.6.10)
              also return if a stopped child has been resumed by delivery of SIGCONT.
*/
  if(arg3 > 0) 
  {
    debug(WAITPID, "arg3 bigger 0, process %ld\n", arg1);
  }
  return return_pid;
}

// 49.6%
// 53.4%