#include "offsets.h"
#include "Syscall.h"
#include "syscall-definitions.h"
#include "Terminal.h"
#include "debug_bochs.h"
#include "VfsSyscall.h"
#include "UserProcess.h"
#include "ProcessRegistry.h"
#include "File.h"
#include "../../../userspace/libc/include/time.h"

typedef struct threadattribute
{
    int initialized_;
    int detach_state_;
    size_t guard_size_; 
    size_t* stackaddress_;
    size_t  stacksize_;
} pthread_attr_t;


size_t Syscall::syscallException(size_t syscall_number, size_t arg1, size_t arg2, size_t arg3, size_t arg4, size_t arg5)
{
  size_t return_value = 0;


  if ((syscall_number != sc_sched_yield) && (syscall_number != sc_outline)) // no debug print because these might occur very often
  {
    debug(SYSCALL, "Syscall %zd called with arguments %zd(=%zx) %zd(=%zx) %zd(=%zx) %zd(=%zx) %zd(=%zx) by thread [%ld]\n",
          syscall_number, arg1, arg1, arg2, arg2, arg3, arg3, arg4, arg4, arg5, arg5, currentThread->getTID());
  }
  //UserProcess::getRandomPageOffset();
  //call exit with phthread cancelled if the thread can be cancelled
  if (syscall_number == sc_pthread_exit)
  {
    goto after;
  }

  currentUserThread->lockFlagMutex();
  if (currentUserThread->getflags()->cancelreq && (currentUserThread->getflags()->cancelable == PTHREAD_CANCEL_ENABLE))
  {
    currentUserThread->unlockFlagMutex();
    Syscall::pthread_exit(PTHREAD_CANCELED);
  }
  currentUserThread->unlockFlagMutex();
 

after:
  switch (syscall_number)
  {
    case sc_pthread_create:
      return_value = pthread_create(arg1, arg2, arg3, arg4, arg5);
      break;
    case sc_pthread_cancel:
      return_value = pthread_cancel(arg1);
      break;
    case sc_pthread_exit:
      pthread_exit((void*)arg1);
      break; 
    case sc_pthread_join:
      return_value = pthread_join(arg1, (void**) arg2);
      break;
    case sc_sched_yield:
      Scheduler::instance()->yield();
      break;
    case sc_createprocess:
      return_value = createprocess(arg1, arg2);
      break;
    case sc_exit:
      exit(arg1);
      break;
    case sc_write:
      return_value = write(arg1, arg2, arg3);
      break;
    case sc_read:
      return_value = read(arg1, arg2, arg3);
      break;
    case sc_open:
      return_value = open(arg1, arg2);
      break;
    case sc_close:
      return_value = close(arg1);
      break;
    case sc_outline:
      outline(arg1, arg2);
      break;
    case sc_fork:
      return_value = fork();
      break;
    case sc_execv:
      return_value = execv((const char *)arg1, (char* const*)arg2);
      break;
    case sc_waitpid:
      return_value = wait_pid((size_t) arg1, (size_t*) arg2, arg3);
      break;
    case sc_getpid:
      return_value = get_pid();
      break;
    case sc_sleep:
      return_value = sleep(arg1);
      break;
    case sc_clock:
      return_value = clock();
      break;
    case sc_trace:
      trace();
      break;
    case sc_pseudols:
      pseudols((const char*) arg1, (char*) arg2, arg3);
      break;
    case sc_pthread_setcancelstate:
      return_value = pthread_setcancelstate((int) arg1, (int*) arg2);
      break;
    case sc_pthread_setcanceltype:
      return_value = pthread_setcanceltype((int) arg1, (int*) arg2);
      break;
    case sc_pthread_attr_init:
      return_value = pthread_attr_init((size_t**) arg1, (size_t*) arg2);
      break;
    case sc_pthread_detach:
      return_value = pthread_detach(arg1);
      break;
    case sc_pthread_self:
      return_value = pthread_self();
      break;
    default:
      kprintf("Syscall::syscall_exception: Unimplemented Syscall Number %zd\n", syscall_number);
  }

  currentUserThread->lockFlagMutex();
  if (currentUserThread->getflags()->cancelreq && (currentUserThread->getflags()->cancelable == PTHREAD_CANCEL_ENABLE))
  {
    currentUserThread->unlockFlagMutex();
    Syscall::pthread_exit(PTHREAD_CANCELED);
  }
  currentUserThread->unlockFlagMutex();

  return return_value;
}

