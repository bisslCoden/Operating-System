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

    size_t getPID(){ return pid_; }
    Loader* getLoader() { return loader_; }

  private:
    // the process ID
    size_t const pid_;
    // the process' fd. see "FileDescriptor.h"
    ssize_t const fd_;
    // i guess this is needed
    FileSystemInfo* const fs_info_;
    // loader loads programms
    Loader* loader_;
    // the directory
    FileSystemInfo* working_dir_;
    // tells us which terminal is used. i think not relevant for now
    Terminal* my_terminal_;
    // name of the process.
    ustl::string name_;

    /* 
    IMPORTANT: this list gets an insert in the UserProcess constructor after constructing the first_thread
    */
    // a list containing TIDs and their appropriate UserThread*
    ustl::map<size_t, UserThread*> list_of_threads_;
    Mutex list_of_threads_lock_;
};

