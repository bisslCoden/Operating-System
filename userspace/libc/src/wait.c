#include "wait.h"
#include "../../../common/include/kernel/syscall-definitions.h"
#include "sys/syscall.h"

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
pid_t waitpid(pid_t pid, int *status, int options)
{
  return __syscall(sc_waitpid, pid, (size_t) status, options, 0x00, 0x00);
}