bool checkAdressValid(void* addr)
{
  if ((long long unsigned)addr > USER_BREAK)
  {
    kprintf("what are you looking for in the kernel huh? - BAD USER!\n");
    return false;
  }
  return true;
}

void Syscall::pseudols(const char *pathname, char *buffer, size_t size)
{
  if(buffer && ((size_t)buffer >= USER_BREAK || (size_t)buffer + size > USER_BREAK))
    return;
  if((size_t)pathname >= USER_BREAK)
    return;
  VfsSyscall::readdir(pathname, buffer, size);
}

void Syscall::exit(size_t exit_code)
{
  debug(SYSCALL, "Syscall::EXIT: called in thread [%ld], exit_code: %ld\n", currentThread->getTID(), exit_code);
  currentUserThread->getProcess()->exit(exit_code);
  debug(USERPROCESS, "exit sucessfuly finished!\n");
  // currentThread->kill();
}

size_t Syscall::write(size_t fd, pointer buffer, size_t size)
{
  //WARNING: this might fail if Kernel PageFaults are not handled
  if ((buffer >= USER_BREAK) || (buffer + size > USER_BREAK))
  {
    return -1U;
  }

  size_t num_written = 0;

  if (fd == fd_stdout) //stdout
  {
    debug(SYSCALL, "Syscall::write: %.*s\n", (int)size, (char*) buffer);
    kprintf("%.*s", (int)size, (char*) buffer);
    num_written = size;
  }
  else
  {
    num_written = VfsSyscall::write(fd, (char*) buffer, size);
  }
  return num_written;
}

size_t Syscall::read(size_t fd, pointer buffer, size_t count)
{
  if ((buffer >= USER_BREAK) || (buffer + count > USER_BREAK))
  {
    return -1U;
  }

  size_t num_read = 0;

  if (fd == fd_stdin)
  {
    //this doesn't! terminate a string with \0, gotta do that yourself
    num_read = currentThread->getTerminal()->readLine((char*) buffer, count);
    debug(SYSCALL, "Syscall::read: %.*s\n", (int)num_read, (char*) buffer);
  }
  else
  {
    num_read = VfsSyscall::read(fd, (char*) buffer, count);
  }
  return num_read;
}

size_t Syscall::close(size_t fd)
{
  return VfsSyscall::close(fd);
}

size_t Syscall::open(size_t path, size_t flags)
{
  debug(SYSCALL, "open called!\n");
  if (path >= USER_BREAK)
  {
    return -1U;
  }
  return VfsSyscall::open((char*) path, flags);
}

void Syscall::outline(size_t port, pointer text)
{
  //WARNING: this might fail if Kernel PageFaults are not handled
  if (text >= USER_BREAK)
  {
    return;
  }
  if (port == 0xe9) // debug port
  {
    writeLine2Bochs((const char*) text);
  }
}

size_t Syscall::createprocess(size_t path, size_t sleep)
{
  // THIS METHOD IS FOR TESTING PURPOSES ONLY!
  // AVOID USING IT AS SOON AS YOU HAVE AN ALTERNATIVE!

  // parameter check begin
  if (path >= USER_BREAK)
  {
    return -1U;
  }
  debug(SYSCALL, "Syscall::createprocess: path:%s sleep:%zd\n", (char*) path, sleep);
  ssize_t fd = VfsSyscall::open((const char*) path, O_RDONLY);
  if (fd == -1)
  {
    return -1U;
  }
  VfsSyscall::close(fd);
  // parameter check end

  size_t process_count = ProcessRegistry::instance()->processCount();
  ProcessRegistry::instance()->createProcess((const char*) path);
  if (sleep)
  {
    while (ProcessRegistry::instance()->processCount() > process_count) // please note that this will fail ;)
    {
      Scheduler::instance()->yield();
    }
  }
  return 0;
}

