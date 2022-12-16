#include "syscall.h"
#include "../../../common/include/kernel/syscall-definitions.h"

// allocate your own physical page
size_t allocPhysicalPage();

// free your allocated page
size_t freePhysicalPage(size_t ppn);
