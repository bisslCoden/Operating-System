#pragma once

#include "Condition.h"
#include "Mutex.h"
#include "Thread.h"
#include "UserProcess.h"

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
     * Makes a process wait till change of state
     */
    size_t waitPid();

    /**
     * @brief The instance of the ProcessRegistry. inherits from Thread
     *
     * @return ProcessRegistry* to access membermethods
     */
    static ProcessRegistry* instance();
    void createProcess(const char* path);

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

    //prevous ID creation for fork for fork
    //size_t process_pids_ = 0;
    //ustl::map<long int, UserProcess*> processes_running_;

    // Mutexes

    // ensures unique IDs for TID AND PID
    size_t next_id_ = 1;
    Mutex next_id_lock_;
    // keeping track of processes alive
    ustl::map<size_t, UserProcess*> list_of_processes_;
    Mutex list_of_processes_lock_;
};

