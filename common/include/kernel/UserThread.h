#pragma once

#include "Thread.h"
#include "Mutex.h"
#include "Condition.h"
#include "UserProcess.h"
#include "uatomic.h"

#define PTHREAD_CANCELED ((void *) -1)
#define STACK_SIZE_MAX_IN_MB 8
#define currentUserThread ((UserThread*)currentThread)

class UserProcess;

enum cancelstate {
    PTHREAD_CANCEL_ENABLE,
    PTHREAD_CANCEL_DISABLE
};

enum canceltype {
    PTHREAD_CANCEL_DEFERRED,
    PTHREAD_CANCEL_ASYNCHRONOUS
};

enum joinbale {
    PTHREAD_CREATE_JOINABLE, 
    PTHREAD_CREATE_DETACHED
};

typedef struct Threadflags
{
  int cancelable = PTHREAD_CANCEL_ENABLE;
  int deferred = PTHREAD_CANCEL_DEFERRED;
  //TODO joinable
  bool cancelreq = false;
  int joinable = PTHREAD_CREATE_JOINABLE;
  ustl::atomic_flag kcancelreq;
  ustl::atomic_flag knotcancelable;
  ustl::atomic_flag kasynchronous;
}Threadflags;

typedef struct StackInfo
{
  size_t userstack_start_ = 0;
  size_t userstack_end_ = 0;
  size_t page_offset_ = 0;
  size_t* UserMutex;
} StackInfo;


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
    UserThread(UserProcess* process_, FileSystemInfo* working_dir, ustl::string name, uint32 terminal_number, size_t page_offset);
    
    /**
     * @brief Construct a new User Thread object for pthread_create()
     * 
     * @param wrapper the wrapper for implicit pthread_exit() call
     * @param page_offset offset for stack location
     * @param terminal_number the terminal to run in (default 0)
     */
    UserThread(size_t wrapper, size_t page_offset, uint32_t terminal_number = 0);

    /**
     * @brief Construct a new User Thread object for fork()
     * 
     * @param child the UserProcess of this new thread
     * @param parent_thread the thread of the parent_thread that called fork()
     */
    UserThread(UserProcess* child, UserThread* parent_thread);

    /**
     * @brief Destroy the User Thread object and check if(isLast()) { destroy parent_; } 
     * 
     */
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

    /**
     * @brief UserThread-part of constructor. sets few members, creates stack in new archemory and 
     * copies arguments from userspace via ident mapping to location pointed to by rsi and rdi
     * 
     * @param argv the userspace arggument vector
     * @param argc the userspace argument count
     * @return error return values
     */
    int execv(char* const argv[], size_t argc);
    // no-args here
    int execv();

    size_t getLastStart() {return last_start_; }

    void setLastStart(size_t time) {last_start_ = time;}

    size_t getTimeToWake() {return time_to_wake_; }

    void setTimeToWake(size_t time) {time_to_wake_ = time;}


    /**
     * @brief join functions: locks and setters for the join mechanics. setJoiner needs to be locked!
     * 
     */
    void setJoinState(int state){myflags_.joinable = state;}
    int getJoinState(){return myflags_.joinable;}

    void lockJoin(){condition_mutex_.acquire();}
    void setJoiner(int32 tid){join_waiter_ = tid;}
    void unlockJoin(){condition_mutex_.release();}
    void waitJoin(bool reacquire){join_cond_.wait(reacquire);}
    void signalJoin(){join_cond_.signal();}
    bool checkFlagLock(Thread* caller){return flag_mutex_.isHeldBy(caller);}

    size_t getPageOffset(){return mystack_.page_offset_;}


    StackInfo getStackInfo() { return mystack_; }

    // tells if thread is the last thread of its process
    // return process of thread
    UserProcess* getParentProcess() { return process_; }

    bool schedulable() override;


    void lockFlagMutex(){ flag_mutex_.acquire();}
    void unlockFlagMutex(){ flag_mutex_.release();}

    void setCancelState(int state);
    void setCancelType(int type);
    void sendCancelRequest();
    void setLast(){last_ = true;}    
    // getters
    Threadflags*  getflags()          { return &myflags_;}     //lock before!
    int32         getJoiner()         { return join_waiter_;}  //lock before!
    void*         getUserstackStart() { return (void*)mystack_.userstack_start_; }
    UserProcess*  getProcess()        { return process_; }
    bool          isLast()            { return last_; }        // tells if thread is the last thread of its process (important: on thread destuction, the process is destroyed if set!)
  private:
    // the process that contains this thread
    UserProcess* process_;

    int32 join_waiter_ = -1;
    Mutex flag_mutex_;
    Mutex condition_mutex_;
    Condition join_cond_;

    /**
      int cancelable  = PTHREAD_CANCEL_ENABLE or PTHREAD_CANCEL_DISABLE
      int deferred    = PTHREAD_CANCEL_DEFERRED or PTHREAD_CANCEL_ASYNCHRONOUS
      int joinable;
      bool cancelreq = false;
      ustl::atomic_flag kcancelreq;
      ustl::atomic_flag knotcancelable;
      ustl::atomic_flag kasynchronous;
     * 
     */
    Threadflags myflags_;
    /** userstack_start_
        size_t userstack_end_
        size_t page_offset_
     */
    StackInfo mystack_;
    
    // only true if removeFromThreadList() detects last thread to delete process
    bool last_ = false; 

    //clock
    size_t last_start_;

    //sleep
    size_t time_to_wake_ = 0;
};

