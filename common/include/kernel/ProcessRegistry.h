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
     * Tells us how many processes are running
     */
    size_t processCount();

    /**
     * @brief The instance of the ProcessRegistry. Is a Thread.
     *
     * @return ProcessRegistry* to access methods
     */
    static ProcessRegistry* instance();

    /**
     * Tells us that a userprocess is being created due to a fork or something similar
     */
    void processStart();

    /**
     * @brief create a process + its first thread.
     * gets fs_info, creates UserProcess, check if success (returnto). add to list_of_processes_
     * 
     * @param path the path to the binary.
     */
    void createProcess(const char* path);

    /**
     * Tells us that a userprocess is being destroyed
     */
    void processExit(UserProcess* user_proc);


    // -------------------------------------------------------------------------
    //                              OUR STUFF
    // -------------------------------------------------------------------------

    /**
     * creates an unique ID for every process OR thread ID
     *
     * @return size_t the ID
     */
    size_t createID();


    /**
     * Creates the child process and returns the pid
     */
    size_t processFork();

    /**
     * @brief handles argument checking
     * will call UserProcess::execv(path, kernel_argv, argc)
     * kernel_argv is the kernel-space argument list that will be free'd later.
     * 
     * @param path the path to the programm, already copied to kernel-space
     * @param argv the args of the user.
     * @return int return value, -1 on fail, shouldn't return on success
     */
    int execv(const char* path, char *const argv[]);
    /**
     * @brief checks exec args
     * @return int that holds argc, -1 on error
     */
    int areExecArgsValid(char* const argv[]);

    void addProcToList(UserProcess* new_proc);

    //just write 0 if you dont care about vpn
    void lockMultArchmem(ustl::map<UserProcess*, size_t> procs);
    void unlockMultArchmem(ustl::map<UserProcess*, size_t> procs);


    /** @brief 1st argument is PID for process to wait to, other are not important, just for posix standard
     * tries to find the prcess in the list_of_processes, if not there -1 is returned
     * uses kernel semaphores for waiting
     * is freed after the process to be waited exits
     * 
     * @param arg1 PID for process to wait to,
     * @param arg2 not important
     * @param arg3 not important
     * @return on success id of process who terminated, else -1
     */ 
    size_t waitPid(size_t arg1, size_t* arg2, size_t arg3, UserProcess* parent_process);

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

