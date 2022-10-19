#include "wait.h"

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
pid_t waitpid(pid_t pid, int *status, int options)
{
  return __syscall(sc_waitpid, 0x00, 0x00, 0x00, 0x00, 0x00);
}


