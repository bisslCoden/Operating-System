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
    
    bool addToRetvalList(size_t tid, size_t value);

    size_t getPID(){ return pid_; }
    Loader* getLoader() { return loader_; }
    
    /**
     * @brief Get the nr of threads in list_of_threads_
     * IMPORTANT: NOT LOCKED, USE list_of_threads_lock_ AROUND FUNCTION CALL
     * 
     * @return size_t the numer of threads
     */
    size_t getNrOfThreads() { return threads_.size(); }

    // called for pthread_create(). returns tid. obviously missing arguments.
    size_t createNewThread();

    bool getRetVal(size_t tid, size_t* value);

  private:
    // the process ID
    size_t const pid_;

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
    ustl::map<size_t, size_t> returnvalues_;
    Mutex returnvalue_lock_;

    // map with tid + return value for join
};

