#pragma once


#include <types.h>
#include "Thread.h"
#include "Scheduler.h"
#include "kprintf.h"

class Syscall
{
  public:
  static size_t syscallException(size_t syscall_number, size_t arg1, size_t arg2, size_t arg3, size_t arg4, size_t arg5);

  static void exit(size_t exit_code);
  static void outline(size_t port, pointer text);

  static size_t write(size_t fd, pointer buffer, size_t size);
  static size_t read(size_t fd, pointer buffer, size_t count);
  static size_t close(size_t fd);
  static size_t open(size_t path, size_t flags);
  static void pseudols(const char *pathname, char *buffer, size_t size);

  static size_t createprocess(size_t path, size_t sleep);
  static void trace();
  static int fork();
  static size_t wait_pid(size_t arg1, size_t* arg2, size_t arg3);
  static int get_pid();

  // pthreads
  static size_t pthread_create(size_t thread, size_t attr, size_t start_routine, size_t arg, size_t wrapper);
  static void pthread_exit(void* value);
  static size_t pthread_join(size_t thread, void** value_ptr);
  // cancel not 100% working
  static int32 pthread_cancel(size_t thread);
  static int32 pthread_setcancelstate(int32 state, int32* oldstate);
  static int32 pthread_setcanceltype(int32 type, int32* oldtype);

};

