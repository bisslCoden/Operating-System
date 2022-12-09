#include "getpid.h"
#include "syscall.h"
#include "../../../common/include/kernel/syscall-definitions.h"

int getpid()
{
  return __syscall(sc_getpid, 0x00, 0x00, 0x00, 0x00, 0x00);
}
