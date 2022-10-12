#pragma once

#include "Thread.h"
#include "Mutex.h"
#include "Condition.h"
#include "UserProcess.h"

class UserProcess;

#define THREADSETUP_FIRST 0
#define THREADSETUP_PTHREAD 1
#define THREADSETUP_FORK 2

class UserThread : public Thread
{
  public:
    /**
     * Constructor for first_thread
     * @param minixfs_filename filename of the file in minixfs to execute
     * @param fs_info filesysteminfo-object to be used
     * @param terminal_number the terminal to run in (default 0)
     *
     */
    UserThread(UserProcess* parent_process, FileSystemInfo* working_dir, ustl::string name, uint32 terminal_number);
    
    UserThread(size_t start_routine, uint32_t terminal_number = 0);

    UserThread(UserProcess* parent);

    ~UserThread();

    /**
     * only asserts if called. 
     * "object of abstract class type "UserThread" is not allowed:C/C++(322)
     * UserProcess.cpp(56, 34): function "UserThread::Run" is a pure virtual function"
     */
    void Run() override { assert(false && "UserThread::Run() was called...\n"); }

    /**
     * @brief sets up stack for a thread. 
     * 
     * @param first_thread set to #define "THREADSETUP_XXX"
     * @return true stack set successfully
     * @return false stack not setup
     */
    bool setupStack(int first_thread);

    void* getUserstackStart() { return (void*)userstack_start_; }

    // tells if thread is the last thread of its process
    bool isLast() { return last_; }
    // return process of thread
    UserProcess* getParentProcess() { return parent_process_; }
    
    //checks for stack over/underflows
    bool isUserStackCanaryOK();

    // setters
    void setLast() { last_ = true; }

  private:
    // the process that contains this thread
    UserProcess* parent_process_;

    // safe stack start + end ppn
    size_t userstack_start_;
    size_t userstack_end_;
    
    // only true if removeFromThreadList() detects last thread
    bool last_ = false; 
};

