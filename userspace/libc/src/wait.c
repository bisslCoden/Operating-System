#include "wait.h"
#include "../../../common/include/kernel/syscall-definitions.h"
#include "sys/syscall.h"
#include "pthread.h"

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
pid_t waitpid(pid_t pid, int *status, int options)
{
  if (checkAdress((void*) status, 1) != 0)
    return -1;
  
  return __syscall(sc_waitpid, pid, (size_t) status, options, 0x00, 0x00);
}


