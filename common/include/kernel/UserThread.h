#pragma once

#include "Thread.h"
#include "Mutex.h"
#include "Condition.h"
#include "UserProcess.h"

class UserProcess;

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
    
    ~UserThread();

    /**
     * only asserts if called. 
     * "object of abstract class type "UserThread" is not allowed:C/C++(322)
     * UserProcess.cpp(56, 34): function "UserThread::Run" is a pure virtual function"
     */
    void Run() override { assert(false && "UserThread::Run() was called...\n"); }

    // tells if thread is the last thread of its process
    bool isLast() { return last_; }
    // return process of thread
    UserProcess* getParentProcess() { return parent_process_; }
    
    //checks for stack over/underflows
    bool isUserStackCanaryOK();

    // setters
    void setLast() { last_ = true; }
  private:

    size_t vpns_for_userstack_[USERSTACK_SIZE];
    size_t ppns_for_userstack_[USERSTACK_SIZE];
    // the process that contains this thread
    UserProcess* parent_process_;

    // safe stack start + end ppn
    size_t* userstack_start_;
    size_t* userstack_end_;
    


    // only true if removeFromThreadList() detects last thread
    bool last_ = false; 
};

