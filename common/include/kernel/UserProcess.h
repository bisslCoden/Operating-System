#pragma once

#include "Thread.h"
#include "umap.h"
#include "UserThread.h"

class UserThread;

class UserProcess
{
  public:
    /**
     * Constructor
     * @param minixfs_filename filename of the file in minixfs to execute
     * @param fs_info filesysteminfo-object to be used
     * @param terminal_number the terminal to run in (default 0)
     *
     */
    UserProcess(ustl::string minixfs_filename, FileSystemInfo *fs_info, uint32 terminal_number = 0);
    /**
     * Constructor
     * @param parent parent process to fork
     * @param pid the id of the parent
     *
     */
    UserProcess(UserProcess* parent, size_t pid);

     /**
     * CopyConstructor
     * @param parent_process
     *
     */

    UserProcess(const UserProcess& parent_process);


    ~UserProcess();



    /**
     * @brief safely adds a userthread to threads
     * 
     * @param thread the userthread
     * @return true if successful
     * @return false if already found in list
     */
    bool addToThreadList(UserThread* thread);

    /**
     * @brief safely removes userthread from threads_
     * 
     * @param thread the userthread
     * @return true if found in list
     * @return false if not found
     */
    bool removeFromThreadList(UserThread* thread);

    Thread* findInThreadList(size_t tid);
    
    bool addToRetvalList(size_t tid, void* value);

    size_t getPID(){ return pid_; }
    Loader* getLoader() { return loader_; }
    FileSystemInfo* getWorkingDir() { return working_dir_; }
    ustl::string getName() { return name_; }
    /**
     * @brief returns threads_.size() but threadsafe
     * 
     * @return size_t the numer of threads
     */
    size_t getNrOfThreads();

    void lockThreadMutex(){threads_lock_.acquire();}
    void unLockThreadMutex(){threads_lock_.release();}


    /**
     * @brief Create a New Thread object (pthread_create)
     * 
     * @param start_routine which thread should execute
     * @return size_t thread ID
     */
    size_t createNewThread(size_t start_routine, size_t args, size_t wrapper);

    /**
     * @brief pushes all threads of process onto list and destroys (currentThread last)
     * 
     * @param exit_code 
     * @return size_t 
     */
    void exit(size_t exit_code);

    void killThread(UserThread* thread);

    bool getRetVal(size_t tid, void** value);

  private:
    // the process ID
    size_t const pid_;

    // the parent  process ID
    //size_t const ppid_;

    // the process' fd. see "FileDescriptor.h"
    ssize_t const fd_;

    // information about the program. path...
    FileSystemInfo* const fs_info_;

    // loader loads the program
    Loader* loader_;

    // the directory
    FileSystemInfo* working_dir_;

    // tells us which terminal is used. i think not relevant for now
    Terminal* my_terminal_;

    // name of the process.
    ustl::string name_;

    // a list containing TIDs and their appropriate UserThread*
    ustl::map<size_t, UserThread*> threads_;
    Mutex threads_lock_;
    ustl::map<size_t, void*> returnvalues_;
    Mutex returnvalue_lock_;

    // map with tid + return value for join
};

