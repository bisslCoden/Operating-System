#pragma once

#include "Thread.h"
#include "Mutex.h"
#include "Condition.h"
#include "UserProcess.h"

#define PTHREAD_CANCELED ((void *) -1)
#define STACK_SIZE_MAX_IN_MB 8
#define SAFE_RETVAL 675467

class UserProcess;

enum cancelstate {
    PTHREAD_CANCEL_ENABLE,
    PTHREAD_CANCEL_DISABLE
};

enum canceltype {
    PTHREAD_CANCEL_DEFERRED, 
    PTHREAD_CANCEL_ASYNCHRONOUS
};





typedef struct Threadflags
{
  int cancelable = PTHREAD_CANCEL_ENABLE;
  int deferred = PTHREAD_CANCEL_DEFERRED;
  //TODO joinable
  int joinable = true;
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

    void lockJoin(){condition_mutex_.acquire();}
    void setJoiner(int32 tid){join_waiter_ = tid;}
    void unlockJoin(){condition_mutex_.release();}
    void waitJoin(bool reacquire){join_cond_.wait(reacquire);}
    void signalJoin(){join_cond_.signal();}


    void* getUserstackStart() { return (void*)userstack_start_; }

    // tells if thread is the last thread of its process
    bool isLast() { return last_; }
    // return process of thread
    UserProcess* getParentProcess() { return parent_process_; }

    void lockFlagMutex(){ flag_mutex_.acquire();}
    void unlockFlagMutex(){ flag_mutex_.release();}

    void setCancelState(int state){ myflags_.cancelable = state; return;}
    void setCancelType(int type) { myflags_.deferred = type; return; }

    // setters
    void setLast() { last_ = true; }

    
    void sendCancelRequest(){ myflags_.cancelreq = true; }

    //lock before!
    Threadflags* getflags(){return &myflags_;}
    
    //lock before!
    int32 getJoiner(){return join_waiter_;}
  
  private:
    // the process that contains this thread
    UserProcess* parent_process_;

    // safe stack start + end ppn
    size_t userstack_start_ = 0;
    size_t userstack_end_ = 0;

    int32 join_waiter_ = -1;
    Mutex flag_mutex_;
    Mutex condition_mutex_;
    Condition join_cond_;

    Threadflags myflags_;
    
    // only true if removeFromThreadList() detects last thread
    bool last_ = false; 
};

