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
     * @brief The instance of the ProcessRegistry. inherits from Thread
     *
     * @return ProcessRegistry* to access membermethods
     */
    static ProcessRegistry* instance();
    void createProcess(const char* path);

    int execvProcess(const char* path, char *const argv[]);


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
};

