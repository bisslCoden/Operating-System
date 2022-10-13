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
    debug(SYSCALL, "Syscall %zd called with arguments %zd(=%zx) %zd(=%zx) %zd(=%zx) %zd(=%zx) %zd(=%zx)\n",
          syscall_number, arg1, arg1, arg2, arg2, arg3, arg3, arg4, arg4, arg5, arg5);
  }

  //call exit with phthread cancelled if the thread can be cancelled
  UserThread* caller = callingThread;
  caller->lockFlagMutex();
  if (caller->getflags()->cancelreq && caller->getflags()->cancelable)
  {
    caller->unlockFlagMutex();
    Syscall::pthread_exit(PTHREAD_CANCELED);
  }
  caller->unlockFlagMutex();

  

  switch (syscall_number)
  {
    case sc_pthread_create:
      return_value = pthread_create(arg1, arg2, arg3, arg4);
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

size_t Syscall::pthread_create(size_t thread, size_t attr, size_t start_routine, size_t arg)
{
  debug(SYSCALL, "Syscall::pthread_create(thread = %lx, attr = %lx, start_routine = %lx, arg = %lx) called\n", thread, attr, start_routine, arg);

  // add as much parameter checking as possible and return -1

  if(currentThread->getType() != Thread::TYPE::USER_THREAD)
    assert(false && "how tf did that happen?");

  // calling thread creation and settind return value to user's pthread_t thread adress 
  size_t tid = ((UserThread*)currentThread)->getParentProcess()->createNewThread(start_routine);
  debug(SYSCALL, "Syscall::pthread_create returns thread with tid: [%ld]\n", tid);
  *(size_t*)thread = tid;

  if(tid == 0)
    return -1;

  return 0;
}

void Syscall::pthread_exit(void* value)
{
  UserThread* callingthread = (UserThread*)currentThread;
  size_t my_tid = callingthread->getTID();
  
  callingthread->getParentProcess()->lockThreadMutex();
  
  if(!callingthread->getParentProcess()->addToRetvalList(my_tid, value))
    debug(USERPROCESS, "UserThread retval already in list: This should already have been thrown!\n");
  if (callingthread->getParentProcess()->findInThreadList(my_tid) != 0x00)
  {
    debug(X_USERTHREAD, "[%ld]: Killing myself \n", my_tid);
    callingthread->getParentProcess()->unLockThreadMutex();
    currentThread->kill();
  }
  else
  {
    debug(X_USERTHREAD, "[%ld]: Hmm... was already killed\n", my_tid);
    callingthread->getParentProcess()->unLockThreadMutex();
  }
  return;
}

int32 Syscall::pthread_setcancelstate(int state, int *oldstate)
{
  if((state != 1) && (state != 0))
  {
    debug(X_USERTHREAD, "got a wrong arg as cancelstate!\n");
    return -1;
  }
  UserThread* callingthread = (UserThread*)currentThread;
  callingthread->lockFlagMutex();
  *oldstate = (int) !callingthread->getflags()->cancelable;
  callingthread->setCancelState(state);
  callingthread->unlockFlagMutex();
  return 0;
};
int32 Syscall::pthread_setcanceltype(int type, int *oldtype){
   if((type != 1) && (type != 0))
  {
    debug(X_USERTHREAD, "got a wrong arg as canceltype!\n");
    return -1;
  }
  UserThread* callingthread = (UserThread*)currentThread;
  callingthread->lockFlagMutex();
  *oldtype = (int) !callingthread->getflags()->cancelable;
  callingthread->setCancelState(type);
  callingthread->unlockFlagMutex();
  return 0;
}


      //  EDEADLK
      //         A  deadlock  was  detected (e.g., two threads tried to join with
      //         each other); or thread specifies the calling thread.

      //  EINVAL thread is not a joinable thread.

      //  EINVAL Another thread is already waiting to join with this thread.

      //  ESRCH  No thread with the ID thread could be found.

size_t Syscall::pthread_join(size_t thread, void** value_ptr)
{
  UserThread* callingthread = (UserThread*)currentThread;
  void* retval;

  //catch edgecases
  if(thread == callingthread->getTID())
    return -1;
  //debug(X_USERTHREAD, "[%ld]trying to join [%ld]; before threadlock\n", callingthread->getTID(), thread);
  callingthread->getParentProcess()->lockThreadMutex();
  //debug(X_USERTHREAD, "[%ld]trying to join [%ld]; afeter threadlock\n", callingthread->getTID(), thread);

  debug(X_USERTHREAD, "[%ld]trying to join [%ld]; before retvallock\n", callingthread->getTID(), thread);
  if (!callingthread->getParentProcess()->getRetVal(thread, &retval) && !callingthread->getParentProcess()->findInThreadList(thread))
  {
    debug(X_USERTHREAD, "[%ld]trying to join [%ld]; after retvallock AND thread didnt exist\n", callingthread->getTID(), thread);
    callingthread->getParentProcess()->unLockThreadMutex();
    return -1;
  }
  debug(X_USERTHREAD, "[%ld]trying to join [%ld]; after retvallock\n", callingthread->getTID(), thread);
  while (!callingthread->getParentProcess()->getRetVal(thread, &retval))
  {
    debug(X_USERTHREAD, "[%ld] didn't find retval yet!\n", callingthread->getTID());
    callingthread->getParentProcess()->unLockThreadMutex();
    Scheduler::instance()->yield();
    callingthread->getParentProcess()->lockThreadMutex();
  }
  callingthread->getParentProcess()->unLockThreadMutex();
  *value_ptr = retval;
  debug(X_USERTHREAD, "[%ld]MANAGED to join [%ld]\n", callingthread->getTID(), thread);
  return 0;
}

int32 Syscall::pthread_cancel(size_t thread)
{
  //TODO:
  //write easy implementation for kernel semaphores
  //post on that sem whenever syscall entry
  //find out which case it is and do the apropriate thing :D

  UserThread* current = callingThread;
  current->getParentProcess()->lockThreadMutex();
  UserThread* cancel_victim;
  if(! (cancel_victim = (UserThread*) current->getParentProcess()->findInThreadList(thread)))
  {
    current->getParentProcess()->unLockThreadMutex();
    return -1;
  }
  cancel_victim->lockFlagMutex();
  const Threadflags* its_flags = cancel_victim->getflags();
  
  if (its_flags->cancelable && !its_flags->deferred) //queue cancellation request
  {
    if(!current->getParentProcess()->addToRetvalList(cancel_victim->getTID(), PTHREAD_CANCELED))
    {
      debug(USERPROCESS, "Userproc retval already in list: This should already have been thrown!\n");
    }
    debug(X_USERTHREAD, "Thread [%ld] could be cancelled right away and is now killed!\n", cancel_victim->getTID());
    
    cancel_victim->unlockFlagMutex();
    current->getParentProcess()->unLockThreadMutex();
    cancel_victim->kill();
    return 0;
  }
  cancel_victim->unlockFlagMutex();
  current->getParentProcess()->unLockThreadMutex();
  cancel_victim->sendCancelRequest();
  return 0;
}