void Syscall::trace()
{
  currentThread->printBacktrace();
}

size_t Syscall::pthread_create(size_t thread, size_t attr, size_t start_routine, size_t arg, size_t wrapper)
{
  debug(SYSCALL, "Syscall::pthread_create(thread = %lx, attr = %lx, start_routine = %lx, arg = %lx) called\n", thread, attr, start_routine, arg);
  // add as much parameter checking as possible and return -1

  assert(currentThread->getType() != Thread::TYPE::USER_THREAD && "how tf did that happen?");

  currentUserThread->getProcess()->lockKill();
  if(currentUserThread->getProcess()->checkKill())
  {
    currentUserThread->getProcess()->unlockKill();
    return -1;
  };
  currentUserThread->getProcess()->unlockKill();

  // could be dangerous but we have NO locks here...
  
  int joinstate = PTHREAD_CREATE_JOINABLE;
  if(attr >= 0x1000)
  {
    if (((pthread_attr_t*) attr)->detach_state_ == PTHREAD_CREATE_DETACHED)
      joinstate = PTHREAD_CREATE_DETACHED;
    else if (((pthread_attr_t*) attr)->detach_state_ != PTHREAD_CREATE_JOINABLE)
      return -1;
  }
  
  UserThread* newthread = currentUserThread->getParentProcess()->createNewThread(start_routine, arg, wrapper, joinstate);
  debug(SYSCALL, "Syscall::pthread_create returns thread with tid: [%ld]\n", newthread->getTID());

  if(newthread != 0)
  {
    currentUserThread->getProcess()->lockKill();
    if(currentUserThread->getProcess()->checkKill())
    {
      newthread->reDirectToDeath();
      currentUserThread->getProcess()->unlockKill();
      return -1;
    };
    currentUserThread->getProcess()->unlockKill();

    *(size_t*)thread = newthread->getTID();
    return 0;
  }
  return -1;
}


void Syscall::pthread_exit(void* value)
{
  debug(X_USERTHREAD, "entering p-exit for thread [%ld]\n", currentUserThread->getTID());
  size_t my_tid = currentUserThread->getTID();
  
  currentUserThread->getProcess()->lockThreadMutex();
  
  if (currentUserThread->getProcess()->findInThreadList(my_tid) != 0x00)
  {
    if (!currentUserThread->getProcess()->checkRetVal(currentThread))
      currentUserThread->getProcess()->lockRetVal();
    
    if (currentUserThread->getJoiner() != 0)
    {
      UserThread* to_be_signaled;
      if((to_be_signaled = (UserThread*)currentUserThread->getProcess()->findInThreadList(currentUserThread->getJoiner()->getTID()))!= 0x00)
      {
        to_be_signaled->signalJoin();
      }
      else
      {
        //this will happen if a forked thread calls it
        debug(X_USERTHREAD, "Veeery strange! Joiner is not -1 but also not in my process... I must be a forked thread!\n");
      }
    }
    currentUserThread->lockFlagMutex();
    if (currentUserThread->getflags()->joinable == PTHREAD_CREATE_DETACHED)
    {
      currentUserThread->unlockFlagMutex();
      currentUserThread->getProcess()->unlockRetVal();
    }
    else
    {  
      currentUserThread->unlockFlagMutex();
      currentUserThread->getParentProcess()->addToRetvalList(currentUserThread->getTID(), value);
    } 
    
    currentUserThread->getParentProcess()->removeFromThreadList(currentUserThread);
    currentUserThread->getParentProcess()->unLockThreadMutex();
    currentUserThread->getParentProcess()->removeFromOffsetList(currentUserThread->getStackInfo().page_offset_);
    //experimentaaal: free my pages on my own!
    currentThread->kill();
  }
  else
  {
    debug(X_USERTHREAD, "[%ld]: Hmm... was already killed\n", my_tid);
    currentUserThread->getProcess()->unLockThreadMutex();
  }
  debug(X_USERTHREAD, "finishing p-exit for thread [%ld]\n", currentUserThread->getTID());
  return;
}

