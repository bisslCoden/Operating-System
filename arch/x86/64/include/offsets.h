#pragma once

#include "types.h"
#include "paging-definitions.h"

/**
 * These are the basic offsets for our memory layout
 */

/**
 * this is the difference between link and load base
 */
#define PHYSICAL_TO_VIRTUAL_OFFSET 0xFFFFFFFF80000000ULL

/**
 * returns the physical address of a virtual address by using the offset
 */
#define VIRTUAL_TO_PHYSICAL_BOOT(x) ((void*)(~PHYSICAL_TO_VIRTUAL_OFFSET & ((uint64)x)))

/**
 * Use only the lower canonical half for userspace
 *///
#define USER_BREAK 0x0000800000000000ULL
#define END_OF_STACKS 0x0000700000000000ULL

#define END_OF_HEAP 0x0000550000000000ULL
#define BEGIN_HEAP_AT_LEAST 0x0000400000000000ULL
#define NO_LOCK_KS 0x12246079


#define SLEEPING_KS 0x46334234
#define AWAKE_KS 0x54321432

#define MAX_STACKS (USER_BREAK - END_OF_STACKS) / (PAGE_SIZE * (STACK_SIZE_IN_PAGES + 2))
/**
 * End of the non-canonical space, start of kernel space
 */
#define KERNEL_START 0xffff800000000000ULL
