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
  /**
   * @brief This syscall exits a process. Internally, UserProcess::exit() is called which
   * will set all other threads to PTHREAD_CANCEL_ENABLE and PTHREAD_CANCEL_ASYNCHRONOUS and then
   * a cancellation request is sent.
   * Lastly, pthread exit is called for the currentThread.
   * 
   * @param exit_code the return value of the main() or another, 
   */
  static void exit(size_t exit_code);
  /**
   * @brief 
   * 
   * @param port 
   * @param text 
   */
  static void outline(size_t port, pointer text);
  /**
   * @brief 
   * 
   * @param fd 
   * @param buffer 
   * @param size 
   * @return size_t 
   */
  static size_t write(size_t fd, pointer buffer, size_t size);
  /**
   * @brief 
   * 
   * @param fd 
   * @param buffer 
   * @param count 
   * @return size_t 
   */
  static size_t read(size_t fd, pointer buffer, size_t count);
  /**
   * @brief 
   * 
   * @param fd 
   * @return size_t 
   */
  static size_t close(size_t fd);
  /**
   * @brief 
   * 
   * @param path 
   * @param flags 
   * @return size_t 
   */
  static size_t open(size_t path, size_t flags);
  /**
   * @brief 
   * 
   * @param pathname 
   * @param buffer 
   * @param size 
   */
  static void pseudols(const char *pathname, char *buffer, size_t size);
  // calls Thread::printBacktrace() 
  static void trace();
  /**
   * @brief 
   * 
   * @param path 
   * @param sleep 
   * @return size_t 
   */
  static size_t createprocess(size_t path, size_t sleep);

  // 
  static void kernelsem_wait();
  static void kernelsem_post();




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
   * @brief 
   * 
   * @param arg1 
   * @param arg2 
   * @param arg3 
   * @return size_t 
   */
  static size_t wait_pid(size_t arg1, size_t* arg2, size_t arg3);
  /**
   * @brief Get the pid object
   * 
   * @return int 
   */
  static int get_pid();
  /**
   * @brief 
   * 
   * @param seconds 
   * @return unsigned int 
   */
  static unsigned int sleep(unsigned int seconds);
  /**
   * @brief 
   * 
   * @return size_t 
   */
  static size_t clock();
  /**
   * @brief 
   * 
   * @return size_t 
   */
  static size_t getRDTSC();






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
   * @param thread a pointer 
   * @param attr initialized with pthread_attr_init(), if NULL -> default values:
   *             (joinstate = PTHREAD_CREATE_JOINABLE,PTHREAD_CANCEL_ENABLE, PTHREAD_CANCEL_DEFERRED)
   * @param start_routine the pointer to the start routine that the thread will start to execute.
   * @param arg the arguments for the start routine.
   * @param wrapper the wrapper that performs the implicite pthread_exit() call.
   * @return size_t -1 on error, 0 on success
   */
  static size_t pthread_create(size_t thread, size_t attr, size_t start_routine, size_t arg, size_t wrapper);
  static void   pthread_exit(void* value);
  static size_t pthread_join(size_t thread, void** value_ptr);
  static int    pthread_detach(size_t thread);
  static size_t pthread_self();

  // cancel 100% working
  static int32 pthread_cancel(size_t thread);
  static int32 pthread_setcancelstate(int32 state, int32* oldstate);
  static int32 pthread_setcanceltype(int32 type, int32* oldtype);
  static int32 pthread_attr_init(size_t** stackaddr, size_t* stacksize);
};