int32 Syscall::pthread_setcancelstate(int32 state, int32 *oldstate)
{
  int old;
  if((state != PTHREAD_CANCEL_ENABLE) && (state != PTHREAD_CANCEL_DISABLE))
  {
    debug(X_USERTHREAD, "got a wrong arg as cancelstate!\n");
    return -1;
  }
  currentUserThread->lockFlagMutex();
  currentUserThread->setCancelState(state);
  old = currentUserThread->getflags()->cancelable;
  currentUserThread->unlockFlagMutex();
  //debug(X_USERTHREAD, "[%ld]: just changed my state to: %d!\n", currentUserThread->getTID(), state);
  if (oldstate >= (int*) 0x1000 && checkAdressValid(oldstate))
    *oldstate = old;

  return 0;
};

int32 Syscall::pthread_setcanceltype(int32 type, int32 *oldtype){
  int old;
   if((type != PTHREAD_CANCEL_ASYNCHRONOUS) && (type != PTHREAD_CANCEL_DEFERRED))
  {
    debug(X_USERTHREAD, "got a wrong arg as canceltype!\n");
    return -1;
  }
  currentUserThread->lockFlagMutex();
  old = currentUserThread->getflags()->deferred;
  currentUserThread->setCancelType(type);
  currentUserThread->unlockFlagMutex();

  if (oldtype >= (int*) 0x1000 && checkAdressValid(oldtype))
    *oldtype = old;

  return 0;
}


      //  EDEADLK
      //         A  deadlock  was  detected (e.g., two threads tried to join with
      //         each other); or thread specifies the calling thread.

      //  EINVAL thread is not a joinable thread.

      //  EINVAL Another thread is already waiting to join with this thread.

      //  ESRCH  No thread with the ID thread could be found.
  //reminder how to fix existing bug with dying threads: make cond var in Userprocess not in the corresponding threadd!!!!!!
int Syscall::pthread_detach(size_t thread){
  currentUserThread->getParentProcess()->lockThreadMutex();
  UserThread* to_be_detached = 0x00;
  if ((to_be_detached = (UserThread*) currentUserThread->getParentProcess()->findInThreadList(thread)) != 0x00)
  {
    currentUserThread->getProcess()->lockRetVal();
    if (to_be_detached->getJoiner() != 0)
    {
      currentUserThread->getProcess()->unlockRetVal();
      currentUserThread->getParentProcess()->unLockThreadMutex();
      return -1;
    }
    currentUserThread->getProcess()->unlockRetVal();

    to_be_detached->lockFlagMutex();
    to_be_detached->getflags()->joinable = PTHREAD_CREATE_DETACHED;
    to_be_detached->unlockFlagMutex();
    currentUserThread->getParentProcess()->unLockThreadMutex();
  }
  else
  {
    currentUserThread->getParentProcess()->unLockThreadMutex();
    return -1;
  }
  return 0;
}



