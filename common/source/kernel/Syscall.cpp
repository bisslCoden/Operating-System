#include "offsets.h"
#include "Syscall.h"
#include "syscall-definitions.h"
#include "Terminal.h"
#include "debug_bochs.h"
#include "VfsSyscall.h"
#include "UserProcess.h"
#include "ProcessRegistry.h"
#include "File.h"


#define callingThread (UserThread*) currentThread 


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
  UserThread* caller = callingThread;
  if (syscall_number == sc_pthread_exit)
  {
    goto after;
  }

  caller->lockFlagMutex();
  if (caller->getflags()->cancelreq && (caller->getflags()->cancelable == PTHREAD_CANCEL_ENABLE))
  {
    caller->unlockFlagMutex();
    Syscall::pthread_exit(PTHREAD_CANCELED);
  }
  caller->unlockFlagMutex();
 

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
    case sc_waitpid:
      return_value = wait_pid((size_t) arg1, (size_t*) arg2, arg3);
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
    default:
      kprintf("Syscall::syscall_exception: Unimplemented Syscall Number %zd\n", syscall_number);
  }
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
  ((UserThread*)currentThread)->getParentProcess()->exit(exit_code);
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
  if(!checkAdressValid((void*) thread))
    exit(999);
  // add as much parameter checking as possible and return -1

  if(currentThread->getType() != Thread::TYPE::USER_THREAD)
    assert(false && "how tf did that happen?");

  // calling thread creation and settind return value to user's pthread_t thread adress 
  size_t tid = ((UserThread*)currentThread)->getParentProcess()->createNewThread(start_routine, arg, wrapper);
  debug(SYSCALL, "Syscall::pthread_create returns thread with tid: [%ld]\n", tid);
  *(size_t*)thread = tid;

  if(tid == 0)
    return -1;

  return 0;
}


