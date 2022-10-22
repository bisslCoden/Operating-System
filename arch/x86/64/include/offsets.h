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
 */
#define USER_BREAK 0x0000800000000000ULL
#define END_OF_STACKS 0x0000650000000000ULL

#define MAX_STACKS (USER_BREAK - END_OF_STACKS) / (PAGE_SIZE * 512ULL * 512ULL * STACK_SIZE_MAX_IN_MB)
/**
 * End of the non-canonical space, start of kernel space
 */
#define KERNEL_START 0xffff800000000000ULL