size_t Syscall::pthread_join(size_t thread, void** value_ptr)
{

  void* retval;
  //catch edgecases
  if(thread == currentUserThread->getTID())
    return -1;

  currentUserThread->getProcess()->lockThreadMutex();
  debug(X_USERTHREAD, "[%ld]trying to join [%ld]; before join and retval\n", currentUserThread->getTID(), thread);

  if (currentUserThread->getProcess()->findInThreadList(thread) != 0x00)
  {
    UserThread* join_victim = (UserThread*) currentUserThread->getParentProcess()->findInThreadList(thread);
    
    join_victim->lockFlagMutex();
    if (join_victim->getflags()->joinable == PTHREAD_CREATE_DETACHED)
    {
      join_victim->unlockFlagMutex();
      currentUserThread->getParentProcess()->unLockThreadMutex();
      return -1;
    }
    join_victim->unlockFlagMutex();
    
    currentUserThread->getProcess()->lockRetVal();
    if (join_victim->getJoiner() != 0 || join_victim->getJoiner() == currentUserThread || currentUserThread->detectCircularJoin(join_victim))
    {
      debug(X_USERTHREAD, "Deadlock in join detected! either thread [%ld] is already joined by another or tries to join each "
      "other with thread [%ld] OR there is a Circular Deadjoin :D!\n",
       thread, currentUserThread->getTID());
      currentUserThread->getProcess()->unlockRetVal();
      currentUserThread->getProcess()->unLockThreadMutex();
      return -1;
    }
    
    join_victim->setJoiner(currentUserThread);
    currentUserThread->getProcess()->unLockThreadMutex();

    //releases retvallock!
    currentUserThread->waitJoin();
    
    //getretval unlocks retvallock :D
    debug(X_USERTHREAD, "woke up to look for [%ld]s retval\n", thread);
    assert(currentUserThread->getProcess()->getRetVal(thread, &retval) && 
    "Waited for thread to finish and didnt find any retval??? this should never happen.\n");
  }
  else if (!currentUserThread->getProcess()->getRetVal(thread, &retval))
  {
    debug(X_USERTHREAD, "[%ld]trying to join [%ld]; after retvallock AND thread didnt exist or was joined alreafy\n", currentUserThread->getTID(), thread);
    //join_victim->unlockJoin();
    currentUserThread->getProcess()->unLockThreadMutex();
    return -1;
  }
  else
  {
    currentUserThread->getProcess()->unLockThreadMutex();
  }

  debug(X_USERTHREAD, "[%ld]MANAGED to join [%ld]\n", currentUserThread->getTID(), thread);
  if (checkAdressValid(value_ptr) && (void*)value_ptr > (void*) 0x1000)
    *value_ptr = retval;
  
  return 0;
}

size_t Syscall::pthread_self(){
  return currentUserThread->getTID();
}



int32 Syscall::pthread_cancel(size_t thread)
{
  UserThread* current = currentUserThread;
  current->getProcess()->lockThreadMutex();
  UserThread* cancel_victim;
  debug(X_USERTHREAD, "[%ld]: I will search for %ld\n", current->getTID(), thread);
 
  if((cancel_victim = (UserThread*) current->getProcess()->findInThreadList(thread)) == 0x00)
  {
    debug(X_USERTHREAD, "Thread [%ld] is tryin' to cancel [%ld] BUT THAT ONE IS DEAD ALREADY!\n",current->getTID(), thread);
    current->getProcess()->unLockThreadMutex();
    return -1;
  }
  debug(X_USERTHREAD, "Thread [%ld] is tryin' to cancel [%ld]!\n",current->getTID(), cancel_victim->getTID());
  cancel_victim->lockFlagMutex();
  cancel_victim->sendCancelRequest();
  cancel_victim->unlockFlagMutex();
  current->getProcess()->unLockThreadMutex();
  return 0;
  //Threadflags* its_flags = cancel_victim->getflags();


/* WE DONT NEED THIS ANYMORE
  if ((its_flags->cancelable == PTHREAD_CANCEL_ENABLE) &&
    its_flags->deferred == PTHREAD_CANCEL_ASYNCHRONOUS) //queue cancellation request
  {
 
    debug(X_USERTHREAD, "Thread [%ld] could be cancelled right away and is now killed!\n", cancel_victim->getTID());
    cancel_victim->lockJoin();
    if (cancel_victim->getJoiner() != -1)
    {
      UserThread* to_be_signaled;
      if((to_be_signaled = (UserThread*)cancel_victim->getProcess()->findInThreadList(cancel_victim->getJoiner()))!= 0x00)
      {
        to_be_signaled->lockJoin();
        to_be_signaled->signalJoin();
        to_be_signaled->unlockJoin();
      }
      else
      {
        debug(X_USERTHREAD, "Veeery strange! Joiner is not -1 but also not in my process?\n");
      }
    }
    cancel_victim->getProcess()->addToRetvalList(cancel_victim->getTID(), PTHREAD_CANCELED);
    cancel_victim->getProcess()->removeFromThreadList(cancel_victim);
    cancel_victim->unlockJoin();
    cancel_victim->unlockFlagMutex();
    current->getProcess()->unLockThreadMutex();
    cancel_victim->kill();
    return 0;
  }
  else
    debug(X_USERTHREAD, "Thread [%ld]: could not kill bc its flags were state: %d type: %d!\n", current->getTID(), cancel_victim->getflags()->cancelable,
    cancel_victim->getflags()->deferred);
*/
}

