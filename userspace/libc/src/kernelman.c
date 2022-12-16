#include "syscall.h"
#include "../../../common/include/kernel/syscall-definitions.h"

size_t allocPhysicalPage()
{
  return __syscall(sc_test_allocppn, 0x0, 0x0, 0x0, 0x0, 0x0);
}

size_t freePhysicalPage(size_t ppn)
{
  return __syscall(sc_test_freeppn, ppn, 0x0, 0x0, 0x0, 0x0);
}