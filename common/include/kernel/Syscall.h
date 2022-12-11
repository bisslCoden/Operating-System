#pragma once


#include <types.h>
#include "Thread.h"
#include "Scheduler.h"
#include "kprintf.h"

#define EXECV_MAX_PATH_LEN 100


class Syscall
{
  public:
  static size_t syscallException(size_t syscall_number, size_t arg1, size_t arg2, size_t arg3, size_t arg4, size_t arg5);

  // -----------------------------------------------------------------------------------------------
  //                    already implemented stuff (usually not much changed)
  // -----------------------------------------------------------------------------------------------

  // calls writeLine2Bochs((const char*) text);
  static void outline(size_t port, pointer text);
  // prints to shell
  static size_t write(size_t fd, pointer buffer, size_t size);
  // reads user character
  static size_t read(size_t fd, pointer buffer, size_t count);
  // calls VfsSyscall::close()
  static size_t close(size_t fd);
  // calls VfsSyscall::open()
  static size_t open(size_t path, size_t flags);
  // if you enter "ls" in console..
  static void pseudols(const char *pathname, char *buffer, size_t size);
  // calls Thread::printBacktrace() 
  static void trace();

  /**
   * @brief This syscall creates a process (called in shell.c) and its first thread.
   * 
   * 
   * @param path the path to the programm
   * @param sleep not really implemented. not anywhere used afaik.
   * @return size_t 
   */
  static size_t createprocess(size_t path, size_t sleep);

  /**
   * @brief This syscall exits a process. Internally, UserProcess::exit() is called which
   * will set all other threads to PTHREAD_CANCEL_ENABLE and PTHREAD_CANCEL_ASYNCHRONOUS and then
   * a cancellation request is sent.
   * Lastly, pthread exit is called for the currentThread.
   * 
   * @param exit_code in "normal" process termination: return value of the main(). otherwise some 
   * random error code tbh.
   */
  static void exit(size_t exit_code);




  // -----------------------------------------------------------------------------------------------
  //                                          fork/exec
  // -----------------------------------------------------------------------------------------------

  /**
   * @brief creates a new UserProcess and copies a lot of fork-specific data to
   * child process. Will also create new Thread. The new Loader will create a new
   * Loader::arch_memory_ that will be copied after getting a PageFault for COW when needed.
   * 
   * @return int returns the PID of the child for the parent process but returns 
   * 0 to the child.
   */
  static int fork();

  /**
   * (1) check isExecPathValid(path)
   * (2) if (argv != NULL) execvProcess(path, argv);
   *                  else execvProcess(path);
   * 
   * @param user_path the path to the binary. must end with '\0'
   * @param user_argv an array of c-strings
   * @return on success this should not return. on fail -1
   */
  static int execv(const char * user_path, char *const user_argv[]);
  /**
   * @brief checks: 
   * path may not be 0
   * may not outside of userspace 
   * and pointers to characters must be in userspace! 
   * may not have more than EXEC_MAX_PATH_LEN
   */
  static bool isExecPathValid(const char* path);





  // -----------------------------------------------------------------------------------------------
  //                                   wait pid, sleep, clock
  // -----------------------------------------------------------------------------------------------

  /**
   * @brief calls the waitPid function
   * 
   * @param arg1 important
   * @param arg2 not important
   * @param arg3 not important
   * @return  waitPid function 
   */
  static size_t wait_pid(size_t arg1, size_t* arg2, size_t arg3);
  /**
   * @brief Get the pid object
   * 
   * @return int 
   */
  static int get_pid();
  /**
   * @brief Takes the number of clock cycles from now and multiplies by 10 bc of no floating numbers allowed
   * Computes the time to wake up: 1st: the given argument(seconds) is multiplied by
   * 2nd: 182 bc 18.2 * ~54 ms = 1 s(thats the reason for multiplying rdtsc_now by 10)
   * 3rd: average of cycles between 2 ticks(avg is computed every 10 ticks new)
   * the part for not scheduling is in schedulable() in UserThread.cpp(else if(getTimeToWake() > (Scheduler::instance()->getRDTSC() * 10)))
   * at the end yielded

  * @param seconds number of seconds to wait
  * @return 0
  */
  static unsigned int sleep(unsigned int seconds);
  /**
   * @brief Avg divided by 54925(~54 ms(even though it looks more like 55 ms, but daniel said ~54 if not mistaken))
   * so avg by that is the number of cpu cycles per micro sec.
   * getDuaration -> duaration is a member variable of UserProcess that increases before scheduling.
   * we get number of microsec by dividing duaration by cyc_per_microsec -> man pages -> (return of clock / 10⁶) should be seconds
   * time_to_add -> time that passed between the last schedule till now
   * @return the time of a process, which is the sum of the time were threads of the process have been scheduled
   */
  static size_t clock();
  /**
  * reads the Time Stamp Counter Register - number of cycles passed since last start of the system
  */
  static size_t getRDTSC();


  static int brk(size_t end_data_segment);

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
  static size_t sbrk(int increment);






  // -----------------------------------------------------------------------------------------------
  //                                             pthreads
  // -----------------------------------------------------------------------------------------------

  /**
   * @brief creates new thread that runs until:
   * (1) pthread_exit() is called explicitely
   * (2) pthread_exit() is called implicitely 
   * (3) it is cancelled: see pthread_cancel()
   * (4) the process terminates via Syscall::exit()
   * 
   * @param thread a pointer to where the thread id will be stored
   * @param attr initialized with pthread_attr_init(), if NULL -> default values:
   *             (joinstate = PTHREAD_CREATE_JOINABLE,PTHREAD_CANCEL_ENABLE, PTHREAD_CANCEL_DEFERRED)
   * @param start_routine the pointer to the start routine that the thread will start to execute.
   * @param arg the arguments for the start routine.
   * @param wrapper the wrapper that performs the implicite pthread_exit() call.
   * @return size_t -1 on error, 0 on success
   */
  static int pthread_create(size_t thread, size_t attr, size_t start_routine, size_t arg, size_t wrapper);

  /**
   * @brief terminates the currentThread and returns a value to retval that can be read
   * by another thread via pthread_join() - if the thread is joinable.
   * implicit pthread_exit(): call pthread_exit when a pthread_create()-thread terminates. 
   * 
   * 
   * @param value ptr to userspace where the exitvalue is saved
   */
  static void   pthread_exit(void* value);

    /**
   * @brief waits to join the thread and save returnvalue in value_ptr if it is not null.
   * Also has advanced "deadlock" detection looking for circles in the joins or threads which 
   * try to join each other
   * 
   * @param thread The TID of the thread which shall be joined
   * @param value_ptr pointer to userspace where the returnvalue (specified by pthread_exit and stored
   * in retval list of process) is written into
   * 
   * @return -1 if something went wrong (invalid tid, deadlock, thread joining itself...) 0 else 
   *  
   */
  static size_t pthread_join(size_t thread, void** value_ptr);
  static int    pthread_detach(size_t thread);
  static size_t pthread_self();

  // cancel 100% working
  static int32 pthread_cancel(size_t thread);
  static int32 pthread_setcancelstate(int32 state, int32* oldstate);
  static int32 pthread_setcanceltype(int32 type, int32* oldtype);
  static int32 pthread_attr_init(size_t** stackaddr, size_t* stacksize);





  // lonely kernel semaphores
  static void kernelsem_wait();
  static void kernelsem_post();
};