//this should never be called when holding any locks
int32 Syscall::pthread_attr_init(size_t** stackaddr, size_t* stacksize)
{
  *stackaddr = (size_t*)currentUserThread->getStackInfo().userstack_start_;
  *stacksize = STACK_SIZE_IN_PAGES * PAGE_SIZE;
  return 0;
}


int Syscall::fork()
{
  debug(SYSCALL, "Calling Syscall Fork!\n");
  return ProcessRegistry::instance()->processFork();
}

int Syscall::execv(const char * path, char *const argv[])
{
  debug(SYSCALL, "execv() checking path.\n");
  if(!isExecPathValid(path))
    return -1;

  // call execv with/without args
  int ret = 0;
  if(argv) // if(argv) for arguments, if(false) fork woking exec D:
    ret = ProcessRegistry::instance()->execv(path, argv);
  else
    ret = ProcessRegistry::instance()->execv(path);

  debug(SYSCALL, "execProcess returned %d\n", ret);
  return ret;
}

bool Syscall::isExecPathValid(const char* path)
{
  // path ptr valid?
  if(!(path && (size_t)path < USER_BREAK))
    return false;

  for(size_t i = 0; likely(path[i]); i++)
  {
    // pointer to char invalid
    if((size_t)(path + i) >= USER_BREAK) 
      return false;
    // char too long
    if(i > EXECV_MAX_PATH_LEN)
      return false;
  }
  debug(SYSCALL, "execv(): isPathValid(): path seems fine: %s\n", path);
  return true;
}
size_t Syscall::wait_pid(size_t arg1, size_t* arg2, size_t arg3)
{
  UserThread* callingthread = (UserThread*)currentThread;
  debug(SYSCALL, "Calling Syscall waitpid!\n");
  return ProcessRegistry::instance()->waitPid(arg1, arg2, arg3, callingthread->getParentProcess());
}

int Syscall::get_pid()
{
  UserThread* callingthread = (UserThread*)currentThread;
  debug(SYSCALL, "Calling Syscall getpid!\n");
  return callingthread->getParentProcess()->getPID();
}

// wake up when getRDTSC == rdtsc_now + (cpu cycles) seconds
  // while(getRDTSC != rdtsc_to_wake)
  // yield
  //
  // ms = mili second 1s/1000 
  // 54 ms = 0.054 s happens a tick
  // CLOCKS_PER_SECOND = 1000000

  // frequency is needed, with that we multiply clock_per_sec
