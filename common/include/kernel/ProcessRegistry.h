#pragma once

#include "Condition.h"
#include "Mutex.h"
#include "Thread.h"
#include "UserProcess.h"
#include "types.h"

#define EXECV_MAX_ARG_LEN 100

class ProcessRegistry : public Thread
{
  public:
    /**
     * Constructor
     * @param root_fs_info the FileSystemInfo
     * @param progs a string-array of the userprograms which should be executed
     */
    ProcessRegistry ( FileSystemInfo *root_fs_info, char const *progs[] );
    ~ProcessRegistry();

    /**
     * Mounts the Minix-Partition with user-programs and creates processes
     */
    virtual void Run();

    /**
     * Tells us that a userprocess is being destroyed
     */
    void processExit();

    /**
     * Tells us that a userprocess is being created due to a fork or something similar
     */
    void processStart();

    /**
     * Tells us how many processes are running
     */
    size_t processCount();

    /**
     * Creates the child process and returns the pid
     */
    size_t processFork();

    /**
     * Makes the process wait
     */
    size_t waitPid(size_t arg1, size_t* arg2, size_t arg3, UserProcess* parent_process);


    /**
     * @brief The instance of the ProcessRegistry. inherits from Thread
     *
     * @return ProcessRegistry* to access membermethods
     */
    static ProcessRegistry* instance();
    void createProcess(const char* path);

    /**
     * @brief handles argument checking
     * will call UserProcess::execv(path, argv, argc)
     * 
     * @param path the path to the programm 
     * @param argv the args as handeled by calling convention
     * @return int return value, -1 on fail, shouldn't return on success
     */
    int execv(const char* path, char *const argv[]);
    /**
     * @brief checks exec args
     * @return int that holds argc, -1 on error
     */
    int areExecArgsValid(char* const argv[]);

    void processExit(UserProcess* user_proc);

    /**
     * creates an unique ID for every process OR thread ID
     *
     * @return size_t the ID
     */
    size_t createID();

  private:
    char const **progs_;
    uint32 progs_running_;
    Mutex counter_lock_;
    Condition all_processes_killed_;
    static ProcessRegistry* instance_;

    // ensures unique IDs for TID AND PID
    size_t next_id_ = 1;
    // keeping track of processes alive
    ustl::map<size_t, UserProcess*> list_of_processes_;
    Mutex list_of_processes_lock_;
    Mutex wait_pid_lock_;
};

