#pragma once

#include "Thread.h"
#include "Mutex.h"
#include "Condition.h"
#include "UserProcess.h"

class UserProcess;


#define STACK_SIZE_MAX_IN_MB 8

typedef struct Threadflags
{
  bool cancelable = true;
  bool deferred = true;
  bool joinable = true;
  bool cancelreq = false;
}Threadflags;


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
    
    UserThread(size_t wrapper, uint32_t terminal_number = 0);

    ~UserThread();

    /**
     * only asserts if called. 
     * "object of abstract class type "UserThread" is not allowed:C/C++(322)
     * UserProcess.cpp(56, 34): function "UserThread::Run" is a pure virtual function"
     */
    void Run() override { assert(false && "UserThread::Run() was called...\n"); }

    /**
     * @brief sets up rsp, allocates ppn, finds vpn,
     * 
     * @return true stack set successfully
     * @return false stack not setup.
     */
    bool setupStack();

    void* getUserstackStart() { return (void*)userstack_start_; }

    // tells if thread is the last thread of its process
    bool isLast() { return last_; }
    // return process of thread
    UserProcess* getParentProcess() { return parent_process_; }

    void lockFlagMutex(){ flag_mutex_.acquire();}
    void unlockFlagMutex(){ flag_mutex_.release();}

    void setCancelState(bool notcancelable){ myflags_.cancelable = !notcancelable; }
    void setCancelType(bool asynchronous) { myflags_.deferred = !asynchronous; }

    // setters
    void setLast() { last_ = true; }

    void sendCancelRequest(){ myflags_.cancelreq = true; }

    const Threadflags* getflags(){return &myflags_;}
  private:
    // the process that contains this thread
    UserProcess* parent_process_;

    // safe stack start + end ppn
    size_t userstack_start_ = 0;
    size_t userstack_end_ = 0;

    Mutex flag_mutex_;

    Threadflags myflags_;

    
    // only true if removeFromThreadList() detects last thread
    bool last_ = false; 
};