unsigned int Syscall::sleep(unsigned int seconds)
{
  debug(SLEEP, "Sleep system call started\n");
  uint64_t rdtsc_now = Scheduler::instance()->getRDTSC() * 10;
  uint64_t time_to_wake = (seconds * 182) * Scheduler::instance()->getRDTSCdiff() + rdtsc_now;
  debug(SLEEP, "rdtsc_now:    %ld\n", rdtsc_now);
  debug(SLEEP, "time_to_wake: %ld\n", time_to_wake);
  //debug(SLEEP, "time_to_wake: %ld, the getRDTSC: %ld, and the Frequency: %ld\n", time_to_wake, Scheduler::instance()->getRDTSC()/(CLOCKS_PER_SEC * 20 ), Scheduler::instance()->getFrequency());
  while(time_to_wake > Scheduler::instance()->getRDTSC() * 10)
  {
    debug(SLEEP, "rdtsc_now:    %ld\n",  Scheduler::instance()->getRDTSC() * 10);
    
    debug(SLEEP, "time_to_wake: %ld\n", time_to_wake);
    
    //debug(SLEEP, "time_to_wake: %ld, the getRDTSC: %ld, and the Frequency: %ld\n", time_to_wake,
     //Scheduler::instance()->getRDTSC()/(CLOCKS_PER_SEC * 20 ),
     //Scheduler::instance()->getFrequency());
    Scheduler::instance()->yield();
  }
  return 0;
}

// 71.6 % 
// 75.4 %


// rdtsc now - rdtsc at program start
// but thread can sleep or yield, so then it doesn't count
// we need to increment the ticks variable for every thread of the process
// then use the number of ticks to get the seconds
// we know how many clocks(cycles i think) happen per second
// we get the number of cycles
size_t Syscall::clock()
{
  UserThread* thread = (UserThread*) currentThread;
  return thread->getParentProcess()->getDuaration();
}

// commented out bc testing
// The clock() function returns an approximation of processor time used by the program
/*size_t Syscall::clock()
{
  UserThread* thread = (UserThread*) currentThread;
  return getRDTSC() - thread->getParentProcess()->getDuaration();

  //else
  //  return (clock_t) -1;

  //uint32 new_ticks = Scheduler::instance()->getTicks(); 
  //size_t reuturn_d_ticks = return_ / new_ticks;
  //size_t return_final = reuturn_d_ticks/1000000;
  //CLOCKS_PER_SECOND * (RUNNING TIME OF WHATEVER)
}*/

/*size_t Syscall::getRDTSC()
{
  size_t firstbits;
  size_t lastbits; 
  asm volatile("rdtsc \n\t" : "=a"(lastbits), "=d"(firstbits));
  //debug(USERPROCESS,"read %ld from tsc and MAX STACKS btw is %lld offset is %ld!!\n", rand, MAX_STACKS, page_offset);
  size_t return_ = firstbits << 32 | lastbits;
  return return_;
}*/


/*size_t Syscall::wait_pid(size_t arg1, size_t* arg2, size_t arg3)
{
  int number = 10;
  if((long int) arg1 < -1) //  any child process whose process group ID is equal to the absolute value of pid. 
  {
    debug(DBEK, "arg1 smaller -1\n");
  }
  else if((long int) arg1 == -1) // any child process.
  {
    debug(DBEK, "arg1 equals -1\n");
  } 
  else if((long int) arg1 == 0) // any child process whose process group ID is equal to that of the calling process. 
  {
    debug(DBEK, "arg1 equals 0\n");
  }
  else if((long int) arg1 > 0) // any specifed process
  {
    debug(DBEK, "arg1 greater 0\n");
  }  
  else //   something went wrong
  {
    debug(DBEK, "we have an error somewhere\n");
  } 
  if(arg2 != 0)
  {
    debug(DBEK, "arg2 different 0\n");
  }
  if(arg3 > 0) 
  {
    debug(DBEK, "arg3 bigger 0\n");
  }
  ustl::map<size_t, UserProcess*> list;
  list = ProcessRegistry::getProcessList();
  auto search = list.find(arg1);
  if (search != list.end())
  {
    debug(DBEK, "Found\n");
  }
  else
  {
    debug(DBEK, "Not found\n");
  }
  //debug(DBEK, "%ld\n\n\n\n\n", search->first);
  return number;
}*/


