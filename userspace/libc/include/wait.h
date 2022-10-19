#pragma once

#include "types.h"
#include "../../../common/include/kernel/syscall-definitions.h"
#include "sys/syscall.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WEXITED 4

//pid typedefs
#ifndef PID_T_DEFINED
#define PID_T_DEFINED
typedef ssize_t pid_t;
#endif // PID_T_DEFINED

extern pid_t waitpid(pid_t pid, int *status, int options);


#ifdef __cplusplus
}
#endif



