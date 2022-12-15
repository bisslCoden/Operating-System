#include "unistd.h"
#include "../../../common/include/kernel/syscall-definitions.h"
#include "sys/syscall.h"
#include "stdio.h"
/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int brk(void *end_data_segment)
{
  return (int) __syscall(sc_brk, (size_t) end_data_segment, 0x00, 0x00, 0x00, 0x00);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
void* sbrk(intptr_t increment)
{
  void* retu = (void*) __syscall(sc_sbrk, (size_t) increment, 0x00, 0x00, 0x00, 0x00);
  //printf("[sbrk] got %p from syscall\n", retu);
  return retu;
}


/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
unsigned int sleep(unsigned int seconds)
{
  return __syscall(sc_sleep, seconds, 0x00, 0x00, 0x00, 0x00);
}


/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int ftruncate(int fildes, off_t length)
{
    return -1;
}