void Syscall::pthread_exit(void* value)
{
  UserThread* callingthread = (UserThread*)currentThread;
  debug(X_USERTHREAD, "entering p-exit for thread [%ld]\n", callingthread->getTID());
  size_t my_tid = callingthread->getTID();
  
  callingthread->getParentProcess()->lockThreadMutex();
  
  if (callingthread->getParentProcess()->findInThreadList(my_tid) != 0x00)
  {
    callingthread->lockJoin();
    if (callingthread->getJoiner() != -1)
    {
      UserThread* to_be_signaled;
      if((to_be_signaled = (UserThread*)callingthread->getParentProcess()->findInThreadList(callingthread->getJoiner()))!= 0x00)
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
    callingthread->unlockJoin();
    callingthread->getParentProcess()->addToRetvalList(callingthread->getTID(), value);
    callingthread->getParentProcess()->removeFromThreadList(callingthread);
    callingthread->getParentProcess()->unLockThreadMutex();
    currentThread->kill();
  }
  else
  {
    debug(X_USERTHREAD, "[%ld]: Hmm... was already killed\n", my_tid);
    callingthread->getParentProcess()->unLockThreadMutex();
  }
  debug(X_USERTHREAD, "finishing p-exit for thread [%ld]\n", callingthread->getTID());
  return;

}

int32 Syscall::pthread_setcancelstate(int32 state, int32 *oldstate)
{
  if(!checkAdressValid((void*) oldstate))
    exit(999);
  if((state != 1) && (state != 0))
  {
    debug(X_USERTHREAD, "got a wrong arg as cancelstate!\n");
    return -1;
  }
  UserThread* callingthread = (UserThread*)currentThread;
  callingthread->lockFlagMutex();
  *oldstate = callingthread->getflags()->cancelable;
  callingthread->setCancelState(state);
  //debug(X_USERTHREAD, "[%ld]: just changed my state to: %d!\n", callingthread->getTID(), state);
  callingthread->unlockFlagMutex();


  // if(state == PTHREAD_CANCEL_ENABLE && callingthread->getflags()->cancelreq)
  // {
  //   callingthread->unlockFlagMutex();
  //   pthread_exit(PTHREAD_CANCELED);
  // }
  return 0;
};
int32 Syscall::pthread_setcanceltype(int32 type, int32 *oldtype){
  if(!checkAdressValid((void*) oldtype))
    exit(999);
   if((type != 1) && (type != 0))
  {
    debug(X_USERTHREAD, "got a wrong arg as canceltype!\n");
    return -1;
  }
  UserThread* callingthread = (UserThread*)currentThread;
  callingthread->lockFlagMutex();
  *oldtype = callingthread->getflags()->cancelable;
  callingthread->setCancelType(type);
  callingthread->unlockFlagMutex();
  //debug(X_USERTHREAD, "[%ld]: just changed my type from %d to: %d!\n" ,callingthread->getTID(), *oldtype, type);
  //debug(X_USERTHREAD, "[%ld]: now my flags are: state: %d type: %d\n", callingthread->getTID(),
  //callingthread->getflags()->cancelable, callingthread->getflags()->deferred);
  // if(type == PTHREAD_CANCEL_ASYNCHRONOUS && callingthread->getflags()->cancelreq
  // && callingthread->getflags()->cancelable == PTHREAD_CANCEL_ENABLE)
  // {
  //   callingthread->unlockFlagMutex();
  //   pthread_exit(PTHREAD_CANCELED);
  // }
  return 0;
}


      //  EDEADLK
      //         A  deadlock  was  detected (e.g., two threads tried to join with
      //         each other); or thread specifies the calling thread.

      //  EINVAL thread is not a joinable thread.

      //  EINVAL Another thread is already waiting to join with this thread.

      //  ESRCH  No thread with the ID thread could be found.
  //reminder how to fix existing bug with dying threads: make cond var in Userprocess not in the corresponding threadd!!!!!!

size_t Syscall::pthread_join(size_t thread, void** value_ptr)
{
  if(!checkAdressValid((void*) value_ptr))
    exit(999);
  UserThread* callingthread = (UserThread*)currentThread;
  void* retval;

  //catch edgecases
  if(thread == callingthread->getTID())
    return -1;
  //debug(X_USERTHREAD, "[%ld]trying to join [%ld]; before threadlock\n", callingthread->getTID(), thread);
  callingthread->getParentProcess()->lockThreadMutex();
  //debug(X_USERTHREAD, "[%ld]trying to join [%ld]; afeter threadlock\n", callingthread->getTID(), thread);

  
  debug(X_USERTHREAD, "[%ld]trying to join [%ld]; before join and retval\n", callingthread->getTID(), thread);

  if (callingthread->getParentProcess()->findInThreadList(thread) != 0x00)
  {
    UserThread* join_victim = (UserThread*) callingthread->getParentProcess()->findInThreadList(thread);
    join_victim->lockJoin();
    callingthread->lockJoin();
    if (join_victim->getJoiner() != -1 || callingthread->getJoiner() == (int32) thread)
    {
      debug(X_USERTHREAD, "Deadlock in join detected! either thread [%ld] is already joined by another or tries to join each other with thread [%ld]!\n",
       thread, callingthread->getTID());
      join_victim->unlockJoin();
      callingthread->unlockJoin();
      callingthread->getParentProcess()->unLockThreadMutex();
      return -1;
    }
    join_victim->setJoiner((int32) callingthread->getTID());
    join_victim->unlockJoin();
    callingthread->getParentProcess()->unLockThreadMutex();

    debug(X_USERTHREAD, "thread [%ld] now trying to wait\n", callingthread->getTID());
    callingthread->waitJoin(true);
    debug(X_USERTHREAD, "thread [%ld] got OUT of wait\n", callingthread->getTID());
    
    if (!callingthread->getParentProcess()->getRetVal(thread, &retval))
      debug(X_USERTHREAD, "Waited for thread to finish and didnt find any retval??? this should never happen.\n");
    callingthread->unlockJoin();
  }
  else if (!callingthread->getParentProcess()->getRetVal(thread, &retval))
  {
    debug(X_USERTHREAD, "[%ld]trying to join [%ld]; after retvallock AND thread didnt exist or was joined alreafy\n", callingthread->getTID(), thread);
    //join_victim->unlockJoin();
    callingthread->getParentProcess()->unLockThreadMutex();
    return -1;
  }
  else
  {
    callingthread->getParentProcess()->unLockThreadMutex();
  }

  *value_ptr = retval;
  debug(X_USERTHREAD, "[%ld]MANAGED to join [%ld]\n", callingthread->getTID(), thread);
  return 0;
}


int32 Syscall::pthread_cancel(size_t thread)
{
  UserThread* current = callingThread;
  current->getParentProcess()->lockThreadMutex();
  UserThread* cancel_victim;
  debug(X_USERTHREAD, "[%ld]: I will search for %ld\n", current->getTID(), thread);
 
  if((cancel_victim = (UserThread*) current->getParentProcess()->findInThreadList(thread)) == 0x00)
  {
    debug(X_USERTHREAD, "Thread [%ld] is tryin' to cancel [%ld] BUT THAT ONE IS DEAD ALREADY!\n",current->getTID(), thread);
    current->getParentProcess()->unLockThreadMutex();
    return -1;
  }
  debug(X_USERTHREAD, "Thread [%ld] is tryin' to cancel [%ld]!\n",current->getTID(), cancel_victim->getTID());
  cancel_victim->lockFlagMutex();
  cancel_victim->sendCancelRequest();
  cancel_victim->unlockFlagMutex();
  current->getParentProcess()->unLockThreadMutex();
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
      if((to_be_signaled = (UserThread*)cancel_victim->getParentProcess()->findInThreadList(cancel_victim->getJoiner()))!= 0x00)
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
    cancel_victim->getParentProcess()->addToRetvalList(cancel_victim->getTID(), PTHREAD_CANCELED);
    cancel_victim->getParentProcess()->removeFromThreadList(cancel_victim);
    cancel_victim->unlockJoin();
    cancel_victim->unlockFlagMutex();
    current->getParentProcess()->unLockThreadMutex();
    cancel_victim->kill();
    return 0;
  }
  else
    debug(X_USERTHREAD, "Thread [%ld]: could not kill bc its flags were state: %d type: %d!\n", current->getTID(), cancel_victim->getflags()->cancelable,
    cancel_victim->getflags()->deferred);
*/


}

int Syscall::fork()
{
  debug(SYSCALL, "Calling Syscall Fork!\n");
  return ProcessRegistry::instance()->processFork();
}

size_t Syscall::wait_pid(size_t arg1, size_t* arg2, size_t arg3)
{
  debug(SYSCALL, "Calling Syscall waitpid!\n");
  return ProcessRegistry::instance()->waitPid(arg1, arg2, arg3);
}

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


