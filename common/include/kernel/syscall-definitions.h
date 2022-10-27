#pragma once

#define fd_stdin 0
#define fd_stdout 1
#define fd_stderr 2

#define sc_exit 1
#define sc_read 3
#define sc_write 4
#define sc_open 5
#define sc_close 6
#define sc_lseek 19
#define sc_pseudols 43
#define sc_outline 105
#define sc_sched_yield 158
#define sc_createprocess 191
#define sc_trace 252

#define sc_pthread_create 400
#define sc_pthread_cancel 401
#define sc_pthread_exit 402
#define sc_pthread_join 403
#define sc_pthread_setcancelstate 404
#define sc_pthread_setcanceltype 405

#define sc_fork 501
#define sc_execv 502